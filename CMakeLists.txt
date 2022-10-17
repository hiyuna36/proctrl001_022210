cmake_minimum_required(VERSION 3.24)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

#Set the project name, the programming language
project(procotrltest001 C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
#project(testusb001)

#project(testusb001)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()


# rest of your project

add_executable(${PROJECT_NAME}
        main.cpp
        usb_descriptors.c
        )

# include dir
include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# pull in common dependencies
target_link_libraries(${PROJECT_NAME} pico_stdlib tinyusb_device tinyusb_board)



#Enable USB output, disable UART output
pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)

# create map/bin/hex file etc.
pico_add_extra_outputs(${PROJECT_NAME})

# add url via pico_set_program_url
#example_auto_set_url(test001)
