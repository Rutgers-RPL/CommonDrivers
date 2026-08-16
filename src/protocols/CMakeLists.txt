# What this basically does is conditionally include the i2c, qspi, spi, and uart
# files if and only if they are enabled in cubemx. 
#
# When cubemx enables the perhipherals, it adds something like this
#   ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_spi.c
# to STM32_Drivers_Src. Therefore we can just iterate through it and find if the
# given drivers exist, and if they do, we can safely include our thin HAL abstraction
# for the driver.
set(PATTERNS
  "_hal_i2c\\.c$"
  "_hal_qspi\\.c$"
  "_hal_spi\\.c$"
  "_hal_uart\\.c$"
)

set(SOURCES
  "i2c.cpp"
  "qspi.cpp"
  "spi.cpp"
  "uart.cpp"
)
  
if(TARGET STM32_Drivers)
  get_target_property(STM32_Drivers_Src STM32_Drivers SOURCES)
  foreach(PATTERN SOURCE IN ZIP_LISTS PATTERNS SOURCES)
    string(REGEX MATCH "${PATTERN}" FOUND_MATCH "${STM32_Drivers_Src}")
    if(FOUND_MATCH)
      # If the first pattern gets matched, then we add our file to the library
      message(STATUS "Including " ${SOURCE} " in build")
      target_sources(common_drivers PRIVATE "${SOURCE}")
    endif()
  endforeach()
endif()
