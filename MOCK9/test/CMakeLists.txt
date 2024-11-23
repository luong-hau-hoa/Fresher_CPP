cmake_minimum_required(VERSION 3.10)
project(IPC)
set(CMAKE_CXX_STANDARD 20) # std::variant

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../lib/src)
message(STATUS "Include directory: ${CMAKE_CURRENT_SOURCE_DIR}")

# Tạo các executable cho Service và App
add_executable(main_service ${CMAKE_CURRENT_SOURCE_DIR}/main_service.cpp)
add_executable(main_temperature_app ${CMAKE_CURRENT_SOURCE_DIR}/main_temperature_app.cpp)
add_executable(main_volume_app ${CMAKE_CURRENT_SOURCE_DIR}/main_volume_app.cpp)
add_executable(main_volume_app2 ${CMAKE_CURRENT_SOURCE_DIR}/main_volume_app2.cpp)

target_include_directories(main_service PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/src)
target_include_directories(main_temperature_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/src)
target_include_directories(main_volume_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/src)
target_include_directories(main_volume_app2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/src)

# Liên kết thư viện với các executable
target_link_libraries(main_service ${CMAKE_CURRENT_SOURCE_DIR}/../lib/build/libService.a rt)
target_link_libraries(main_temperature_app ${CMAKE_CURRENT_SOURCE_DIR}/../lib/build/libClient.a rt)
target_link_libraries(main_volume_app ${CMAKE_CURRENT_SOURCE_DIR}/../lib/build/libClient.a rt)
target_link_libraries(main_volume_app2 ${CMAKE_CURRENT_SOURCE_DIR}/../lib/build/libClient.a rt)

