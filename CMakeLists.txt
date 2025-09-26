cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_SDK_FETCH_FROM_GIT ON)
set(PICO_SDK_FETCH_FROM_GIT_TAG 2.1.1)

include(pico_sdk_import.cmake)

project("pico-ident" VERSION "2.1.0")

pico_sdk_init()

add_executable(pico-ident
  src/AT24CM02.cpp
  src/main.cpp
)

target_link_libraries("${PROJECT_NAME}"
  pico_stdlib
  pico_unique_id
  hardware_flash
  hardware_gpio
  hardware_i2c
)

target_compile_features("${PROJECT_NAME}" PUBLIC cxx_std_20)

if (DEFINED MIN_PULSE_WIDTH_US)
  if (MIN_PULSE_WIDTH_US GREATER_EQUAL 50000)
    target_compile_definitions("${PROJECT_NAME}" PRIVATE
      "-DCONFIG_MIN_PULSE_WIDTH_US=${MIN_PULSE_WIDTH_US}"
    )
  else()
    message(FATAL_ERROR
      "Invalid minimum pulse width: ${MIN_PULSE_WIDTH_US}\n"
      "MIN_PULSE_WIDTH_US must be a number >= 50000."
    )
  endif()
endif()

option(USB_SERIAL "Use USB serial instead of UART" OFF)

# add -DUSB_SERIAL=ON when configuring to use USB (9600 baud) instead of UART
if(USB_SERIAL)
  message(STATUS "Using USB serial")
  # enable USB output, disable UART
  pico_enable_stdio_usb("${PROJECT_NAME}" 1)
  pico_enable_stdio_uart("${PROJECT_NAME}" 0)
else()
  message(STATUS "Using UART")
endif()

pico_set_program_url("${PROJECT_NAME}"
  "https://github.com/BloomyControls/pico-ident")
pico_set_program_version("${PROJECT_NAME}" "${PROJECT_VERSION}")

pico_add_extra_outputs("${PROJECT_NAME}" unique_board_id)
