<div align="center">
  <a href="" />
    <img alt="MBASE" height="325px" src="https://github.com/user-attachments/assets/c8866234-8605-4d15-9f60-829f1cc209e4">
  </a>
</div>
<br>


# MBASE NLQuery

MBASE NLQuery is a natural language to SQL generator/executor engine using the [MBASE SDK](https://github.com/Emreerdog/mbase) as an LLM inference SDK. 

It internally uses the [Qwen2.5-7B-Instruct-NLQuery](https://huggingface.co/MBASE/Qwen2.5-7B-Instruct-NLQuery) model to convert the provided natural language into SQL queries and executes it through the database client SDKs (PostgreSQL only for now). However, the execution can be disabled for security.

MBASE NLQuery doesn't require the user to supply a schema or table information on the database. User only needs to supply mundane parameters such as: database address, port, username, password etc.

It serves a single HTTP REST API endpoint called `nlquery` which can serve to multiple users at the same time and it requires a super-simple JSON formatted data to call.

Also, an MCP server is available to interact with the NLQuery engine.

## Features

- Offline Text-To-SQL generation/executor engine using [Qwen2.5-7B-Instruct-NLQuery](https://huggingface.co/MBASE/Qwen2.5-7B-Instruct-NLQuery) model with [MBASE SDK](https://github.com/Emreerdog/mbase) as an inference SDK which internally uses [llama.cpp](https://github.com/ggml-org/llama.cpp) as an inference engine.
- Can generate/execute SQLs without externally providing schema or table information.
- Higher-level security through 'generate_only' at API call to prevent the NLQuery engine to execute the generated SQL.
- Easy to use single HTTP REST API endpoint named as `nlquery`.
- HTTPS support through OpenSSL.
- Simple web UI for testing purposes.
- MCP server support for MCP client integration.
- CUDA acceleration by default if NVIDIA GPU is found

## Platforms

- Windows (Not well-tested)
- Linux
- MacOS (Apple Chip, Metal)

## Installation

> [!IMPORTANT]
> This installation is for Linux. for Windows and macOS users, refer to compiling from source.

Setting up the MBASE apt repository:
```bash
sudo curl -fsSL https://repo.mbasesoftware.com/apt/pgp-key.public -o /etc/apt/keyrings/mbase-apt.public
echo "deb [arch=amd64 signed-by=/etc/apt/keyrings/mbase-apt.public] https://repo.mbasesoftware.com/apt/ stable main" | sudo tee /etc/apt/sources.list.d/mbase.list > /dev/null
sudo apt-get update
```

Installing the NLQuery:

```bash
sudo apt-get -y install mbase-nlquery
```

## Compiling From Source

In order to compile from source, you will need the following packages:

- [MBASE SDK](https://github.com/Emreerdog/mbase)
- [PostgreSQL](https://www.postgresql.org) libraries
- Optional: [OpenSSL](https://openssl-library.org) if HTTPS is desired

After you satisfy the requirements, clone the repository:

```bash
git clone https://github.com/Emreerdog/mbase_nlquery
cd mbase_nlquery
```

There are two CMake configuration parameters user can specify given as:

- `MBASE_NLQUERY_PROGRAM_PATH`(default=`${CMAKE_CURRENT_BINARY_DIR}/nlquery`): NLQuery will use this path to download the [Qwen2.5-7B-Instruct-NLQuery](https://huggingface.co/MBASE/Qwen2.5-7B-Instruct-NLQuery) and install the web application html.

- `MBASE_NLQUERY_SSL`(default=`OFF`): If set, it will compile the application with HTTPS support.

Now, we will build the program:

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release -j
```

After the compilation is finished, you can run the program:

```bash
./mbase_nlquery
```

## Usage

### Running without configuration

```bash
mbase_nl_query
```

### Running with configuration

```bash
mbase_nl_query --hostname localhost --port 8080 --user-count 4 --max-rows 1000 --disable-webui 
```


### Printing Help

```bash
mbase_nl_query --help
```

## Single-api Endpoint

- API Endpoint: `/nlquery`
- Example Full API Endpoint: `http://localhost:8080/nlquery`
- Content-Type: application/json
- Method: POST

### Request Body

```js
{
    "db_provider" : "PostgreSQL", // case-insensitive
    "db_hostname" : "#host_here",
    "db_port" : #port,
    "db_name" : "#database",
    "db_username" : "#username",
    "db_password" : "#password",
    "query" : "#Your prompt",
    "sql_history" : "#response_history", // Optional
    "generate_only": true | false // Optional, default is true
}
```

### Response Body On Success (Reading data)

```js
{
    "status" : 0,
    "sql" : "#generated_sql_here",
    "data" : {
        "#col_name_1" : [ #row_1, #row_2, ... #row_n ],
        "#col_name_2" : [ #row_1, #row_2, ... #row_n ],
        ...
        "#col_name_n" : [ #row_1, #row_2, ... #row_n ]
    }
}
```

### Response Body On Success (Modifying the DB or generate_only flag is set):

```js
{
    "status" : 0,
    "sql" : "#generated_sql_here",
}
```

### Response Body On Fail:

```js
{
    "status" : #status_code,
    "message" : "#error_message",
    "data": "#sql" // This key exists if the engine generated an SQL but failed to execute it
}
```

### Response status codes and messages

| Status | Message                                                                                                 |
| ------ | ------------------------------------------------------------------------------------------------------- |
| 0      |                                                                                                         |
| 1      | NLQuery engine is overloaded. Try again later                                                           |
| 2      | Database connection failed                                                                              |
| 3      | Given query is invalid. Make sure it is natural language and its context is related to the SQL database |
| 4      | Internal server error. Try again later                                                                  |
| 5      | Message body is invalid. Make sure you populate the mandatory fields correctly                          |
| 6      | Given database provider is not supported                                                                |
| 7      | Database failed to execute the generated query                                                          |
| 8      | Given prompt is too long. This may also happen if the provided sql_history is too long                  |
| 9      | Too much data returned from the database                                                                |

## Security

### Disclaimer!

#### Use with care!

The NLQuery engine is capable of executing CRUD operations on your database. To mitigate potential risks and prevent unintended modifications, ensure that users have restricted privileges and proper access control mechanisms in place.

#### Data Privacy

Neither the NLQuery API nor the web interface stores any user-related data, query history, or session information on server side application(mbase_nlquery). mbase_nlquery do not monitor, log, or retain any generated SQL queries. To enhance response quality, your generated SQL history is temporarily stored only in a client-side variable within your browser if the webui option enabled. This data is not saved in local storage, cookies, or any persistent storage, and it is lost when the page is refreshed or closed. However, since the application is open-source this behavior can be altered by manual code implementations.

#### Liability Disclaimer

We are not responsible for any unintended modifications, data loss, or security risks resulting from the use of this application. Users are solely responsible for managing their database permissions and the result of the SQL query that has been executed on your database. Use at your own risk.

## Contact

If you have any question, idea, want information or, if you want to be active in MBASE NLQuery project, send us an email at erdog@mbasesoftware.com
