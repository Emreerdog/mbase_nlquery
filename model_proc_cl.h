#ifndef MBASE_NLQ_MODEL_PROC_CL_H
#define MBASE_NLQ_MODEL_PROC_CL_H

#include <mbase/common.h>
#include <mbase/inference/inf_t2t_model.h>
#include <mbase/inference/inf_t2t_processor.h>
#include <mbase/inference/inf_t2t_client.h>
#include "global_state.h"

MBASE_BEGIN

I32 gProcCounter = 1;

class NlqClient : public InfClientTextToText {
public:
    bool is_processing() const
    {
        return isProcessing;
    }

    const mbase::string& get_generated_query() const
    {
        return generatedQuery;
    }

    GENERIC on_register(InfProcessorBase* out_processor) override
    {

    }

	GENERIC on_unregister(InfProcessorBase* out_processor) override
    {

    }

    GENERIC on_batch_processed(InfProcessorTextToText* out_processor, const U32& out_proc_batch_length, const bool& out_is_kv_locked) override
    {
        if(out_is_kv_locked)
        {
            isProcessing = false;
        }
        else
        {
            generatedQuery = "";
            isProcessing = true;
            mbase::decode_behavior_description dbd;
            dbd.mTokenAtMost = 1;
            dbd.mHaltDelay = 1;
            dbd.mHaltOnWrite = false;
            out_processor->next(dbd);
        }
    }

	GENERIC on_write(InfProcessorTextToText* out_processor, const inf_text_token_vector& out_token, bool out_is_finish) override
    {
        inf_token_description description;
        out_processor->token_to_description(out_token[0], description);
        if(!description.mIsSpecial)
        {
            generatedQuery += description.mTokenString;
        }

        if(!out_is_finish)
        {
            mbase::decode_behavior_description dbd;
            dbd.mTokenAtMost = 1;
            dbd.mHaltDelay = 1;
            dbd.mHaltOnWrite = false;
            out_processor->next(dbd);
        }
    }

	GENERIC on_finish(InfProcessorTextToText* out_processor, size_type out_total_token_size, InfProcessorTextToText::finish_state out_finish_state) override
    {
        isProcessing = false;
    }

    GENERIC query_hard_reset()
    {
        generatedQuery = "";
    }
private:
    mbase::string generatedQuery;
    bool isProcessing;
};

class NlqProcessor : public InfProcessorTextToText {
public:
    bool is_prompt_cached() const
    {
        return mIsPromptCached;
    }

    GENERIC on_initialize() override
    {
        mbase::inf_text_token_vector tokVec;
        mbase::GgufMetaConfigurator metaConfigurator(mbase::from_utf8(gModelPath));

        this->set_inference_client(&myClient);
        metaConfigurator.get_key("nlquery.tokens", tokVec);
        printf("INFO: Initializing (%d) processor...\n", gProcCounter);
        this->execute_input_sync(tokVec, true);
        printf("INFO: Processor (%d) initialized\n", gProcCounter);
        gProcCounter++;
        mIsPromptCached = true;
    }

	GENERIC on_destroy() override
    {

    }

    GENERIC on_initialize_fail(last_fail_code out_code) override
    {
        printf("ERR: Proc initialization is failed. Make sure you have enough memory for such operation\n");
        exit(1);
    }

private:
    NlqClient myClient;
    bool mIsPromptCached;
};

class NlqModel : public InfModelTextToText {
public:
    NlqModel(const I32 in_processor_count) : mProcessorCount(in_processor_count)
    {
    }

    GENERIC wait_prompt_caching()
    {
        while(1)
        {
            this->update();
            mbase::sleep(2);
            bool allPromptsCached = true;
            for(mbase::vector<NlqProcessor*>::iterator It = mAvailableProcessors.begin(); It != mAvailableProcessors.end(); It++)
            {
                NlqProcessor* tmpProcessor = *It;
                if(!tmpProcessor->is_prompt_cached())
                {
                    allPromptsCached = false;
                    break;
                }
            }

            if(allPromptsCached)
            {
                return;
            }
        }
    }

    GENERIC on_initialize_fail(init_fail_code out_fail_code) override
    {
        printf("ERR: Model initialization is failed. Make sure you have enough memory for such operation\n");
        exit(1);
    }

	GENERIC on_initialize() override
    {
        for(I32 i = 0; i < mProcessorCount; i++)
        {
            NlqProcessor* newProcessor = new NlqProcessor; // Leak is fine, program will 24/7 run anyways

            newProcessor->set_manual_caching(true, mbase::InfProcessorTextToText::cache_mode::KV_LOCK_MODE); // For system prompt caching

            this->register_context_process(
                newProcessor,
                4096,
                2048,
                16,
                16,
                true,
                {} // by giving empty set, applying greedy sampling
            );
            mAvailableProcessors.push_back(newProcessor);
        }
        printf("Initializing All processors\n");
        this->wait_prompt_caching();
        // Initialize all processors
    }

	GENERIC on_destroy() override
    {
        // This will never be called
    }

    bool acquire_processor(NlqProcessor*& out_processor)
    {
        mbase::lock_guard lockGuard(mProcDistributionSync);

        mbase::vector<NlqProcessor*>::iterator It = mAvailableProcessors.begin();
        if(It == mAvailableProcessors.end())
        {
            return false;
        }
        out_processor = *It;
        mAvailableProcessors.erase(It);
        return true;
    }

    GENERIC release_processor(NlqProcessor* in_processor)
    {
        mbase::lock_guard lockGuard(mProcDistributionSync);
    
        mAvailableProcessors.push_back(in_processor);
    }

private:
    mbase::mutex mProcDistributionSync;
    mbase::vector<NlqProcessor*> mAvailableProcessors;
    I32 mProcessorCount = 0;
};

MBASE_END

#endif // MBASE_NLQ_MODEL_PROC_CL_H