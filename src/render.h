#pragma once

#include "audio.h"
#include "track.h"

std::shared_ptr<IAudioProvider> create_render_audio_provider(const Track& track, Track::Chunk* first, Track::Chunk* last);
