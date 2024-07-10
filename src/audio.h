#pragma once

class IAudioDevice {
public:
    virtual ~IAudioDevice();

    static IAudioDevice* create();
};
