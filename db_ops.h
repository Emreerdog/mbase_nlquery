#ifndef MBASE_NLQ_DB_OPS_H
#define MBASE_NLQ_DB_OPS_H

#include <mbase/common.h>
#include <mbase/string.h>
#include <libpq-fe.h>
#include "model_proc_cl.h"
#include "nlq_status.h"

MBASE_BEGIN

class PostgreSafeConnect {
public:
    PostgreSafeConnect(
        const mbase::string& in_hostname,
        const I32& in_port,
        const mbase::string& in_dbname,
        const mbase::string& in_username,
        const mbase::string& in_password
    )
    {
        mbase::string outputFormat = mbase::string::from_format("host=%s port=%d dbname=%s user=%s password=%s connect_timeout=2 sslmode=allow", in_hostname.c_str(), in_port, in_dbname.c_str(), in_username.c_str(), in_password.c_str());
        mPostgreConnection = PQconnectdb(outputFormat.c_str());

        if(PQstatus(mPostgreConnection) == ConnStatusType::CONNECTION_BAD)
        {
            // siktir git o zaman
            if(mPostgreConnection)
            {
                PQfinish(mPostgreConnection);
            }
            mPostgreConnection = nullptr;
        }
        else
        {
            this->bIsConnected = true;
        }
    }

    ~PostgreSafeConnect()
    {
        if(mPostgreConnection)
        {
            PQfinish(mPostgreConnection);
        }
    }

    bool isConnected()
    {
        return bIsConnected;
    }

    PGconn* get_connection_ptr()
    {
        return mPostgreConnection;
    }
private:
    PGconn* mPostgreConnection = nullptr;
    bool bIsConnected = false;
};

mbase::string prepare_nlquery_prompt(
    const mbase::string& in_sql_history,
    const mbase::string& in_nlquery
)
{
    mbase::string sqlHistorySection = "<SQL_HISTORY_BEGIN>\n" + in_sql_history + "\n<SQL_HISTORY_END>\n";
    mbase::string nlQuerySection = "<NLQUERY_BEGIN>\n" + in_nlquery + "\n<NLQUERY_END>\n";
    return sqlHistorySection + nlQuerySection;
}

mbase::string prepare_semantic_correction_prompt(
    const mbase::string& in_sql_history,
    const mbase::string& in_language
)
{
    mbase::string sqlHistorySection = "<SQL_HISTORY_BEGIN>\n" + in_sql_history + "\n<SQL_HISTORY_END>\n";
    mbase::string correctionSection = "<SEMANTIC_CORRECT_NATURAL_LANGUAGE_BEGIN>\n" + in_language + "\n<SEMANTIC_CORRECT_NATURAL_LANGUAGE_END>\n";
    return sqlHistorySection + correctionSection;
}

GENERIC build_table_metadata(const mbase::string& in_schema_name, const mbase::string& in_table_name, mbase::vector<mbase::Json>& in_meta_vector)
{
    mbase::string tableMetaTotalString = in_table_name + '=';
    for(mbase::Json& metaItem : in_meta_vector)
    {
        table_relation_meta trm;
        mbase::string constType = "null";
        mbase::string refColumn = "null";
        mbase::string refTable = "null";

        if(metaItem["constraint_type"].isString())
        {
            constType = metaItem["constraint_type"].getString();
        }

        if(metaItem["referenced_table"].isString())
        {
            refTable = metaItem["referenced_table"].getString();
        }

        if(metaItem["referenced_column"].isString())
        {
            refColumn = metaItem["referenced_column"].getString();
        }

        trm.columnName = metaItem["column_name"].getString();
        trm.columnDataType = metaItem["data_type"].getString();
        trm.constraintName = constType;
        trm.referenceTable = refTable;
        trm.referenceColumn = refColumn;

        gCachedTableRelations[in_table_name].push_back(trm);
        tableMetaTotalString += trm.columnName + ';' + trm.columnDataType + ';' + trm.referenceTable + ',';
    }
    tableMetaTotalString.pop_back(); // remove the last comma
    tableMetaTotalString += '\n';
    gSchemaTableMap[in_schema_name] += tableMetaTotalString;
}

