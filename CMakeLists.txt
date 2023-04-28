CMAKE_MINIMUM_REQUIRED(VERSION 3.18.1)
project("Quick Render")

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

file(GLOB SOURCES
      ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(Render ${SOURCES})

target_link_libraries(Render vulkan glfw glm)

