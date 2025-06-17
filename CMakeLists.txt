cmake_minimum_required(VERSION 3.30)
   
project("mbase_nlquery" LANGUAGES CXX)

if(NOT MBASE_NLQUERY_PROGRAM_PATH)
    set(MBASE_NLQUERY_PROGRAM_PATH ${CMAKE_CURRENT_BINARY_DIR}/nlquery)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/web/index.html ${MBASE_NLQUERY_PROGRAM_PATH}/web/index.html COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/web/mbase_nlquery_logo.svg ${MBASE_NLQUERY_PROGRAM_PATH}/web/mbase_nlquery_logo.svg COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/web/bootstrap.bundle.min.js ${MBASE_NLQUERY_PROGRAM_PATH}/web/bootstrap.bundle.min.js COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/web/bootstrap.min.css ${MBASE_NLQUERY_PROGRAM_PATH}/web/bootstrap.min.css COPYONLY)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/global_state.h.in ${CMAKE_CURRENT_SOURCE_DIR}/global_state.h)

add_executable(mbase_nlquery main.cpp)
add_executable(mbase_nlquery_cooker prompt_cooker.cpp)

find_package(mbase.libs REQUIRED COMPONENTS std inference json)

if(WIN32)
    set(OPENSSL_MSVC_STATIC_RT "TRUE")
endif()

set(OPENSSL_USE_STATIC_LIBS "TRUE")

if(MBASE_NLQUERY_SSL STREQUAL "ON")
    find_package(OpenSSL REQUIRED)
    target_compile_definitions(mbase_nlquery PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)
    target_link_libraries(mbase_nlquery PRIVATE ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
    target_include_directories(mbase_nlquery PUBLIC ${OPENSSL_INCLUDE_DIR})
endif()

find_package(PostgreSQL REQUIRED)

target_compile_features(mbase_nlquery PUBLIC cxx_std_20)
target_link_libraries(mbase_nlquery PRIVATE mbase-std mbase-inference mbase-json PostgreSQL::PostgreSQL)
target_include_directories(mbase_nlquery PUBLIC mbase-std mbase-inference mbase-json PostgreSQL::PostgreSQL)

target_compile_features(mbase_nlquery_cooker PUBLIC cxx_std_20)
target_link_libraries(mbase_nlquery_cooker PRIVATE mbase-inference)
target_include_directories(mbase_nlquery_cooker PUBLIC mbase-inference)