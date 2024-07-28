#include "controller.h"


Controller::Controller(Track& track):track(track)
{
    audiodev=std::unique_ptr<IAudioDevice>(IAudioDevice::create());
}


Controller::~Controller()
{
}
