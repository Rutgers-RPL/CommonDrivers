add_subdirectory(flash)
add_subdirectory(protocols)
add_subdirectory(sensors)

target_sources(common_drivers PRIVATE "log.cpp")
