#pragma once

#include <iostream>
#include "track.h"

struct Project {
    std::unique_ptr<Track>  track;

    void read(std::istream&);
    void write(std::ostream&);

    template<typename Archive>
    void serialize(Archive& ar, uint32_t ver);
};
