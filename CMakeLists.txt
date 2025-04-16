cmake_minimum_required(VERSION 3.4)
   
project("mbase_nlquery" LANGUAGES CXX)

add_executable(mbase_nlquery main.cpp)

find_package(mbase.libs REQUIRED COMPONENTS std inference json)
find_package(OpenSSL REQUIRED)
find_package(PostgreSQL REQUIRED)

target_compile_features(mbase_nlquery PUBLIC cxx_std_20)
target_link_libraries(mbase_nlquery PRIVATE mbase-std mbase-inference mbase-json PostgreSQL::PostgreSQL)
target_include_directories(mbase_nlquery PUBLIC mbase-std mbase-inference mbase-json PostgreSQL::PostgreSQL)