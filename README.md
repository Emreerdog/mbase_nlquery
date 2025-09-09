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

MBASE NLQuery is a natural language to SQL generator/executor engine using the [MBASE SDK](https://github.com/Emreerdog/mbase) as an LLM SDK. 

It internally uses the [Qwen2.5-7B-Instruct-NLQuery](https://huggingface.co/MBASE/Qwen2.5-7B-Instruct-NLQuery) model to convert the provided natural language into SQL queries and executes it through the database client SDKs (PostgreSQL only for now). However, the execution can be disabled for security.

MBASE NLQuery doesn't require the user to supply a table information on the database. User only needs to supply mundane parameters such as: database address, scheman name, port, username, password etc.

It serves a single HTTP REST API endpoint called `nlquery` which can serve to multiple users at the same time and it requires a super-simple JSON formatted data to call.

## Features

- Offline Text-To-SQL generation/executor engine using [Qwen2.5-7B-Instruct-NLQuery](https://huggingface.co/MBASE/Qwen2.5-7B-Instruct-NLQuery) model with [MBASE SDK](https://github.com/Emreerdog/mbase) as an LLM SDK which internally uses [llama.cpp](https://github.com/ggml-org/llama.cpp) as an inference engine.
- Can generate/execute SQLs without externally providing table information.
- Higher-level security through 'generate_only' at API call to prevent the NLQuery engine to execute the generated SQL.
- Easy to use single HTTP REST API endpoint named as `nlquery`.
- HTTPS support through OpenSSL.
- Simple web UI for testing purposes.
- CUDA acceleration by default if NVIDIA GPU is found
- Extra context information support through `--hint-file` option on program startup.
- Turn your Text-to-Text GGUF model into NLQuery engine compatible format through prompt cooker.

## Platforms

- Windows
- Linux
- MacOS

## Build

In order to build from source, you will need the following packages:

- [MBASE SDK](https://docs.mbasesoftware.com/setting-up/download.html)
- [PostgreSQL](https://www.postgresql.org) client libraries
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
./mbase_nlquery --hostname localhost --port 8080 --user-count 4 --max-rows 1000 --disable-webui 
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
      <img alt="NLQuery Diagram" alt="nlquery_diag_dark" src="https://github.com/user-attachments/assets/2befff01-84da-4046-9adc-3d6d2e19f928" />
    </picture>
  </a>
</div>

## Performance Benchmark

The experiment is simple, we start the NLQuery engine and invoke the `/nlquery` endpoint with various queries and observe in average how long it takes in seconds to get a response.

The postgresql database we use in this benchmark is the [dvdrental](https://neon.com/postgresql/postgresql-getting-started/postgresql-sample-database) database.

#### Environment Information

- **Name**: Mac Mini M4
- **Model Name**: A3238
- **RAM**: 16 GB
- **CPU**: 10 Core M4
- **GPU**: 10 Core M4
- **Compute Framework**: Metal
- **LLM**: [Qwen 2.5 7B Instruct NLQuery 8-bit quantization](https://huggingface.co/MBASE/Qwen2.5-7B-Instruct-NLQuery) 


#### Results

| Natural Language                                                                                      | Generated Query                                                                                                                                                                                                 | Time to Respond (in seconds) |
|-------------------------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|------------------------------|
| How many tables are there?                                                                            | SELECT count(*) FROM information_schema.tables WHERE table_schema = 'public'                                                                                                                                    | 1.44                         |
| Show me the names of all tables.                                                                      | \nSELECT table_name \nFROM information_schema.tables \nWHERE table_schema = 'public';\n                                                                                                                         | 1.90                         |
| Show me all actors, sort it by their id in an ascending order.                                        | \nSELECT first_name, last_name, actor_id\nFROM actor\nORDER BY actor_id ASC;\n                                                                                                                                  | 2.15                         |
| Show me all actors, who played in at least 30 movies                                                  | \nSELECT a.actor_id, a.first_name, a.last_name\nFROM actor a\nJOIN film_actor fa ON a.actor_id = fa.actor_id\nGROUP BY a.actor_id, a.first_name, a.last_name\nHAVING COUNT(fa.film_id) >= 30;\n                 | 5.18                         |
| Remove all actors whose name is John                                                                  | DELETE FROM actor WHERE first_name = 'John' OR last_name = 'John';                                                                                                                                              | 1.66                         |
| What are the last 10 rented movies and how many people rented them?                                   | \nSELECT r.rental_id, r.customer_id, COUNT(*) AS num_people_rented\nFROM rental r\nGROUP BY r.rental_id, r.customer_id\nORDER BY r.rental_date DESC\nLIMIT 10;\n                                                | 4.5                          |
| What are the last 10 rented movies and how many people rented them? Show me the names of those movies | \nSELECT f.title, COUNT(*) AS rental_count\nFROM rental r\nJOIN inventory i ON r.inventory_id = i.inventory_id\nJOIN film f ON i.film_id = f.film_id\nGROUP BY f.title\nORDER BY rental_count DESC\nLIMIT 10;\n | 5.03                         |
| What are the fields of movie table?                                                                   | \nSELECT column_name \nFROM information_schema.columns \nWHERE table_name = 'film';\n                                                                                                                           | 1.59                         |

## Hint File

> [!IMPORTANT]  
> The text inside the hint file will be KV-Cached into the LLM which implies that there won't be any performance degregation regardless of the size of your text in the hint file besides longer program startup.

User can provide specialized contextual information about the database structure, how tables relate to each other, distinct question-answer pairs etc. in a text file. The given text file can be provided to NLQuery engine at the program startup for better contextual understanding of your database for the LLM.

We can provide a hint file for the popular [dvdrental](https://neon.com/postgresql/postgresql-getting-started/postgresql-sample-database) postgresql sample database

The text below can be provided as an hint to NLQuery engine for better understanding about the context (text is taken from the [dvdrental](https://neon.com/postgresql/postgresql-getting-started/postgresql-sample-database) site):
```txt
There are 15 tables in the DVD Rental database:

actor – stores actor data including first name and last name.
film – stores film data such as title, release year, length, rating, etc.
film_actor – stores the relationships between films and actors.
category – stores film’s categories data.
film_category- stores the relationships between films and categories.
store – contains the store data including manager staff and address.
inventory – stores inventory data.
rental – stores rental data.
payment – stores customer’s payments.
staff – stores staff data.
customer – stores customer data.
address – stores address data for staff and customers
city – stores city names.
country – stores country names.
```

Then, we can store this text in the `sample.txt` file and give it as an hint file at the program startup:

```bash
mbase_nlquery ... --hint-file sample.txt
```

Which will start the engine with better contextual understanding of your database.

## Prompt Cooking

> [!IMPORTANT]  
> Prompt cooker doesn't modify model weights. It only applies a configuration to the gguf file's metadata section.

> [!CAUTION]  
> Do not close the application while cooking the system prompt! If so, model file will be corrupted and unusable!

Using the `mbase_nlquery_cooker` program, you can configure your Text-to-Text LLM into NLQuery engine compatible LLM.

What prompt cooker does is that it tokenizes the given text file and store the token information in the .gguf file metadata.
So, when the program starts, NLQuery engine reads those tokens from the GGUF file and KV-Caches into the LLM. 

Here is the usage:

```bash
Description: Embedding the generated tokens of the given prompt to the .gguf file
Usage: mbase_nlquery_cooker <nlquery_prompt_path> <model.gguf>
```

## Security

### Disclaimer!

#### Use with care!

The NLQuery engine is capable of executing CRUD operations on your database. To mitigate potential risks and prevent unintended modifications, ensure that users have restricted privileges and proper access control mechanisms in place.

#### Liability Disclaimer

We are not responsible for any unintended modifications, data loss, or security risks resulting from the use of this application. Users are solely responsible for managing their database permissions and the result of the SQL query that has been executed on your database. Use at your own risk.

## Contact

If you have any question, idea, want information or, if you want to be active in MBASE NLQuery project, send us an email at erdog@mbasesoftware.com