bool psql_get_all_tables(PGconn* in_connection)
{
    mbase::string cachedTableJsonString = mbase::read_file_as_string("table.json");
    if(cachedTableJsonString.size())
    {
        // reading from cached table metadata
        printf("INFO: Reading from cached schema information!\n");
        mbase::Json tableMetaInformation = mbase::Json::parse(cachedTableJsonString).second;
        std::map<mbase::string, mbase::Json> schemaObject = tableMetaInformation.getObject();
        for(auto& n : schemaObject)
        {
            for(mbase::Json& metadataObject : n.second.getArray())
            {
                build_table_metadata(n.first, metadataObject["table"].getString(), metadataObject["meta"].getArray());
            }
        }
        return true;
    }

    if(!gProvidedSchemas.size())
    {
        // Retrieve all schemas from the database
        mbase::string getSchemasQuery = "SELECT nspname FROM pg_namespace";
        PGresult* resultExec = PQexec(in_connection, getSchemasQuery.c_str());
    
        ExecStatusType est = PQresultStatus(resultExec);
        if(est != ExecStatusType::PGRES_TUPLES_OK)
        {
            if(resultExec)
            {
                PQclear(resultExec);
            }
        }
        else
        {
            int tupleCount = PQntuples(resultExec);
            for(int i = 0; i < tupleCount; i++)
            {
                mbase::string currentSchemaName(PQgetvalue(resultExec, i, 0));
                if(currentSchemaName == "information_schema" || currentSchemaName == "pg_catalog" || currentSchemaName == "pg_toast")
                {
                    continue;
                }
                gProvidedSchemas.insert(currentSchemaName);
            }
        }
    }

    mbase::string outputTables;
    mbase::Json totalJson;

    for(const mbase::string& schemaName: gProvidedSchemas)
    {
        totalJson[schemaName].setArray();
        //mbase::string getTablesQuery = mbase::string::from_format("SELECT t.table_name, json_agg( json_build_object( 'column_name', c.column_name, 'data_type', c.data_type ) ) AS columns FROM information_schema.tables t JOIN information_schema.columns c ON t.table_name = c.table_name WHERE t.table_schema = '%s' AND t.table_type = 'BASE TABLE' AND c.table_schema = '%s' GROUP BY t.table_name;", schemaName.c_str(), schemaName.c_str());
        //mbase::string getTablesQuery = mbase::string::from_format("SELECT t.table_name, json_agg( json_build_object( 'column_name', c.column_name, 'data_type', c.data_type, 'constraint_type', tc.constraint_type, 'referenced_table', ccu.table_name, 'referenced_column', ccu.column_name ) ) AS columns FROM information_schema.tables t JOIN information_schema.columns c ON t.table_name = c.table_name AND t.table_schema = c.table_schema LEFT JOIN information_schema.key_column_usage kcu ON c.table_name = kcu.table_name AND c.column_name = kcu.column_name AND c.table_schema = kcu.table_schema LEFT JOIN information_schema.table_constraints tc ON kcu.constraint_name = tc.constraint_name AND kcu.table_schema = tc.table_schema LEFT JOIN information_schema.constraint_column_usage ccu ON tc.constraint_type = 'FOREIGN KEY' AND tc.constraint_name = ccu.constraint_name AND tc.table_schema = ccu.table_schema WHERE t.table_schema = '%s' AND t.table_type = 'BASE TABLE' GROUP BY t.table_name;", schemaName.c_str());
        mbase::string getTablesQuery = mbase::string::from_format("SELECT t.table_name, json_agg( json_build_object( 'column_name', c.column_name, 'data_type', c.data_type, 'constraint_type', tc.constraint_type, 'referenced_table', ccu.table_name, 'referenced_column', ccu.column_name ) ) AS columns FROM information_schema.tables t JOIN information_schema.columns c ON t.table_name = c.table_name AND t.table_schema = c.table_schema LEFT JOIN information_schema.key_column_usage kcu ON c.table_name = kcu.table_name AND c.column_name = kcu.column_name AND c.table_schema = kcu.table_schema LEFT JOIN information_schema.table_constraints tc ON kcu.constraint_name = tc.constraint_name AND kcu.table_schema = tc.table_schema LEFT JOIN information_schema.constraint_column_usage ccu ON tc.constraint_type = 'FOREIGN KEY' AND tc.constraint_name = ccu.constraint_name AND tc.table_schema = ccu.table_schema WHERE t.table_schema = '%s' AND t.table_type = 'BASE TABLE' GROUP BY t.table_name;", schemaName.c_str());
        PGresult* resultExec = PQexec(in_connection, getTablesQuery.c_str());
    
        ExecStatusType est = PQresultStatus(resultExec);
        if(est != ExecStatusType::PGRES_TUPLES_OK)
        {
            if(resultExec)
            {
                PQclear(resultExec);
            }
            return false;
        }
        
        mbase::vector<mbase::Json>& jsonArray = totalJson[schemaName].getArray();
        int tupleCount = PQntuples(resultExec);
        for(int i = 0; i < tupleCount; i++)
        {
            mbase::string tableName = PQgetvalue(resultExec, i, 0);
            mbase::string tableMetaData = PQgetvalue(resultExec, i, 1);

            mbase::Json tableDescriptionJson;
            mbase::Json metadataJsonArray = mbase::Json::parse(tableMetaData).second;

            tableDescriptionJson["table"] = tableName;
            tableDescriptionJson["meta"] = metadataJsonArray;

            build_table_metadata(schemaName, tableName, metadataJsonArray.getArray());

            jsonArray.push_back(tableDescriptionJson);
        }
    
        PQclear(resultExec);    
    }
    mbase::write_string_to_file("table.json", totalJson.toStringPretty());
    return true;
}

bool psql_produce_output(PGconn* in_connection, NlqModel* in_model, bool in_genonly, const mbase::string& in_prompt, const mbase::string& in_sql_history, mbase::Json& out_json, I32& out_status, mbase::string& out_sql)
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
    if(genSql.contains("NLQ_INV"))
    {
        out_status = NLQ_PROMPT_INVALID;
        return false;
    }

    if(genSql.size() > 9)
    {
        // ```sql``` this is 9 characters
        mbase::string markdownString(genSql.begin(), genSql.begin() + 6);
        if(markdownString == "```sql")
        {
            // trim the ```sql``` part
            genSql = mbase::string(genSql.begin() + 6, genSql.end() - 3);
        }
    }

    if(in_genonly)
    {
        out_json["status"] = NLQ_SUCCESS;
        out_json["sql"] = genSql;
        return true;
    }

    PGresult* resultExec = PQexec(in_connection, genSql.c_str());
    if(!resultExec)
    {
        out_status = NLQ_INTERNAL_SERVER_ERROR;
        out_sql = genSql;
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

MBASE_END

#endif // MBASE_NLQ_DB_OPS_H