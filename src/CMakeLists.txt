target_include_directories(meow PUBLIC .)

target_sources(meow PUBLIC correlation.cc waveform.cc track.cc serialization.cc controller.cc audio.cc render.cc)

add_subdirectory(frontends)
