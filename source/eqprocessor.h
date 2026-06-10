//------------------------------------------------------------------------
// TakeMeIn 16-Band Parametric Equalizer - VST3 Plugin
// Copyright(c) 2024 TakeMeIn
//------------------------------------------------------------------------

#pragma once

#include "biquad.h"
#include "eqcids.h"
#include "public.sdk/source/vst/vstaudioeffect.h"

namespace TakeMeIn {

//------------------------------------------------------------------------
//  EQProcessor - Real-time 16-band parametric equalizer
//------------------------------------------------------------------------
class EQProcessor : public Steinberg::Vst::AudioEffect {
public:
  EQProcessor();
  ~EQProcessor() SMTG_OVERRIDE;

  // Create function
  static Steinberg::FUnknown *createInstance(void * /*context*/) {
    return (Steinberg::Vst::IAudioProcessor *)new EQProcessor;
  }

  //--- AudioEffect overrides ---
  Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown *context)
      SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API terminate() SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool state) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API
  setupProcessing(Steinberg::Vst::ProcessSetup &newSetup) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API
  canProcessSampleSize(Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData &data)
      SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream *state)
      SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream *state)
      SMTG_OVERRIDE;

protected:
  //--- Per-band parameters ---
  double mGainDB[kNumBands]; // gain in dB for each band
  double mQ[kNumBands];      // Q factor for each band
  bool mBypass = false;
  bool mStateTouched = false; // saw param changes or host state this run

  //--- Master output gain ---
  double mOutGainDB = 0.0;
  double mOutGainLin = 1.0;

  //--- Solo (transient): -1 = off, else band index ---
  int mSoloBand = -1;
  Biquad mSoloFilters[2]; // per-channel bandpass when soloing
  void recalcSolo();

  //--- Biquad filters: [band][channel] ---
  static constexpr int kMaxChannels = 2;
  Biquad mFilters[kNumBands][kMaxChannels];

  //--- Internal helpers ---
  void recalcBand(int band);
  void recalcAnalysis();

  //--- Spectrum analysis ---
  Biquad mAnalysis[kNumBands]; // bandpass analysis filters (mono, on output)
  float mBandPeak[kNumBands]{};
  static constexpr int kSpectrumInterval = 64;
  int mSpectrumCounter = 0;
};

//------------------------------------------------------------------------
} // namespace TakeMeIn
