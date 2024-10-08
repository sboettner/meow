#pragma once

#include <memory>


class IAudioProvider {
public:
    virtual ~IAudioProvider();

    virtual unsigned long provide(float* buffer, unsigned long count) = 0;

    void terminate();

protected:
    bool    terminating=false;
};


class IAudioDevice {
public:
    virtual ~IAudioDevice();

    virtual void play(std::shared_ptr<IAudioProvider>) = 0;

    static IAudioDevice* create();
};
