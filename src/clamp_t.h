
#pragma once

template <typename T>
static inline T clampT(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
