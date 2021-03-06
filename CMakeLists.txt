cmake_minimum_required(VERSION 3.10)

project(spinet VERSION 0.0.1)

if(MSVC)
    add_compile_options("/utf-8")
endif()



#--------------------for the cmake settings start--------------------
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)
#--------------------for the cmake settings end--------------------



#--------------------for the options start--------------------
option(NO_EXAMPLES "Build without example programs" OFF)
option(ENABLE_ASAN "Enable the address sanitizer" OFF)
message("")
message("SETTINGS:")
message("NO_EXAMPLES=${NO_EXAMPLES}")
message("ENABLE_ASAN=${ENABLE_ASAN}")
message("")
#--------------------for the options end--------------------



#--------------------for the compiler start--------------------
set(CMAKE_CXX_STANDARD 17)

add_compile_options(-Wall)
add_compile_options(-Wextra)

if(ENABLE_ASAN)
    add_compile_options(-fno-omit-frame-pointer)
    add_compile_options(-fsanitize=address)
endif()
#--------------------for the compiler end--------------------



#--------------------for the public include directories start--------------------
set(PUBLIC_DEPENDENCIES ${PROJECT_SOURCE_DIR}/include)
include_directories("${PUBLIC_DEPENDENCIES}")
#--------------------for the public include directories end--------------------



#--------------------for the dependencies start--------------------
find_package(Threads REQUIRED)
#--------------------for the dependencies end--------------------



#--------------------for the macro definitions start--------------------
#--------------------for the macro definitions end--------------------



#--------------------for the lib start--------------------
file(GLOB_RECURSE SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)
set(LIB_DEPENDENCIES ${PROJECT_SOURCE_DIR}/third_party ${PROJECT_SOURCE_DIR}/src)

add_library(spinet_objlib OBJECT ${SOURCES})
target_include_directories(spinet_objlib PUBLIC "${LIB_DEPENDENCIES}")
set_target_properties(spinet_objlib PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

add_library(spinet_shared SHARED $<TARGET_OBJECTS:spinet_objlib>)
target_link_libraries(spinet_shared Threads::Threads)
set_target_properties(spinet_shared PROPERTIES OUTPUT_NAME spinet)

add_library(spinet_static STATIC $<TARGET_OBJECTS:spinet_objlib>)
target_link_libraries(spinet_static Threads::Threads)
set_target_properties(spinet_static PROPERTIES OUTPUT_NAME spinet POSITION_INDEPENDENT_CODE TRUE)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ 
DESTINATION ${PROJECT_SOURCE_DIR}/release)

install(TARGETS spinet_shared
LIBRARY DESTINATION ${PROJECT_SOURCE_DIR}/release/lib/
)
install(TARGETS spinet_static
ARCHIVE DESTINATION ${PROJECT_SOURCE_DIR}/release/lib/
)
#--------------------for the lib end--------------------



#--------------------for the examples start--------------------
if(NOT NO_EXAMPLES)
    add_executable(cmd_echo_client ${PROJECT_SOURCE_DIR}/example/echo/cmd_client.cpp)
    target_include_directories(cmd_echo_client PUBLIC ${PROJECT_SOURCE_DIR}/example/include)
    target_compile_options(cmd_echo_client PUBLIC -Wno-unused-parameter)

    add_executable(async_echo_server ${PROJECT_SOURCE_DIR}/example/echo/async/server.cpp)
    target_include_directories(async_echo_server PUBLIC ${PROJECT_SOURCE_DIR}/example/include)
    target_compile_options(async_echo_server PUBLIC -Wno-unused-parameter)
    target_link_libraries(async_echo_server spinet_static Threads::Threads)

    add_executable(async_echo_client ${PROJECT_SOURCE_DIR}/example/echo/async/client.cpp)
    target_include_directories(async_echo_client PUBLIC ${PROJECT_SOURCE_DIR}/example/include)
    target_compile_options(async_echo_client PUBLIC -Wno-unused-parameter)
    target_link_libraries(async_echo_client spinet_static Threads::Threads)

    add_executable(async_echo_client2 ${PROJECT_SOURCE_DIR}/example/echo/async/client2.cpp)
    target_include_directories(async_echo_client2 PUBLIC ${PROJECT_SOURCE_DIR}/example/include)
    target_compile_options(async_echo_client2 PUBLIC -Wno-unused-parameter)
    target_link_libraries(async_echo_client2 spinet_static Threads::Threads)

    add_executable(http_server_simple ${PROJECT_SOURCE_DIR}/example/http/simple/server.cpp)
    target_include_directories(http_server_simple PUBLIC ${PROJECT_SOURCE_DIR}/example/include)
    target_include_directories(http_server_simple PUBLIC ${PROJECT_SOURCE_DIR}/example/http/include)
    target_compile_options(http_server_simple PUBLIC -Wno-unused-parameter)
    target_link_libraries(http_server_simple spinet_static Threads::Threads)

    add_executable(http_server_keep_alive ${PROJECT_SOURCE_DIR}/example/http/keep_alive/server.cpp)
    target_include_directories(http_server_keep_alive PUBLIC ${PROJECT_SOURCE_DIR}/example/include)
    target_include_directories(http_server_keep_alive PUBLIC ${PROJECT_SOURCE_DIR}/example/http/include)
    target_compile_options(http_server_keep_alive PUBLIC -Wno-unused-parameter)
    target_link_libraries(http_server_keep_alive spinet_static Threads::Threads)

    add_executable(address_resolve ${PROJECT_SOURCE_DIR}/example/address/resolve.cpp)
    target_include_directories(address_resolve PUBLIC ${PROJECT_SOURCE_DIR}/example/include)
    target_compile_options(address_resolve PUBLIC -Wno-unused-parameter)
    target_link_libraries(address_resolve spinet_static Threads::Threads)
endif()
#--------------------for the examples end--------------------