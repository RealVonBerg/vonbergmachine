// VONBERGMACHINE - built-in synthesized drum kit (the defaults before the user drops samples)
#pragma once
#include "VbmCore.h"

namespace vbm
{
// Synthesizes the default sound of a lane at the given sample rate.
SamplePtr makeDefaultSample (int lane, double sampleRate);

// Root frequency of the default 808 (C1). User-dropped 808s are assumed to be rooted on C as well.
constexpr float BASS_ROOT_HZ = 32.7032f;
} // namespace vbm
