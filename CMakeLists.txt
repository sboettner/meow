cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0076 NEW)

project(Meow VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(GNUInstallDirs)

find_package(PkgConfig)
pkg_check_modules(PORTAUDIO portaudio-2.0)
pkg_check_modules(SNDFILE sndfile)
pkg_check_modules(FFTW fftw3)
pkg_check_modules(GTKMM gtkmm-3.0)

add_executable(meow)
add_subdirectory(src)

target_include_directories(meow PUBLIC ${PORTAUDIO_INCLUDE_DIRS} ${SNDFILE_INCLUDE_DIRS} ${FFTW_INCLUDE_DIRS} ${GTKMM_INCLUDE_DIRS})
target_link_libraries(meow PUBLIC ${PORTAUDIO_LIBRARIES} ${SNDFILE_LIBRARIES} ${FFTW_LIBRARIES} ${GTKMM_LIBRARIES})
