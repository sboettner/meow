#pragma once

#include <iostream>
#include "track.h"

struct Project {
    std::vector<std::unique_ptr<Track>>     tracks;

    void read(std::istream&);
    void write(std::ostream&);

    template<typename Archive>
    void serialize(Archive& ar, uint32_t ver);
};
