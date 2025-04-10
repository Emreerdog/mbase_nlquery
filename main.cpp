#include <mbase/inference/inf_device_desc.h>
#include <mbase/vector.h>
#include <iostream>
#include <libpq-fe.h>
#include <mbase/io_file.h>
#include <mbase/json/json.h>
#include "global_state.h"
#include "db_ops.h"
#include "model_proc_cl.h"
#include "nlq_status.h"
#include "httplib.h"

void send_error(const httplib::Request& in_req, httplib::Response& in_resp, int in_status_code)
{
    mbase::Json errorDesc;
    errorDesc["status"] = in_status_code;
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
    std::cout << givenJson.toStringPretty() << std::endl;
    if(!givenJson["db_name"].isString() || !givenJson["db_provider"].isString() || !givenJson["db_username"].isString()
        || !givenJson["db_password"].isString() || !givenJson["db_hostname"].isString() || !givenJson["db_port"].isLong()
        || !givenJson["query"].isString())
    {
        send_error(in_req, in_resp, NLQ_INVALID_PAYLOAD);
        return;
    }

    const mbase::string& databaseName = givenJson["db_name"].getString();
    const mbase::string& provider = givenJson["db_provider"].getString();
    const mbase::string& username = givenJson["db_username"].getString();
    const mbase::string& password = givenJson["db_password"].getString();
    const mbase::string& hostname = givenJson["db_hostname"].getString();
    const mbase::string& query = givenJson["query"].getString();
    int hostPort = givenJson["db_port"].getLong();

    mbase::string sqlHistory;
    if(givenJson["sql_history"].isString())
    {
        sqlHistory = givenJson["sql_history"].getString();
    }

    if(!databaseName.size() || !provider.size() || !username.size() || !hostname.size() || !query.size() || hostPort <= 0)
    {
        send_error(in_req, in_resp, NLQ_INVALID_PAYLOAD);
        return;
    }
    
    if(provider == "PostgreSQL")
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
        mbase::psql_produce_output(connPtr, gGlobalModel, formedString, outputJson);
        mbase::string outputString = outputJson.toString();
        in_resp.set_content(outputString.c_str(), outputString.size(), "application/json");
        return;
    }

    else if(provider == "MySQL")
    {
        
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
    if(gProgramData.keyFileSet)
    {
        svr = new httplib::SSLServer(gProgramData.publicKeyFile.c_str(), gProgramData.privateKeyFile.c_str());
    }

    else
    {
        svr = new httplib::Server;
    }
    #else
    httplib::Server server;
    svr = &server;
    #endif

    svr->set_mount_point("/", "./static");
    svr->Post("/nlquery", nlquery_endpoint);
    printf("Server started listening...\n");
    svr->listen("127.0.0.1", 8080);
    printf("ERR: Server can't listen\n");
    exit(1);
}

int main()
{       
    mbase::NlqModel myModel(4);
    myModel.initialize_model_sync(L"/Users/erdog/GGUF/qwen2.5-7b-instruct-q3_k_m.gguf", 9999999, 999);
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