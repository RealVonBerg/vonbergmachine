// VONBERGMACHINE - arrangement / pattern generator
#pragma once
#include "VbmCore.h"

namespace vbm
{
constexpr int NUM_STYLES = 5;
constexpr int BANK_SIZE  = 500;

const char* styleName (int style);     // "ROCKY I" ...
float       styleBpm  (int style);     // reference tempo measured from the source tracks
int         styleKey  (int style);     // default 808 key (0 = C ... 11 = B)

// styleSel: -1 = AUTO (bank: index % 5, random: any style), 0..4 = fixed style
SongPtr generateBank   (int bankIndex, int styleSel, int bars);
SongPtr generateRandom (uint32_t seed, int styleSel, int bars);
SongPtr generateSong   (uint32_t seed, int style, int bars, int bankIndex);
} // namespace vbm
