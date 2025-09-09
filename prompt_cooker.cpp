#include <mbase/inference/inf_device_desc.h>
#include <mbase/inference/inf_t2t_model.h>
#include <mbase/inference/inf_t2t_processor.h>
#include <mbase/inference/inf_t2t_client.h>
#include <mbase/inference/inf_gguf_metadata_configurator.h>
#include <mbase/filesystem.h>
#include <iostream>
#include <mbase/vector.h>

void print_usage()
{
    printf("Description: Embedding the generated tokens of the given prompt to the .gguf file\n");
    printf("Usage: mbase_nlquery_cooker <nlquery_prompt_path> <model.gguf>\n\n");
}

bool gIsRunning = true;
mbase::string nlqueryPromptPath;
mbase::string modelPath;

class ModelObject;
class ProcessorObject;

class ProcessorObject : public mbase::InfProcessorTextToText {
public:
    ProcessorObject(){}
    ~ProcessorObject()
    {
        this->release_inference_client_stacked();
    }

    void on_initialize_fail(last_fail_code out_code) override
    {
        std::cout << "Processor initialization failed." << std::endl;
        gIsRunning = false;
    }

    void on_initialize() override
    {
        std::cout << "Processor is initialized." << std::endl;
        std::cout << "Cooking the system prompt." << std::endl;
        
        mbase::string nlqueryPromptString = mbase::read_file_as_string(nlqueryPromptPath);
        if(!nlqueryPromptString.size())
        {
            std::cout << "NLQuery system prompt is missing!" << std::endl;
            exit(1);
        }

        mbase::context_line ctxLine;
        ctxLine.mRole = mbase::context_role::NONE;
        ctxLine.mMessage = nlqueryPromptString;

        mbase::inf_text_token_vector nlqueryTokenVector;

        if(this->tokenize_input(&ctxLine, 1, nlqueryTokenVector) != ProcessorObject::flags::INF_PROC_SUCCESS)
        {
            std::cout << "Unable to tokenize the nlquery system prompt" << std::endl;
            exit(1);
        }

        if(!nlqueryTokenVector.size())
        {
            std::cout << "Unable to tokenize the nlquery system prompt" << std::endl;
            exit(1);
        }

        mbase::GgufMetaConfigurator ggufMetaConfig(mbase::from_utf8(modelPath));

        ggufMetaConfig.set_key("nlquery.tokens", nlqueryTokenVector);
        ggufMetaConfig.clear_context();
        std::cout << "System prompt succesfully cooked!" << std::endl;
        exit(0);
    }

    void on_destroy() override{}
};

class ModelObject : public mbase::InfModelTextToText {
public:
    void on_initialize_fail(init_fail_code out_fail_code) override
    {
        std::cout << "Model initialization failed." << std::endl;
        gIsRunning = false;
    }

    void on_initialize() override
    {
        std::cout << "Model is initialized." << std::endl;

        uint32_t contextSize = 1024;
        uint32_t batchSize = 512;
        uint32_t procThreadCount = 16;
        uint32_t genThreadCount = 8;
        bool isFlashAttention = true;
        mbase::inf_sampling_set samplingSet; // We are setting greedy sampler by supplying empty sampling set

        ModelObject::flags registerationStatus = this->register_context_process(
            &processorObject,
            contextSize,
            batchSize,
            genThreadCount,
            procThreadCount,
            isFlashAttention,
            samplingSet
        );

        if(registerationStatus != ModelObject::flags::INF_MODEL_INFO_REGISTERING_PROCESSOR)
        {
            std::cout << "Registration unable to proceed." << std::endl;
            gIsRunning = false;
        }
    }
    void on_destroy() override{}
private:
    ProcessorObject processorObject;
};

int main(int argc, char** argv)
{
    if(argc < 3)
    {
        print_usage();
        exit(1);
    }

    nlqueryPromptPath = mbase::string(argv[1]);
    modelPath = mbase::string(argv[2]);

    ModelObject modelObject;

    uint32_t totalContextLength = 32000;
    int32_t gpuLayersToUse = 80;
    bool isMmap = true;
    bool isMLock = true;

    if (modelObject.initialize_model_ex(
        mbase::from_utf8(modelPath),
        totalContextLength,
        gpuLayersToUse,
        isMmap,
        isMLock
    ) != ModelObject::flags::INF_MODEL_INFO_INITIALIZING_MODEL)
    {
        std::cout << "Unable to start initializing the model." << std::endl;
        return 1;
    }

    while(gIsRunning)
    {
        modelObject.update();
        mbase::sleep(2);
    }

    return 0;
}
