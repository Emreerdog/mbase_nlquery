<div align="center">
  <a href="#">
    <picture>
      <source srcset="https://github.com/user-attachments/assets/b927d271-35db-4ec0-a9dd-2980f676863f" media="(prefers-color-scheme: dark)">
      <source srcset="https://github.com/user-attachments/assets/310ad545-e090-4c65-b7dc-9dc821c01ca2" media="(prefers-color-scheme: light)">
      <img alt="MBASE" height="325px" src="https://github.com/user-attachments/assets/b927d271-35db-4ec0-a9dd-2980f676863f">
    </picture>
  </a>
</div>
<br>

# MBASE NLQuery

MBASE NLQuery is a natural language to SQL generator/executor engine using the [MBASE SDK](https://github.com/Emreerdog/mbase) as an LLM inference SDK. 

It internally uses the [Qwen2.5-7B-Instruct-NLQuery](https://huggingface.co/MBASE/Qwen2.5-7B-Instruct-NLQuery) model to convert the provided natural language into SQL queries and executes it through the database client SDKs (PostgreSQL only for now). However, the execution can be disabled for security.

MBASE NLQuery doesn't require the user to supply a table information on the database. User only needs to supply mundane parameters such as: database address, scheman name, port, username, password etc.

It serves a single HTTP REST API endpoint called `nlquery` which can serve to multiple users at the same time and it requires a super-simple JSON formatted data to call.

## Features

- Offline Text-To-SQL generation/executor engine using [Qwen2.5-7B-Instruct-NLQuery](https://huggingface.co/MBASE/Qwen2.5-7B-Instruct-NLQuery) model with [MBASE SDK](https://github.com/Emreerdog/mbase) as an inference SDK which internally uses [llama.cpp](https://github.com/ggml-org/llama.cpp) as an inference engine.
- Can generate/execute SQLs without externally providing table information.
- Higher-level security through 'generate_only' at API call to prevent the NLQuery engine to execute the generated SQL.
- Easy to use single HTTP REST API endpoint named as `nlquery`.
- HTTPS support through OpenSSL.
- Simple web UI for testing purposes.
- CUDA acceleration by default if NVIDIA GPU is found

## Platforms

- Windows
- Linux
- MacOS

## Build

In order to build from source, you will need the following packages:

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
cmake -B build
cmake --build build --config Release -j
```

After the compilation is finished, go to the build directory

```bash
cd build
```

## Usage

### Printing Help

```bash
./mbase_nlquery --help
```

### Running without configuration

```bash
./mbase_nlquery
```

### Running with configuration

```bash
mbase_nlquery --hostname localhost --port 8080 --user-count 4 --max-rows 1000 --disable-webui 
```

## Single-api Endpoint

- API Endpoint: `/nlquery`
- Example Full API Endpoint: `http://localhost:8080/nlquery`
- Content-Type: application/json
- Method: POST

### Request Body

```js
{
    "db_username" : "#username", // Optional if --force-credentials is not set
    "db_password" : "#password", // Optional if --force-credentials is not set
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
| 0      | Success                                                                                                 |
| 1      | NLQuery engine is overloaded. Try again later                                                           |
| 2      | Database connection failed                                                                              |
| 3      | Given query is invalid. Make sure it is natural language and its context is related to the SQL database |
| 4      | Internal server error. Try again later                                                                  |
| 5      | Message body is invalid. Make sure you populate the HTTP body correctly.                                |
| 6 (ignore)      | Given database provider is not supported                                                       |
| 7      | Database failed to execute the generated query                                                          |
| 8      | Given prompt is too long. This may also happen if the provided sql_history is too long                  |
| 9      | Too much data returned from the database, specify the --max-rows option at program startup              |

## NLQuery Schema

<div align="center">
  <a href="#">
    <picture>
      <source srcset="https://github.com/user-attachments/assets/2befff01-84da-4046-9adc-3d6d2e19f928" media="(prefers-color-scheme: dark)">
      <source srcset="https://github.com/user-attachments/assets/5839f322-bc4a-4163-a995-cc16d84b4633" media="(prefers-color-scheme: light)">
      <img alt="NLQuery Diagram" width="970" height="486" alt="nlquery_diag_dark" src="https://github.com/user-attachments/assets/2befff01-84da-4046-9adc-3d6d2e19f928" />
    </picture>
  </a>
</div>

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
