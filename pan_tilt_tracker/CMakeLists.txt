cmake_minimum_required(VERSION 3.16)
project(pan_tilt_tracker VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Release by default for performance on Pi
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# OpenCV
find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui videoio objdetect)
message(STATUS "OpenCV version: ${OpenCV_VERSION}")

# Sources
set(SOURCES
    src/main.cpp
    src/PIDController.cpp
    src/ServoController.cpp
    src/ObjectTracker.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${OpenCV_LIBS}
)

# Warnings
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Release>:-O3 -march=native>
)

# Install rule (optional)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

# Print helpful info after configure
message(STATUS "--------------------------------------------------")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "After build run with: sudo ./${PROJECT_NAME}")
message(STATUS "Remember to enable: dtoverlay=pwm-2chan")
message(STATUS "--------------------------------------------------")
