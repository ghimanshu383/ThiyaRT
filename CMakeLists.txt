cmake_minimum_required(VERSION 3.28)
project(ThiyaRT VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ThiyaRT main.cpp
        src/Gpu.cpp
        include/Gpu.h
        include/Util.h
        include/AppWindow.h
        src/AppWindow.cpp
        include/GraphicsPipeline.h
        src/GraphicsPipeline.cpp)

include(FetchContent)

FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.15.0
        GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(spdlog)

target_include_directories(ThiyaRT PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        D:\\VulkanSDK\\1.3.283.0\\Include
        ${CMAKE_CURRENT_SOURCE_DIR}/ext/glm
)
target_link_libraries(ThiyaRT PUBLIC
        spdlog
        glfw
        D:\\VulkanSDK\\1.3.283.0\\Lib\\vulkan-1.lib
)
FetchContent_MakeAvailable(spdlog)