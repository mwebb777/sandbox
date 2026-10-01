cmake_minimum_required(VERSION 3.16)
project(VideoStreamServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt6 components
find_package(Qt6 REQUIRED COMPONENTS Core Network Gui HttpServer)

qt_standard_project_setup()

add_executable(VideoStreamServer
    main.cpp
)

target_link_libraries(VideoStreamServer PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Gui
    Qt6::HttpServer
)

# Optional: install target
install(TARGETS VideoStreamServer
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
