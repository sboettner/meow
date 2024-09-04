# Meow Vocal Studio
Pitch and Timing Correction for Vocal Recordings

## Building

### Prerequisites

Meow depends on the following packages and libraries:

* CMake 3.18
* libsndfile
* PortAudio
* fftw3
* gtkmm3
* cereal

Note that Meow has currently has been tested only on Ubuntu 22.04 LTS.

### Building

In the Meow source directory, run the following commands:

    cmake -S . -B build
    cd build
    make
    make install
