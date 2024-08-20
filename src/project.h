#pragma once

#include <iostream>
#include "track.h"

struct Project {
    // these don't have any effect on the signal processing, but are used to display a beat grid in the user interface
    double  bpm=120.0;
    int     beat_subdivisions=4;

    std::vector<std::unique_ptr<Track>>     tracks;

    void read(std::istream&);
    void write(std::ostream&);

    template<typename Archive>
    void serialize(Archive& ar, uint32_t ver);
};
