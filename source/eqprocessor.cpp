//------------------------------------------------------------------------
// TakeMeIn 16-Band Parametric Equalizer - VST3 Plugin
// Copyright(c) 2024 TakeMeIn
//------------------------------------------------------------------------

#include "eqprocessor.h"
#include "eqcids.h"
#include "eqeditor.h"
#include "eqpersist.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <algorithm>
#include <cmath>

using namespace Steinberg;

namespace TakeMeIn {

//------------------------------------------------------------------------
// Helper: convert normalized [0,1] to gain dB range
//------------------------------------------------------------------------
static double normToGainDB(double norm) {
  return kGainMinDB + norm * (kGainMaxDB - kGainMinDB);
}

//------------------------------------------------------------------------
// Helper: convert gain dB to normalized [0,1]
//------------------------------------------------------------------------
static double gainDBToNorm(double db) {
  return (db - kGainMinDB) / (kGainMaxDB - kGainMinDB);
}

//------------------------------------------------------------------------
// Helper: convert normalized [0,1] to Q range
//------------------------------------------------------------------------
static double normToQ(double norm) { return kQMin + norm * (kQMax - kQMin); }

//------------------------------------------------------------------------
// Helper: convert Q to normalized [0,1]
//------------------------------------------------------------------------
static double qToNorm(double q) { return (q - kQMin) / (kQMax - kQMin); }

//------------------------------------------------------------------------
// EQProcessor
//------------------------------------------------------------------------
EQProcessor::EQProcessor() {
  setControllerClass(kEQControllerUID);

  // Initialize defaults
  for (int i = 0; i < kNumBands; ++i) {
    mGainDB[i] = kGainDefaultDB;
    mQ[i] = kQDefault;
  }
}

//------------------------------------------------------------------------
EQProcessor::~EQProcessor() {}

//------------------------------------------------------------------------
tresult PLUGIN_API EQProcessor::initialize(FUnknown *context) {
  tresult result = AudioEffect::initialize(context);
  if (result != kResultOk)
    return result;

  // Stereo in / Stereo out
  addAudioInput(STR16("Stereo In"), Steinberg::Vst::SpeakerArr::kStereo);
  addAudioOutput(STR16("Stereo Out"), Steinberg::Vst::SpeakerArr::kStereo);

  // Restore last session so a fresh instance starts with the user's mix.
  // Host project state (setState) arrives later and overrides this.
  EQState last;
  if (loadLastSession(last)) {
    for (int i = 0; i < kNumBands; ++i) {
      mGainDB[i] = std::min(kGainMaxDB, std::max(kGainMinDB, (double)last.gainDB[i]));
      mQ[i] = std::min(kQMax, std::max(kQMin, (double)last.q[i]));
    }
    mBypass = last.bypass > 0;
    mOutGainDB = std::min(kOutGainMaxDB,
                          std::max(kOutGainMinDB, (double)last.outGainDB));
    mOutGainLin = std::pow(10.0, mOutGainDB / 20.0);
  }

  return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API EQProcessor::terminate() {
  // Persist the final state as the last session (non-realtime context), but
  // only if this instance actually received parameter changes or host state —
  // otherwise we would clobber a fresher autosave with stale defaults.
  if (mStateTouched) {
    EQState s;
    for (int i = 0; i < kNumBands; ++i) {
      s.gainDB[i] = static_cast<float>(mGainDB[i]);
      s.q[i] = static_cast<float>(mQ[i]);
    }
    s.bypass = mBypass ? 1 : 0;
    s.outGainDB = static_cast<float>(mOutGainDB);
    saveLastSession(s);
  }
  return AudioEffect::terminate();
}

//------------------------------------------------------------------------
tresult PLUGIN_API EQProcessor::setActive(TBool state) {
  if (state) {
    for (int b = 0; b < kNumBands; ++b)
      for (int c = 0; c < kMaxChannels; ++c)
        mFilters[b][c].reset();
    for (int b = 0; b < kNumBands; ++b)
      recalcBand(b);

    // Setup analysis bandpass filters
    for (int b = 0; b < kNumBands; ++b) {
      mAnalysis[b].reset();
      mBandPeak[b] = 0.f;
    }
    recalcAnalysis();
    recalcSolo();
  }
  return AudioEffect::setActive(state);
}

//------------------------------------------------------------------------
tresult PLUGIN_API EQProcessor::setupProcessing(Vst::ProcessSetup &newSetup) {
  return AudioEffect::setupProcessing(newSetup);
}

//------------------------------------------------------------------------
tresult PLUGIN_API EQProcessor::canProcessSampleSize(int32 symbolicSampleSize) {
  if (symbolicSampleSize == Vst::kSample32)
    return kResultTrue;

  return kResultFalse;
}

//------------------------------------------------------------------------
void EQProcessor::recalcBand(int band) {
  double sr = processSetup.sampleRate;
  if (sr <= 0.0)
    sr = 44100.0;

  for (int c = 0; c < kMaxChannels; ++c) {
    mFilters[band][c].setParams(sr, kBandFrequencies[band], mGainDB[band],
                                mQ[band]);
  }
}

//------------------------------------------------------------------------
tresult PLUGIN_API EQProcessor::process(Vst::ProcessData &data) {
  //--- Read parameter changes ---
  if (data.inputParameterChanges) {
    int32 numParamsChanged = data.inputParameterChanges->getParameterCount();
    for (int32 index = 0; index < numParamsChanged; ++index) {
      Vst::IParamValueQueue *paramQueue =
          data.inputParameterChanges->getParameterData(index);
      if (!paramQueue)
        continue;

      Vst::ParamValue value;
      int32 sampleOffset;
      int32 numPoints = paramQueue->getPointCount();
      Vst::ParamID paramId = paramQueue->getParameterId();

      // Get the last point value
      if (paramQueue->getPoint(numPoints - 1, sampleOffset, value) !=
          kResultTrue)
        continue;

      if (paramId == EQParams::kSoloBandId) {
        // transient — does not mark state as touched
        int s = static_cast<int>(value * kNumBands + 0.5) - 1;
        if (s != mSoloBand) {
          mSoloBand = (s >= 0 && s < kNumBands) ? s : -1;
          recalcSolo();
        }
        continue;
      }
      mStateTouched = true;
      if (paramId == EQParams::kBypassId) {
        mBypass = (value > 0.5);
      } else if (paramId == EQParams::kOutputGainId) {
        mOutGainDB = kOutGainMinDB + value * (kOutGainMaxDB - kOutGainMinDB);
        mOutGainLin = std::pow(10.0, mOutGainDB / 20.0);
      } else if (paramId >= EQParams::kBandGainFirst &&
                 paramId <= EQParams::kBandGainLast) {
        int band = paramId - EQParams::kBandGainFirst;
        mGainDB[band] = normToGainDB(value);
        recalcBand(band);
      } else if (paramId >= EQParams::kBandQFirst &&
                 paramId <= EQParams::kBandQLast) {
        int band = paramId - EQParams::kBandQFirst;
        mQ[band] = normToQ(value);
        recalcBand(band);
      }
    }
  }

  //--- Process audio ---
  if (data.numInputs == 0 || data.numOutputs == 0)
    return kResultOk;

  int32 numChannels = data.inputs[0].numChannels;
  int32 numSamples = data.numSamples;

  if (numSamples <= 0)
    return kResultOk;

  // Limit to stereo
  if (numChannels > kMaxChannels)
    numChannels = kMaxChannels;

  float **in = data.inputs[0].channelBuffers32;
  float **out = data.outputs[0].channelBuffers32;

  if (mBypass) {
    // Bypass: copy input to output
    for (int32 ch = 0; ch < numChannels; ++ch) {
      if (in[ch] != out[ch]) {
        for (int32 s = 0; s < numSamples; ++s)
          out[ch][s] = in[ch][s];
      }
    }
    // Clear any extra output channels
    for (int32 ch = numChannels; ch < data.outputs[0].numChannels; ++ch) {
      for (int32 s = 0; s < numSamples; ++s)
        out[ch][s] = 0.0f;
    }
    return kResultOk;
  }

  // Process through 16-band EQ cascade
  for (int32 ch = 0; ch < numChannels; ++ch) {
    // Copy input to output first (in-place processing)
    if (in[ch] != out[ch]) {
      for (int32 s = 0; s < numSamples; ++s)
        out[ch][s] = in[ch][s];
    }

    // Apply each band filter in series
    for (int b = 0; b < kNumBands; ++b) {
      for (int32 s = 0; s < numSamples; ++s) {
        out[ch][s] = mFilters[b][ch].process(out[ch][s]);
      }
    }

    // Solo monitoring: isolate one band with a bandpass (+6 dB makeup)
    if (mSoloBand >= 0) {
      for (int32 s = 0; s < numSamples; ++s)
        out[ch][s] = mSoloFilters[ch].process(out[ch][s]) * 2.0f;
    }

    // Master output gain
    if (mOutGainLin != 1.0) {
      float g = static_cast<float>(mOutGainLin);
      for (int32 s = 0; s < numSamples; ++s)
        out[ch][s] *= g;
    }
  }

  // Output peak for the clip indicator
  {
    float peak = 0.f;
    for (int32 ch = 0; ch < numChannels; ++ch)
      for (int32 s = 0; s < numSamples; ++s) {
        float a = std::fabs(out[ch][s]);
        if (a > peak)
          peak = a;
      }
    g_spectrumData.outPeak.store(peak, std::memory_order_relaxed);
  }

  // Clear any extra output channels
  for (int32 ch = numChannels; ch < data.outputs[0].numChannels; ++ch) {
    for (int32 s = 0; s < numSamples; ++s)
      out[ch][s] = 0.0f;
  }

  //--- Spectrum analysis on output --- (Instruction 3)
  for (int32 s = 0; s < numSamples; ++s) {
    // Mix to mono for analysis
    float mono = out[0][s];
    if (numChannels > 1)
      mono = (out[0][s] + out[1][s]) * 0.5f;

    // Run through each analysis bandpass filter
    for (int b = 0; b < kNumBands; ++b) {
      float filtered = mAnalysis[b].process(mono);
      float absVal = std::fabs(filtered);
      if (absVal > mBandPeak[b])
        mBandPeak[b] = absVal;
    }
    mSpectrumCounter++;

    if (mSpectrumCounter >= kSpectrumInterval) {
      // Write band levels to shared buffer
      for (int b = 0; b < kNumBands; ++b) {
        g_spectrumData.bandLevels[b].store(mBandPeak[b],
                                           std::memory_order_relaxed);
        mBandPeak[b] = 0.f;
      }
      mSpectrumCounter = 0;
    }
  }

  return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API EQProcessor::setState(IBStream *state) {
  if (!state)
    return kResultFalse;

  IBStreamer streamer(state, kLittleEndian);

  // Read gain values
  for (int i = 0; i < kNumBands; ++i) {
    float val = 0.f;
    if (streamer.readFloat(val) == false)
      return kResultFalse;
    mGainDB[i] = static_cast<double>(val);
  }

  // Read Q values
  for (int i = 0; i < kNumBands; ++i) {
    float val = 1.f;
    if (streamer.readFloat(val) == false)
      return kResultFalse;
    mQ[i] = static_cast<double>(val);
  }

  // Read bypass
  int32 bypassState = 0;
  if (streamer.readInt32(bypassState) == false)
    return kResultFalse;
  mBypass = bypassState > 0;
  mStateTouched = true;

  // Output gain — appended in v1.1; tolerate older streams without it
  float outDB = 0.f;
  if (streamer.readFloat(outDB)) {
    mOutGainDB = std::min(kOutGainMaxDB, std::max(kOutGainMinDB, (double)outDB));
    mOutGainLin = std::pow(10.0, mOutGainDB / 20.0);
  }

  // Recalculate all filters
  for (int b = 0; b < kNumBands; ++b)
    recalcBand(b);

  return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API EQProcessor::getState(IBStream *state) {
  IBStreamer streamer(state, kLittleEndian);

  // Write gain values
  for (int i = 0; i < kNumBands; ++i)
    streamer.writeFloat(static_cast<float>(mGainDB[i]));

  // Write Q values
  for (int i = 0; i < kNumBands; ++i)
    streamer.writeFloat(static_cast<float>(mQ[i]));

  // Write bypass
  streamer.writeInt32(mBypass ? 1 : 0);

  // Output gain (appended in v1.1)
  streamer.writeFloat(static_cast<float>(mOutGainDB));

  return kResultOk;
}

//------------------------------------------------------------------------
void EQProcessor::recalcSolo() {
  double sr = processSetup.sampleRate;
  if (sr <= 0.0)
    sr = 44100.0;
  for (int c = 0; c < 2; ++c) {
    mSoloFilters[c].reset();
    if (mSoloBand >= 0)
      mSoloFilters[c].setBandpass(sr, kBandFrequencies[mSoloBand],
                                  std::max(1.0, mQ[mSoloBand]));
  }
}

//------------------------------------------------------------------------
void EQProcessor::recalcAnalysis() {
  double sr = processSetup.sampleRate;
  if (sr <= 0.0)
    sr = 44100.0;
  for (int b = 0; b < kNumBands; ++b) {
    // Configure as true bandpass filter for frequency analysis
    mAnalysis[b].setBandpass(sr, kBandFrequencies[b], 1.5);
  }
}

//------------------------------------------------------------------------
} // namespace TakeMeIn
