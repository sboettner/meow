configure_file(config.h.in config.h)

target_include_directories(meow PUBLIC . ${CMAKE_CURRENT_BINARY_DIR})

target_sources(meow PUBLIC correlation.cc waveform.cc track.cc serialization.cc controller.cc audio.cc render.cc)

add_subdirectory(frontends)
