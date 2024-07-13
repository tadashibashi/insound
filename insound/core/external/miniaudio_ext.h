/// Contains some functions using private implementation of miniaudio
#pragma once
#include <insound/core/Marker.h>

#include <string>
#include <vector>

/// Get marker data from a WAV file
bool insound_ma_dr_wav_get_markers(const std::string &filepath, std::vector<insound::Marker> *outMarkers);
