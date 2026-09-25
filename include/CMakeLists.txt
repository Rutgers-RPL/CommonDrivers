target_include_directories(common_drivers PUBLIC
  "."
  "sensors"
  "protocols"
  "flash"
)

if(TARGET FreeRTOS)
  target_include_directories(common_drivers PUBLIC "tasks")
endif()
