#pragma once

#include "track.h"

class IAudioProvider;

IAudioProvider* create_render_audio_provider(const Track& track, Track::Chunk* first, Track::Chunk* last);
