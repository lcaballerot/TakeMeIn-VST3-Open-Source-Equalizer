//------------------------------------------------------------------------
// TakeMeIn 16-Band Parametric Equalizer - VST3 Plugin
// Copyright(c) 2024 TakeMeIn
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace TakeMeIn {

//------------------------------------------------------------------------
// Number of EQ bands
//------------------------------------------------------------------------
static constexpr int kNumBands = 16;

//------------------------------------------------------------------------
// Fixed center frequencies (Hz) for each band
//------------------------------------------------------------------------
static constexpr double kBandFrequencies[kNumBands] = {
    25.0, 40.0, 63.0, 100.0, 160.0, 250.0, 400.0, 630.0,
    1000.0, 1600.0, 2500.0, 4000.0, 6300.0, 10000.0, 16000.0, 20000.0
};

//------------------------------------------------------------------------
// Band frequency labels for UI display
//------------------------------------------------------------------------
static const char* const kBandLabels[kNumBands] = {
    "25 Hz", "40 Hz", "63 Hz", "100 Hz", "160 Hz", "250 Hz", "400 Hz", "630 Hz",
    "1 kHz", "1.6 kHz", "2.5 kHz", "4 kHz", "6.3 kHz", "10 kHz", "16 kHz", "20 kHz"
};

//------------------------------------------------------------------------
// Parameter IDs
//------------------------------------------------------------------------
enum EQParams : Steinberg::Vst::ParamID
{
    kBypassId = 0,

    // Band gain parameters: IDs 1..16
    kBandGainFirst = 1,
    kBandGainLast  = kBandGainFirst + kNumBands - 1,  // 16

    // Band Q parameters: IDs 17..32
    kBandQFirst = kBandGainLast + 1,                   // 17
    kBandQLast  = kBandQFirst + kNumBands - 1,         // 32

    // Master output gain (dB)
    kOutputGainId = kBandQLast + 1,                    // 33

    // Solo band: 0 = off, 1..16 = band index + 1 (transient, not persisted)
    kSoloBandId = kOutputGainId + 1                    // 34
};

//------------------------------------------------------------------------
// Output gain range in dB
//------------------------------------------------------------------------
static constexpr double kOutGainMinDB = -12.0;
static constexpr double kOutGainMaxDB =  12.0;

//------------------------------------------------------------------------
// Gain range in dB
//------------------------------------------------------------------------
static constexpr double kGainMinDB = -24.0;
static constexpr double kGainMaxDB =  24.0;
static constexpr double kGainDefaultDB = 0.0;

//------------------------------------------------------------------------
// Q range
//------------------------------------------------------------------------
static constexpr double kQMin = 0.1;
static constexpr double kQMax = 10.0;
static constexpr double kQDefault = 1.0;

//------------------------------------------------------------------------
// Unique IDs for Processor and Controller
//------------------------------------------------------------------------
static const Steinberg::FUID kEQProcessorUID  (0x7A3B1C2D, 0x4E5F6A7B, 0x8C9D0E1F, 0x2A3B4C5D);
static const Steinberg::FUID kEQControllerUID (0x1D2C3B4A, 0x5F6E7D8C, 0x9A0B1C2D, 0x3E4F5A6B);

#define TakeMeInEQVST3Category "Fx|EQ"

//------------------------------------------------------------------------
} // namespace TakeMeIn
