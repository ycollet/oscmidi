cmake_minimum_required(VERSION 2.8)

project(oscmidi C CXX)

include(FindPkgConfig)

find_package(ALSA REQUIRED)
include_directories(${ALSA_INCLUDE_DIR})

find_package(Threads REQUIRED)

pkg_check_modules(liblo REQUIRED liblo)
include_directories(${liblo_INCLUDE_DIRS})
link_directories(${liblo_LIBRARY_DIRS})

add_executable(oscmidi src/oscmidi.cpp)
target_link_libraries(oscmidi ${liblo_LIBRARIES} ${ALSA_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS oscmidi DESTINATION bin)
