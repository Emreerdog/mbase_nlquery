#include <mbase/inference/inf_device_desc.h>
#include <mbase/vector.h>
#include <iostream>
#include <libpq-fe.h>
#include <mbase/io_file.h>
#include <mbase/json/json.h>
#include <mbase/argument_get_value.h>
#include <mbase/inference/inf_gguf_metadata_configurator.h>
#include "global_state.h"
#include "db_ops.h"
#include "model_proc_cl.h"
#include "nlq_status.h"
#include "httplib.h"

#define MBASE_NLQUERY_VERSION "v1.0.0"

void print_usage()
{
    printf("========================================\n");
    printf("#Program name:      mbase_nlquery\n");
    printf("#Version:           %s\n", MBASE_NLQUERY_VERSION);
    printf("#Type:              Application\n");
    printf("#Further docs:      https://github.com/Emreerdog/mbase_nlquery\n");
    printf("***** DESCRIPTION *****\n");
    printf("This program runs the NLQuery application and provides a single REST API endpoint '/nlquery'\n");
    printf("========================================\n\n");
    printf("Usage: mbase_nlquery *[<option> [<value>]]\n");
    printf("Options: \n\n");
    printf("--help                            Print usage.\n");
    printf("-v, --version                     Shows program version.\n");
    printf("--program-path <str>              NLQuery web page and model path (default=./nlquery).\n");
    printf("--hostname <str>                  Hostname to listen to (default=\"127.0.0.1\").\n");
    printf("--port <int>                      Port to listen to (default=\"8080 if HTTP, 443 if HTTPS\").\n");
    printf("--ssl-public <str>                SSL public key file.\n");
    printf("--ssl-private <str>               SSL private key file.\n");
    printf("--user-count <int>                Amount of users that the NLQuery can process simultaneously (default=2).\n");
    printf("--max-rows <int>                  Total number of rows that the NLQuery can return (default=1000).\n");
    printf("--disable-webui                   Disables webui.\n\n");
}

void send_error(const httplib::Request& in_req, httplib::Response& in_resp, int in_status_code, const mbase::string& in_data = "")
{
    mbase::Json errorDesc;
    errorDesc["status"] = in_status_code;

    if(in_status_code == NLQ_ENGINE_OVERLOADED)
    {
        errorDesc["message"] = "NLQuery engine is overloaded. Try again later";
    }

    else if(in_status_code == NLQ_CONNECTION_FAILED)
    {
        errorDesc["message"] = "Database connection failed";
    }

    else if(in_status_code == NLQ_PROMPT_INVALID)
    {
        errorDesc["message"] = "Given query is invalid. Make sure it is natural language and its context is related to the SQL database";
    }

    else if(in_status_code == NLQ_INTERNAL_SERVER_ERROR)
    {
        errorDesc["message"] = "Internal server error. Try again later";
    }

    else if(in_status_code == NLQ_INVALID_PAYLOAD)
    {
        errorDesc["message"] = "Message body is invalid. Make sure you populate the mandatory fields correctly";
    }

    else if(in_status_code == NLQ_NOT_SUPPORTED)
    {
        errorDesc["message"] = "Given database provider is not supported";
    }

    else if(in_status_code == NLQ_DB_ERR)
    {
        errorDesc["message"] = "Database failed to execute the generated query";
    }

    else if(in_status_code == NLQ_INPUT_TOO_LONG)
    {
        errorDesc["message"] = "Given prompt is too long. This may also happen if the provided sql_history is too long";
    }

    else if(in_status_code == NLQ_TOO_MUCH_DATA)
    {
        errorDesc["message"] = "Too much data returned from the database";
    }

    if(in_data.size())
    {
        errorDesc["data"] = in_data;
    }

    mbase::string outputString = errorDesc.toString();
    in_resp.set_content(outputString.c_str(), outputString.size(), "application/json");
}

