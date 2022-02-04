cmake_minimum_required(VERSION 3.10)

project(tester  VERSION 1.0
                DESCRIPTION "Tester for Juice - battery charging and monitoring process"
)

add_executable(${CMAKE_PROJECT_NAME}
   ${PROJECT_SOURCE_DIR}/main.cpp
)

find_package(OpenSSL REQUIRED)
find_library(paho-mqttpp3 NAMES libpaho-mqttpp3.a REQUIRED)
find_library(paho-mqtt3a NAMES libpaho-mqtt3a.so REQUIRED)

target_link_libraries(${CMAKE_PROJECT_NAME} paho-mqtt3a paho-mqttpp3)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

