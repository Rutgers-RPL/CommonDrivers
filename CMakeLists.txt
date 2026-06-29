cmake_minimum_required(VERSION 3.22)
enable_language(C)

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
add_library(bmi08 STATIC
    ${bmi08_SOURCE_DIR}/bmi08a.c
    ${bmi08_SOURCE_DIR}/bmi08g.c
    ${bmi08_SOURCE_DIR}/bmi08xa.c
    ${bmi08_SOURCE_DIR}/bmi088_mma.c
    ${bmi08_SOURCE_DIR}/bmi088_anymotiona.c
)
target_include_directories(bmi08 PUBLIC ${bmi08_SOURCE_DIR})

message(STATUS "Resolving bmp5 sensors api dependency...")
FetchContent_Declare(bmp5
  GIT_REPOSITORY https://github.com/boschsensortec/BMP5_SensorAPI.git
  GIT_SHALLOW TRUE
  GIT_TAG master
)
FetchContent_MakeAvailable(bmp5)
add_library(bmp5 STATIC ${bmp5_SOURCE_DIR}/bmp5.c)
target_include_directories(bmp5 PUBLIC ${bmp5_SOURCE_DIR})

message(STATUS "Resolving littlefs dependency...")
FetchContent_Declare(littlefs
  GIT_REPOSITORY https://github.com/littlefs-project/littlefs.git
  GIT_SHALLOW TRUE
  GIT_TAG master
)
FetchContent_MakeAvailable(littlefs)
add_library(littlefs STATIC
  ${littlefs_SOURCE_DIR}/lfs.c
  ${littlefs_SOURCE_DIR}/lfs_util.c
)
target_include_directories(littlefs PUBLIC ${littlefs_SOURCE_DIR})

message(STATUS "Dependencies resolved, now creating library...")
add_library(common_drivers STATIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src/flash.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/flash/gd5f1gq5xe.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/sensors/bmp581.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/sensors/bmi088.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/sensors/CD-PA1616S.c"
)
target_include_directories(common_drivers PUBLIC 
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/flash"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/sensors"
)

message(STATUS "Linking libary to dependencies...")
target_link_libraries(common_drivers PUBLIC bmi08 bmp5 littlefs)

if(NOT TARGET stm32cubemx)
  # Empty for now, will add stuff for tests later
else()
  message(STATUS "Linking stm32 drivers to library....")
  target_link_libraries(common_drivers PUBLIC stm32cubemx)
endif()
