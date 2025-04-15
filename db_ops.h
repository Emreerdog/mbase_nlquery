#ifndef MBASE_NLQ_DB_OPS_H
#define MBASE_NLQ_DB_OPS_H

#include <mbase/common.h>
#include <mbase/string.h>
#include <libpq-fe.h>
#include "model_proc_cl.h"
#include "nlq_status.h"

MBASE_BEGIN

bool psql_get_all_tables(PGconn* in_connection, mbase::string& out_tables)
{
    mbase::string getTablesQuery = "SELECT t.table_name, json_agg( json_build_object( 'column_name', c.column_name, 'data_type', c.data_type ) ) AS columns FROM information_schema.tables t JOIN information_schema.columns c ON t.table_name = c.table_name WHERE t.table_schema = 'public' AND t.table_type = 'BASE TABLE' AND c.table_schema = 'public' GROUP BY t.table_name;";
    PGresult* resultExec = PQexec(in_connection, getTablesQuery.c_str());

    ExecStatusType est = PQresultStatus(resultExec);
    if(est != ExecStatusType::PGRES_TUPLES_OK)
    {
        return false;
    }
    mbase::string outputTables;
    int tupleCount = PQntuples(resultExec);
    for(int i = 0; i < tupleCount; i++)
    {
        outputTables += mbase::string::from_format("%s : %s\n", PQgetvalue(resultExec, i, 0), PQgetvalue(resultExec, i, 1));
    }
    out_tables = std::move(outputTables);

    PQclear(resultExec);
    return true;
}

bool psql_produce_output(PGconn* in_connection, NlqModel* in_model, const mbase::string& in_prompt, mbase::Json& out_json, I32& out_status, mbase::string& out_sql)
{
    NlqProcessor* activeProcessor = NULL;
    if(!in_model->acquire_processor(activeProcessor))
    {
        out_status = NLQ_ENGINE_OVERLOADED;
        return false;
    }
    mbase::context_line ctxLine;
    ctxLine.mMessage = in_prompt;
    ctxLine.mRole = mbase::context_role::USER;
    mbase::inf_text_token_vector tokenVector;
    if(activeProcessor->tokenize_input(&ctxLine, 1, tokenVector) == NlqProcessor::flags::INF_PROC_ERR_UNABLE_TO_TOKENIZE_INPUT)
    {
        out_status = NLQ_INTERNAL_SERVER_ERROR;
        in_model->release_processor(activeProcessor);
        return false;
    }
    NlqClient* clientPtr = static_cast<NlqClient*>(activeProcessor->get_assigned_client());
    clientPtr->query_hard_reset();
    if(activeProcessor->execute_input_sync(tokenVector) != NlqProcessor::flags::INF_PROC_INFO_NEED_UPDATE)
    {
        out_status = NLQ_INTERNAL_SERVER_ERROR;
        in_model->release_processor(activeProcessor);
        return false;
    }
    gLoopSync.acquire();
    activeProcessor->update();
    gLoopSync.release();
    while(clientPtr->is_processing())
    {
        mbase::sleep(2); // prevent overuse
    }
    mbase::string genSql = clientPtr->get_generated_query();
    in_model->release_processor(activeProcessor);
    if(genSql == "NLQ_INV")
    {
        out_status = NLQ_PROMPT_INVALID;
        return false;
    }

    PGresult* resultExec = PQexec(in_connection, genSql.c_str());
    if(!resultExec)
    {
        out_status = NLQ_INTERNAL_SERVER_ERROR;
        out_sql = genSql;
        PQclear(resultExec);
        return false;
    }

    ExecStatusType est = PQresultStatus(resultExec);
    if(est == ExecStatusType::PGRES_COMMAND_OK)
    {
        // means db is modified
        out_json["status"] = NLQ_SUCCESS;
        out_json["sql"] = genSql;
    }

    else if(est == ExecStatusType::PGRES_TUPLES_OK)
    {
        // means data is read from db
        out_json["status"] = NLQ_SUCCESS;
        out_json["sql"] = genSql;
        out_json["data"].setObject();
        for(I32 i = 0; i < PQnfields(resultExec); ++i)
        {
            mbase::string fieldName = PQfname(resultExec, i);
            out_json["data"][fieldName].setArray();
            I32 rowCounter = 0;
            for(I32 j = 0; j < PQntuples(resultExec); ++j)
            {
                if(rowCounter == gMaxRows)
                {
                    // Prompt returned a result which contains more than 1000 rows
                    out_status = NLQ_TOO_MUCH_DATA;
                    out_sql = genSql;
                    break;
                }
                I32 binaryLength = PQgetlength(resultExec, j, i);
                mbase::string dataString(PQgetvalue(resultExec, j, i), binaryLength);
                if(binaryLength <= 64)
                {
                    if(dataString.is_integer(dataString.c_str()))
                    {
                        out_json["data"][fieldName][j] = dataString.to_i64();
                    }
                    else if(dataString.is_float(dataString.c_str()))
                    {
                        out_json["data"][fieldName][j] = dataString.to_f64();
                    }
                    else
                    {
                        out_json["data"][fieldName][j] = dataString;
                    }
                }
                else
                {
                    out_json["data"][fieldName][j] = dataString;
                }
            }
        }
    }

    else
    {
        out_sql = genSql;
        out_status = NLQ_DB_ERR;
        PQclear(resultExec);
        return false;
    }
    out_sql = genSql;
    PQclear(resultExec);
    return true;
}

mbase::string prepare_prompt(
    const mbase::string& in_db_provider,
    const mbase::string& in_table_info,
    const mbase::string& in_sql_history,
    const mbase::string& in_nlquery
)
{
    mbase::string providerSection = "<DB_SOURCE_BEGIN>\n" + in_db_provider + "\n<DB_SOURCE_END>\n";
    mbase::string tableInfoSection = "<TABLE_INFO_BEGIN>\n" + in_table_info + "\n<TABLE_INFO_END>\n";
    mbase::string sqlHistorySection = "<SQL_HISTORY_BEGIN>\n" + in_sql_history + "\n<SQL_HISTORY_END>\n";
    mbase::string nlQuerySection = "<NL_QUERY_BEGIN>\n" + in_nlquery + "\n<NL_QUERY_END>\n";

    return providerSection + tableInfoSection + sqlHistorySection + nlQuerySection;
}

MBASE_END

#endif // MBASE_NLQ_DB_OPS_H