void nlquery_endpoint(const httplib::Request& in_req, httplib::Response& in_resp)
{
    mbase::string reqBody(in_req.body.c_str(), in_req.body.size());
    std::pair<mbase::Json::Status, mbase::Json> parseResult = mbase::Json::parse(reqBody);

    if(parseResult.first != mbase::Json::Status::success)
    {
        send_error(in_req, in_resp, NLQ_INVALID_PAYLOAD);
        return;
    }
    mbase::Json& givenJson = parseResult.second;
    if(!givenJson["db_name"].isString() || !givenJson["db_provider"].isString() || !givenJson["db_username"].isString()
        || !givenJson["db_password"].isString() || !givenJson["db_hostname"].isString() || !givenJson["db_port"].isLong()
        || !givenJson["query"].isString())
    {
        send_error(in_req, in_resp, NLQ_INVALID_PAYLOAD);
        return;
    }

    const mbase::string& databaseName = givenJson["db_name"].getString();
    mbase::string& provider = givenJson["db_provider"].getString();
    const mbase::string& username = givenJson["db_username"].getString();
    const mbase::string& password = givenJson["db_password"].getString();
    const mbase::string& hostname = givenJson["db_hostname"].getString();
    const mbase::string& query = givenJson["query"].getString();
    int hostPort = givenJson["db_port"].getLong();

    mbase::string sqlHistory;
    bool genOnly = true;
    if(givenJson["sql_history"].isString())
    {
        sqlHistory = givenJson["sql_history"].getString();
    }

    if(givenJson["generate_only"].isBool())
    {
        genOnly = givenJson["generate_only"].getBool();
    }

    if(!databaseName.size() || !provider.size() || !username.size() || !hostname.size() || !query.size() || hostPort <= 0)
    {
        send_error(in_req, in_resp, NLQ_INVALID_PAYLOAD);
        return;
    }
    
    provider.to_lower();

    if(provider == "postgresql")
    {
        mbase::string outputFormat = mbase::string::from_format("host=%s port=%d dbname=%s user=%s password=%s connect_timeout=2 sslmode=allow", hostname.c_str(), hostPort, databaseName.c_str(), username.c_str(), password.c_str());
        PGconn* connPtr = PQconnectdb(outputFormat.c_str());

        if(PQstatus(connPtr) == ConnStatusType::CONNECTION_BAD) // connection bad? monke sad.
        {
            send_error(in_req, in_resp, NLQ_CONNECTION_FAILED);
            return;
        }

        mbase::string tableInformation;
        try
        {
            if(!mbase::psql_get_all_tables(connPtr, tableInformation))
            {
                send_error(in_req, in_resp, NLQ_DB_ERR);
                return;
            }   
        }
        catch(const std::exception& e)
        {
            send_error(in_req, in_resp, NLQ_INTERNAL_SERVER_ERROR);
            return;
        }

        mbase::string formedString = mbase::prepare_prompt(provider, tableInformation, sqlHistory, query);
        mbase::Json outputJson;
        mbase::I32 outputCode;
        mbase::string generatedSql;
        if(!mbase::psql_produce_output(connPtr, gGlobalModel, genOnly, formedString, outputJson, outputCode, generatedSql))
        {
            send_error(in_req, in_resp, outputCode, generatedSql);
            return;
        }
        mbase::string outputString = outputJson.toString();
        in_resp.set_content(outputString.c_str(), outputString.size(), "application/json");
        return;
    }

    else
    {
        send_error(in_req, in_resp, NLQ_NOT_SUPPORTED);
        return;
    }
}

void server_thread()
{
    httplib::Server* svr = NULL;
    #ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    if(gSSLEnabled)
    {
        svr = new httplib::SSLServer(gSSLPublicPath.c_str(), gSSLPrivatePath.c_str());
    }

    else
    {
        svr = new httplib::Server;
    }
    #else
    httplib::Server server;
    svr = &server;
    #endif

    if(gIsWebui)
    {
        mbase::string webPath = gProgramPath + "/web";
        svr->set_mount_point("/", webPath.c_str());
    }
    svr->Post("/nlquery", nlquery_endpoint);
    printf("Server started listening...\n");
    svr->listen(gListenHostname.c_str(), gListenPort);
    printf("ERR: Server can't listen! Make sure the hostname and port is valid\n");
    exit(1);
}

