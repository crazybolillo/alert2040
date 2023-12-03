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

add_compile_definitions("NDEBUG=1")

set(API_IPV4_ADDR "" CACHE STRING "IPV4 address to use for IOT functionality")
set(API_PORT "8080" CACHE STRING "TCP port used to connect with the IOT service")
set(WIFI_TIMEOUT "10000" CACHE STRING "Milliseconds to wait for a WiFi connection to be established")

if (NOT API_IPV4_ADDR)
    message(FATAL_ERROR "The API's IPV4 address must be specified")
endif ()

if (NOT WIFI_SSID)
    message(FATAL_ERROR "WiFi's SSID must be specified")
endif ()

if (NOT WIFI_PSK)
    message(FATAL_ERROR "WiFi's PSK must be specified")
endif ()

string(REGEX MATCH "([0-9]+)\.([0-9]+)\.([0-9]+)\.([0-9]+)" IPV4_ADDR_OUT ${API_IPV4_ADDR})
add_compile_definitions("API_IPV4_B1=${CMAKE_MATCH_1}")
add_compile_definitions("API_IPV4_B2=${CMAKE_MATCH_2}")
add_compile_definitions("API_IPV4_B3=${CMAKE_MATCH_3}")
add_compile_definitions("API_IPV4_B4=${CMAKE_MATCH_4}")

add_compile_definitions("WIFI_TIMEOUT=${WIFI_TIMEOUT}")
add_compile_definitions("WIFI_SSID=\"${WIFI_SSID}\"")
add_compile_definitions("WIFI_PSK=\"${WIFI_PSK}\"")

add_compile_definitions("API_IPV4=\"${API_IPV4_ADDR}\"")
add_compile_definitions("API_PORT=${API_PORT}")

include_directories(config)

add_executable(alert2040
    main.c
        http.c
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
