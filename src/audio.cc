#include <stdio.h>
#include <portaudio.h>
#include "audio.h"


class AudioDevice:public IAudioDevice {
public:
    AudioDevice();
    virtual ~AudioDevice();

private:
    PaStream*   stream;
    int         phase=0;

    void process(float* output, unsigned long count);

    static int callback(const void* input, void* output, unsigned long count, const PaStreamCallbackTimeInfo* timeinfo, PaStreamCallbackFlags flags, void* userdata);
};


IAudioDevice::~IAudioDevice()
{
}


IAudioDevice* IAudioDevice::create()
{
    PaError err=Pa_Initialize();
    
    if (err!=paNoError) {
        fprintf(stderr, "PortAudio: %s\n", Pa_GetErrorText(err));
        return nullptr;
    }
    
    return new AudioDevice();
}


AudioDevice::AudioDevice()
{
    PaError err=Pa_OpenDefaultStream(
        &stream,
        0,  // no input
        1,  // mono output
        paFloat32,
        48000,
        paFramesPerBufferUnspecified,
        &AudioDevice::callback,
        this);

    if (err!=paNoError) {
        fprintf(stderr, "PortAudio: %s\n", Pa_GetErrorText(err));
        stream=nullptr;
        return;
    }

    Pa_StartStream(stream);
}


AudioDevice::~AudioDevice()
{
    Pa_AbortStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
}


void AudioDevice::process(float* output, unsigned long count)
{
    for (int i=0;i<count;i++) {
        output[i]=0.0f;
    }
}


int AudioDevice::callback(const void* input, void* output, unsigned long count, const PaStreamCallbackTimeInfo* timeinfo, PaStreamCallbackFlags flags, void* userdata)
{
    reinterpret_cast<AudioDevice*>(userdata)->process(reinterpret_cast<float*>(output), count);

    return paContinue;
}
