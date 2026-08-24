cmake_minimum_required(VERSION 3.22)
project(CommonDrivers)
enable_language(C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
message(STATUS "Resolving dependencies...")

message(STATUS "Resolving bmi08 sensors api dependency...")
FetchContent_Declare(bmi08
  GIT_REPOSITORY https://github.com/boschsensortec/BMI08x_SensorAPI.git
  GIT_SHALLOW TRUE
  GIT_TAG master
)
FetchContent_MakeAvailable(bmi08)

message(STATUS "Resolving bmp5 sensors api dependency...")
FetchContent_Declare(bmp5
  GIT_REPOSITORY https://github.com/boschsensortec/BMP5_SensorAPI.git
  GIT_SHALLOW TRUE
  GIT_TAG master
)
FetchContent_MakeAvailable(bmp5)

message(STATUS "Resolving littlefs dependency...")
FetchContent_Declare(littlefs
  GIT_REPOSITORY https://github.com/littlefs-project/littlefs.git
  GIT_SHALLOW TRUE
  GIT_TAG master
)
FetchContent_MakeAvailable(littlefs)

message(STATUS "Resolving gtest dependency...")
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.18.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

message(STATUS "Resolving fff dependency...")
FetchContent_Declare(
  fff
  GIT_REPOSITORY https://github.com/meekrosoft/fff.git
  GIT_TAG master
)

add_library(common_drivers STATIC)
if(TARGET stm32cubemx)
  message(STATUS "Compiling for target host (MCU), linking libraries...")

  if(NOT TARGET STM32_Drivers)
    message(FATAL_ERROR "STM32_Drivers source was not added, something is wrong!")
  endif()
  get_target_property(STM32_Drivers_Src STM32_Drivers SOURCES)

  # Autodetect series
  # STM32 includes a file of the form (depending on the series)
  #   ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/system_stm32f4xx.c
  # to sources, which we can use regex detection to get the series
  string(REGEX MATCH "stm32([a-z0-9]+)\\.c" FOUND_MATCH "${STM32_Drivers_Src}")
  if(FOUND_MATCH)
    # We then can use use this `HAL_STM32_SERIES` in
    # hal.h to automatically include the right hal file.
    message(STATUS "Detected " ${CMAKE_MATCH_1} " series...")
    target_compile_definitions(common_drivers PUBLIC HAL_STM32_SERIES=${CMAKE_MATCH_1})
  else()
    message(FATAL_ERROR "Unable to autodetect STM32 series!")
  endif()

  # Autodetect perhipherals
  # What this basically does is conditionally include the i2c, qspi, spi, and uart
  # files if and only if they are enabled in cubemx. 
  #
  # When cubemx enables the perhipherals, it adds something like this
  #   ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_spi.c
  # to STM32_Drivers_Src. Therefore we can just iterate through it and find if the
  # given drivers exist, and if they do, we can safely include our thin HAL abstraction
  # for the driver.
  set(PATTERNS
    "_hal_i2c\\.c"
    "_hal_qspi\\.c"
    "_hal_spi\\.c"
    "_hal_uart\\.c"
  )
  set(DEFINES
    I2C
    QSPI
    SPI
    UART
  )

  foreach(PATTERN DEFINE IN ZIP_LISTS PATTERNS DEFINES)
    string(REGEX MATCH "${PATTERN}" FOUND_MATCH "${STM32_Drivers_Src}")
    if(FOUND_MATCH)
      message(STATUS "Adding " ${DEFINE} " to build...")
      set_property(GLOBAL APPEND PROPERTY PERHIPHERALS "${DEFINE}")
    endif()
  endforeach()

  # Add libaries
  add_library(bmi08 STATIC
    ${bmi08_SOURCE_DIR}/bmi08a.c
    ${bmi08_SOURCE_DIR}/bmi08g.c
    ${bmi08_SOURCE_DIR}/bmi08xa.c
    ${bmi08_SOURCE_DIR}/bmi088_mma.c
    ${bmi08_SOURCE_DIR}/bmi088_anymotiona.c
  )
  target_include_directories(bmi08 PUBLIC ${bmi08_SOURCE_DIR})

  add_library(bmp5 STATIC ${bmp5_SOURCE_DIR}/bmp5.c)
  target_include_directories(bmp5 PUBLIC ${bmp5_SOURCE_DIR})

  add_library(littlefs STATIC
    ${littlefs_SOURCE_DIR}/lfs.c
    ${littlefs_SOURCE_DIR}/lfs_util.c
  )
  target_include_directories(littlefs PUBLIC ${littlefs_SOURCE_DIR})

  message(STATUS "Linking dependencies to library...")
  target_link_libraries(common_drivers PUBLIC bmi08 bmp5 littlefs)

  message(STATUS "Linking stm32 drivers to library....")
  target_link_libraries(common_drivers PUBLIC stm32cubemx)
else()
  enable_testing()
  message(STATUS "Compiling for tests, linking gtest, fff, and mocks to library...")
  FetchContent_MakeAvailable(googletest)
  FetchContent_MakeAvailable(fff)

  add_library(bmp5 INTERFACE)
  target_include_directories(bmp5 INTERFACE ${bmp5_SOURCE_DIR})

  add_library(littlefs INTERFACE)
  target_include_directories(littlefs INTERFACE ${littlefs_SOURCE_DIR})

  add_library(bmi08 INTERFACE)
  target_include_directories(bmi08 INTERFACE ${bmi08_SOURCE_DIR})

  message(STATUS "Linking mock dependencies to library...")
  target_link_libraries(common_drivers PUBLIC bmp5 littlefs bmi08)

  message(STATUS "Creating tests for library...")
  add_compile_definitions(TEST)
  add_subdirectory(tests)
endif()

message(STATUS "Adding source files and headers to library...")
add_subdirectory(include)
add_subdirectory(src)

add_custom_target(coverage 
  COMMAND mkdir -p coverage
  COMMAND gcovr -r ${CMAKE_SOURCE_DIR} -e "${CMAKE_SOURCE_DIR}/tests/" -e "${CMAKE_BINARY_DIR}/" --html-details -o coverage/index.html .
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
