#include <stdio.h>
#include <assert.h>
#include <portaudio.h>
#include "audio.h"


class AudioDevice:public IAudioDevice {
public:
    AudioDevice();
    virtual ~AudioDevice();

    virtual void play(std::shared_ptr<IAudioProvider>) override;

private:
    PaStream*   stream;
    int         phase=0;

    std::shared_ptr<IAudioProvider> current_provider;

    void process(float* output, unsigned long count);

    static int callback(const void* input, void* output, unsigned long count, const PaStreamCallbackTimeInfo* timeinfo, PaStreamCallbackFlags flags, void* userdata);
};


IAudioProvider::~IAudioProvider()
{
}


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


void AudioDevice::play(std::shared_ptr<IAudioProvider> provider)
{
    current_provider=provider;
}


void AudioDevice::process(float* output, unsigned long count)
{
    while (current_provider && count) {
        unsigned long done=current_provider->provide(output, count);
        assert(done<=count);

        if (!done) {
            current_provider=nullptr;
            break;
        }

        output+=done;
        count -=done;
    }

    while (count--)
        *output++=0.0f;
}


int AudioDevice::callback(const void* input, void* output, unsigned long count, const PaStreamCallbackTimeInfo* timeinfo, PaStreamCallbackFlags flags, void* userdata)
{
    reinterpret_cast<AudioDevice*>(userdata)->process(reinterpret_cast<float*>(output), count);

    return paContinue;
}
