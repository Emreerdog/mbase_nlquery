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
    printf("-h, --help                        Print usage.\n");
    printf("-v, --version                     Shows program version.\n");
    printf(mbase::string::from_format("--program-path <str>              NLQuery web page path (default=%s).\n", gProgramPath.c_str()).c_str());
    printf(mbase::string::from_format("--model-path <str>                NLQuery model path (default=%s).\n", gModelPath.c_str()).c_str());
    printf("--hostname <str>                  Hostname to listen to (default=\"127.0.0.1\").\n");
    printf("--port <int>                      Port to listen to (default=\"8080 if HTTP, 443 if HTTPS\").\n");
    printf("--ssl-public <str>                SSL public key file.\n");
    printf("--ssl-private <str>               SSL private key file.\n");
    printf("--schema <str>                    Schema name to query from. For multiple schemas, specify this option multiple times. If no schema name is provided, the NLQuery engine will query all schema information in the database.\n");
    printf("--user-count <int>                Amount of users that the NLQuery can process simultaneously (default=2).\n");
    printf("--max-rows <int>                  Total number of rows that the NLQuery can return (default=1000).\n");
    printf("--disable-webui                   Disables webui.\n");
    printf("--hint-file <str>                 Optional text file containing hints and information about the database. If given, may improve performance.\n");
    printf("--db-hostname <str>               Hostname of the postgresql database.\n");
    printf("--db-port <int>                   Port of the database.\n");
    printf("--db-name <str>                   Name of the database.\n");
    printf("--db-username <str>               Username to use for accessing to the database.\n");
    printf("--db-password <str>               Password of the database username.\n");
    printf("--gpu-layers <int>                Number of layers to be offloaded to GPU (default=999).\n\n");
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

    mbase::string databaseName = gDBName;
    mbase::string provider = gDBProvider;
    mbase::string userName = gDBUsername;
    mbase::string password = gDBPassword;
    mbase::string hostname = gDBHostname;
    int hostPort = gDBPort;

    mbase::Json& givenJson = parseResult.second;
    
    if(!givenJson["query"].isString())
    {
        send_error(in_req, in_resp, NLQ_INVALID_PAYLOAD);
        return;
    }

    if(givenJson["db_username"].isString())
    {
        userName = givenJson["db_username"].getString();
    }

    if(givenJson["db_password"].isString())
    {
        password = givenJson["db_password"].getString();
    }

    mbase::string query = givenJson["query"].getString();
    mbase::string sqlHistory;
    bool genOnly = true;
    if(givenJson["sql_history"].isArray())
    {
        mbase::I32 historyCounter = 1;
        for(mbase::Json& sqlHistoryItem : givenJson["sql_history"].getArray())
        {
            if(sqlHistoryItem["query_old"].isString() && sqlHistoryItem["sql"].isString())
            {
                sqlHistory +=  mbase::string::from_format("NLQ-%d: ", historyCounter) + sqlHistoryItem["query_old"].getString() + '=' + sqlHistoryItem["sql"].getString() + '\n';
                historyCounter++;
            }
        }
    }

    if(givenJson["generate_only"].isBool())
    {
        genOnly = givenJson["generate_only"].getBool();
    }

    if(!databaseName.size() || !provider.size() || !userName.size() || !hostname.size() || !query.size() || hostPort <= 0)
    {
        send_error(in_req, in_resp, NLQ_INVALID_PAYLOAD);
        return;
    }
    
    provider.to_lower();

    if(provider == "postgresql")
    {
        mbase::PostgreSafeConnect postgreConnector(hostname, hostPort, databaseName, userName, password);
        
        if(!postgreConnector.isConnected()) // connection bad? monke sad.
        {
            send_error(in_req, in_resp, NLQ_CONNECTION_FAILED);
            return;
        }

        mbase::string tableInformation;
        try
        {
            if(!mbase::psql_get_all_tables(postgreConnector.get_connection_ptr(), tableInformation, gSchemaTableMap))
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

        mbase::string formedString = mbase::prepare_prompt_static(sqlHistory, query);
        mbase::Json outputJson;
        mbase::I32 outputCode;
        mbase::string generatedSql;
        if(!mbase::psql_produce_output(postgreConnector.get_connection_ptr(), gGlobalModel, genOnly, formedString, outputJson, outputCode, generatedSql))
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
    printf("\nServer started listening.\n\n");
    mbase::string protocolString = "http://";
    if(gSSLEnabled)
    {
        protocolString = "https://";
    }

    mbase::string webUrl = protocolString + mbase::string::from_format("%s:%d", gListenHostname.c_str(), gListenPort);

    if(gIsWebui)
    {    
        printf("WebUI link: %s\n", webUrl.c_str());
    }
    printf("REST API URL: %s/nlquery\n\n", webUrl.c_str());

    svr->listen(gListenHostname.c_str(), gListenPort);
    printf("ERR: Server can't listen! Make sure the hostname and port is valid\n");
    exit(1);
}

int main(int argc, char** argv)
{       
    if(argc < 2)
    {
        print_usage();
        exit(0);
    }
    for(int i = 0; i < argc; i++)
    {
        mbase::string argumentString = argv[i];
        if(argumentString == "-v" || argumentString == "--version")
        {
            printf("MBASE NLQuery %s\n", MBASE_NLQUERY_VERSION);
            return 0;
        }

        else if(argumentString == "-h" || argumentString == "--help")
        {
            print_usage();
            return 0;
        }

        else if(argumentString == "--program-path")
        {
            mbase::argument_get<mbase::string>::value(i, argc, argv, gProgramPath);
        }

        else if(argumentString == "--model-path")
        {
            mbase::argument_get<mbase::string>::value(i, argc, argv, gModelPath);
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
        
        else if(argumentString == "--schema")
        {
            mbase::string schemaInfo;
            mbase::argument_get<mbase::string>::value(i, argc, argv, schemaInfo);
            gProvidedSchemas.insert(schemaInfo);
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

        else if(argumentString == "--hint-file")
        {
            mbase::argument_get<mbase::string>::value(i, argc, argv, gHintFilePath);
        }

        else if(argumentString == "--db-hostname")
        {
            mbase::argument_get<mbase::string>::value(i, argc, argv, gDBHostname);
        }

        else if(argumentString == "--db-port")
        {
            mbase::argument_get<int>::value(i, argc, argv, gDBPort);
        }

        else if(argumentString == "--db-name")
        {
            mbase::argument_get<mbase::string>::value(i, argc, argv, gDBName);
        }

        else if(argumentString == "--db-username")
        {
            mbase::argument_get<mbase::string>::value(i, argc, argv, gDBUsername);
        }

        else if(argumentString == "--db-password")
        {
            mbase::argument_get<mbase::string>::value(i, argc, argv, gDBPassword);
        }

        else if(argumentString == "--gpu-layers")
        {
            mbase::argument_get<mbase::I32>::value(i, argc, argv, gNLayers);
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
        
    if(!gDBHostname.size() || !gDBName.size() || !gDBUsername.size())
    {
        printf("ERR: DB parameters must be supplied on program startup\n");
        printf("INFO: Make sure you provide the following parameters:\n");
        printf("* --db-hostname (MUST)\n");
        printf("* --db-port (OPTIONAL)(DEFAULT=5432)\n");
        printf("* --db-name (MUST)\n");
        printf("* --db-username (MUST)\n");
        printf("* --db-password (OPTIONAL)\n");
        return 1;
    }
    
    printf("INFO: Static schema option is specified\n");
    printf("INFO: Retrieving schema information from the database...\n");

    mbase::PostgreSafeConnect postgreConnect(gDBHostname, gDBPort, gDBName, gDBUsername, gDBPassword);
    if(!postgreConnect.isConnected())
    {
        printf("FATAL: Unable to connect to PostgreSQL database\n");
        return 1;
    }

    if(!mbase::psql_get_all_tables(postgreConnect.get_connection_ptr(), gTotalSchemaString, gSchemaTableMap))
    {
        printf("FATAL: Unable to retrieve schema information from the database\n");
        return 1;
    }
    printf("SUCCESS: Schema information succesfully retrieved!\n\n");
        
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
            mbase::string shellCommand = mbase::string::from_format("cd %s && curl -L -O https://huggingface.co/MBASE/Qwen2.5-7B-Instruct-NLQuery/resolve/main/Qwen2.5-7B-Instruct-1M-NLQuery-q8_0.gguf", gProgramPath.c_str());
            system(shellCommand.c_str());
            triedBefore = true;
        }

        else
        {
            if(!ggufMetaConfig.has_kv_key("nlquery.tokens"))
            {
                printf("ERR: Model parameters are manually altered!\n");
                printf("INFO: Delete the model and restart the application\n");
                exit(1);
            }
            break;
        }
    }
 
    mbase::NlqModel myModel(gUserCount);

    if(myModel.initialize_model_ex(mbase::from_utf8(gModelPath), 9999999, gNLayers, true, true, mbase::inf_query_devices()) != mbase::NlqModel::flags::INF_MODEL_INFO_INITIALIZING_MODEL)
    {
        printf("ERR: Failed to start model initialization\n");
        exit(1);
    }

    mbase::vector<char> loadingCharacters = {'\\', '|', '-', '/'};
    while(myModel.signal_initializing())
    {
        for(char& n : loadingCharacters)
        {
            fflush(stdout);
            printf("\rSUCCESS: Initializing the model %c", n);
            mbase::sleep(150);
        }
    }
    printf("\n");
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