int main(int argc, char** argv)
{       
    for(int i = 0; i < argc; i++)
    {
        mbase::string argumentString = argv[i];
        if(argumentString == "-v" || argumentString == "--version")
        {
            printf("MBASE NLQuery %s\n", MBASE_NLQUERY_VERSION);
            return 0;
        }

        else if(argumentString == "--help")
        {
            print_usage();
            return 0;
        }

        else if(argumentString == "--program-path")
        {
            mbase::argument_get<mbase::string>::value(i, argc, argv, gProgramPath);
        }

        else if(argumentString == "--hostname")
        {
            mbase::argument_get<mbase::string>::value(i, argc, argv, gListenHostname);
        }

        else if(argumentString == "--port")
        {
            mbase::argument_get<int>::value(i, argc, argv, gListenPort);
        }

        else if(argumentString == "--ssl-public")
        {
            mbase::argument_get<mbase::string>::value(i, argc, argv, gSSLPublicPath);
        }

        else if(argumentString == "--ssl-private")
        {
            mbase::argument_get<mbase::string>::value(i, argc, argv, gSSLPrivatePath);
        }

        else if(argumentString == "--user-count")
        {
            mbase::argument_get<int>::value(i, argc, argv, gUserCount);
        }

        else if(argumentString == "--max-rows")
        {
            mbase::argument_get<int>::value(i, argc, argv, gMaxRows);
        }

        else if(argumentString == "--disable-webui")
        {
            gIsWebui = false;
        }
    }

    if(!gUserCount)
    {
        printf("ERR: User count must be greater than 0\n");
    }

    if(!gListenHostname.size())
    {
        printf("ERR: Hostname must be specified\n");
    }

    if(!gListenPort)
    {
        printf("ERR: Port can't be 0\n");
    }

    if(gSSLPublicPath.size() || gSSLPrivatePath.size())
    {
        gSSLEnabled = true;
    }

    gModelPath = gProgramPath + "/Qwen2.5-7B-Instruct-1M-NLQuery-q8_0.gguf";
    bool triedBefore = false;

    while(1)
    {
        mbase::GgufMetaConfigurator ggufMetaConfig(mbase::from_utf8(gModelPath));

        if(!ggufMetaConfig.is_open())
        {
            if(triedBefore)
            {
                printf("ERR: NLQuery attempted to download the model from Huggingface but failed. Make sure you have the 'curl' and an internet connection so that the NLQuery will download the model at program startup\n");
                exit(1);
            }
            printf("INFO: Model not found at program path\n");
            printf("INFO: Downloading the model from Huggingface: \n");
            mbase::string shellCommand = "cd $HOME/.local/myapp && curl -L -O https://huggingface.co/MBASE/Qwen2.5-7B-Instruct-NLQuery/resolve/main/Qwen2.5-7B-Instruct-1M-NLQuery-q8_0.gguf";
            system(shellCommand.c_str());
            triedBefore = true;
        }

        else
        {
            if(!ggufMetaConfig.has_kv_key("nlquery.tokens"))
            {
                printf("ERR: Model parameters are manually altered!\n");
                printf("INFO: Delete the model and restart the application\n");
            }
            break;
        }
    }

    mbase::NlqModel myModel(gUserCount);
    if(myModel.initialize_model_sync(mbase::from_utf8(gModelPath), 9999999, 999) == mbase::NlqModel::flags::INF_MODEL_ERR_CANT_LOAD_MODEL)
    {
        printf("ERR: Failed to load the model\n");
        exit(1);
    }
    myModel.update();

    gGlobalModel = &myModel;

    mbase::thread t1(server_thread);
    t1.run();
    while(1)
    {
        gLoopSync.acquire();
        myModel.update();
        gLoopSync.release();
        mbase::sleep(2);
    }
    return 0;
}