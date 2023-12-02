cmake_minimum_required(VERSION 3.26)

set(PICO_BOARD pico_w)
set(FREERTOS_PORT GCC_RP2040)
include(vendor/pico-sdk/pico_sdk_init.cmake)
include(vendor/freertos/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

project(alert2040
    HOMEPAGE_URL https://github.com/crazybolillo/alert2040
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

pico_sdk_init()

include_directories(config)

add_executable(alert2040
    main.c
)
pico_enable_stdio_usb(alert2040 1)
pico_enable_stdio_uart(alert2040 0)
target_compile_definitions(alert2040 PRIVATE
    PICO_STDIO_ENABLE_CRLF_SUPPORT=0
    PICO_STDIO_USB_DEFAULT_CRLF=0
    PICO_STDIO_DEFAULT_CRLF=0
)

target_link_libraries(alert2040
    pico_stdlib
    hardware_adc
    hardware_timer
    pico_cyw43_arch_lwip_sys_freertos
    FreeRTOS-Kernel-Heap4
)
pico_add_extra_outputs(alert2040)
