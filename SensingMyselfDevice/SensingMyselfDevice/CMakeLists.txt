#  Copyright (c) Microsoft Corporation. All rights reserved.
#  Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)

project(SensingMyself C)
 
azsphere_configure_tools(TOOLS_REVISION "20.04")
azsphere_configure_api(TARGET_API_SET "5+Beta2004")

add_executable(${PROJECT_NAME} main.c azure_iot_utilities.c parson.c max30102.c oledc_driver.c oledc_hal.c algorithm_by_RF.c )
target_include_directories(${PROJECT_NAME} PUBLIC ${AZURE_SPHERE_API_SET_DIR}/usr/include/azureiot)
target_compile_definitions(${PROJECT_NAME} PUBLIC AZURE_IOT_HUB_CONFIGURED)
target_link_libraries(${PROJECT_NAME} m azureiot applibs pthread gcc_s c)

target_compile_options(${PROJECT_NAME} PRIVATE -w)

azsphere_target_add_image_package(${PROJECT_NAME})
