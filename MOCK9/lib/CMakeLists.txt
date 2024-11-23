cmake_minimum_required(VERSION 3.10)
project(lib)
set(CMAKE_CXX_STANDARD 20) #std::variant

# Tạo thư viện cho Client
add_library(Service STATIC src/libService.cpp)
target_include_directories(Service PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

message(STATUS "Include directory: ${CMAKE_CURRENT_SOURCE_DIR}/include")

# Tạo thư viện cho Client
add_library(Client STATIC src/libClient.cpp)
target_include_directories(Client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

message(STATUS "Include directory: ${CMAKE_CURRENT_SOURCE_DIR}/include")


