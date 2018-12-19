cmake_minimum_required(VERSION 3.1.0)
project(libqite)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed
set(CMAKE_AUTOMOC ON)

# Find the QtWidgets library
find_package(Qt5Core CONFIG REQUIRED)
find_package(Qt5Gui CONFIG REQUIRED)
find_package(Qt5Multimedia CONFIG REQUIRED)
find_package(Qt5Widgets CONFIG REQUIRED)

include(libqite.cmake)

add_library(qite STATIC ${SOURCES})
set_property(TARGET qite PROPERTY CXX_STANDARD 14)
target_link_libraries(qite Qt5::Widgets Qt5::Multimedia)

install(TARGETS qite DESTINATION lib)
install(FILES ${HEADERS} DESTINATION include)
