cmake_minimum_required(VERSION 3.30)
   
project("mbase_nlquery" LANGUAGES CXX)

if(NOT MBASE_NLQUERY_PROGRAM_PATH)
    if(MBASE_NLQUERY_RELEASE)
        set(MBASE_NLQUERY_PROGRAM_PATH ~/.nlquery)
    else()
        set(MBASE_NLQUERY_PROGRAM_PATH ${CMAKE_CURRENT_BINARY_DIR}/nlquery)
    endif()
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/web/index.html ${MBASE_NLQUERY_PROGRAM_PATH}/web/index.html COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/web/mbase_nlquery_logo.svg ${MBASE_NLQUERY_PROGRAM_PATH}/web/mbase_nlquery_logo.svg COPYONLY)

add_executable(mbase_nlquery main.cpp)

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

if(MBASE_NLQUERY_RELEASE)
    include(GNUInstallDirs)

    install(
        FILES 
            ${CMAKE_CURRENT_SOURCE_DIR}/web/index.html
            ${CMAKE_CURRENT_SOURCE_DIR}/web/mbase_nlquery_logo.svg
        PERMISSIONS 
            OWNER_READ
            OWNER_WRITE
            GROUP_READ
            WORLD_READ
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/nlquery
    )
endif()