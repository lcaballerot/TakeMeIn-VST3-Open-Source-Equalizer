//------------------------------------------------------------------------
// TakeMeIn 16-Band Parametric Equalizer - VST3 Plugin
// Copyright(c) 2024 TakeMeIn
//------------------------------------------------------------------------
// Biquad filter - Peaking EQ implementation
// Based on Audio EQ Cookbook by Robert Bristow-Johnson
//------------------------------------------------------------------------

#pragma once

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace TakeMeIn {

//------------------------------------------------------------------------
//  Biquad - Direct Form II Transposed peaking EQ filter
//------------------------------------------------------------------------
class Biquad {
public:
  Biquad() { reset(); }

  //--- Reset delay state ---
  void reset() {
    z1 = 0.0;
    z2 = 0.0;
  }

  //--- Calculate coefficients for peaking EQ ---
  // sampleRate: sample rate in Hz
  // freq:       center frequency in Hz
  // gainDB:     gain in dB (negative = cut, positive = boost)
  // Q:          quality factor (bandwidth control)
  void setParams(double sampleRate, double freq, double gainDB, double Q) {
    if (sampleRate <= 0.0 || freq <= 0.0 || Q <= 0.0)
      return;

    // Clamp frequency to Nyquist
    if (freq >= sampleRate * 0.5)
      freq = sampleRate * 0.5 - 1.0;

    double A = std::pow(10.0, gainDB / 40.0); // amplitude
    double w0 = 2.0 * M_PI * freq / sampleRate;
    double sinW0 = std::sin(w0);
    double cosW0 = std::cos(w0);
    double alpha = sinW0 / (2.0 * Q);

    double b0 = 1.0 + alpha * A;
    double b1 = -2.0 * cosW0;
    double b2 = 1.0 - alpha * A;
    double a0 = 1.0 + alpha / A;
    double a1 = -2.0 * cosW0;
    double a2 = 1.0 - alpha / A;

    // Normalize by a0
    mB0 = b0 / a0;
    mB1 = b1 / a0;
    mB2 = b2 / a0;
    mA1 = a1 / a0;
    mA2 = a2 / a0;
  }

  //--- Calculate coefficients for bandpass filter (constant 0dB peak gain) ---
  void setBandpass(double sampleRate, double freq, double Q) {
    if (sampleRate <= 0.0 || freq <= 0.0 || Q <= 0.0)
      return;
    if (freq >= sampleRate * 0.5)
      freq = sampleRate * 0.5 - 1.0;

    double w0 = 2.0 * M_PI * freq / sampleRate;
    double sinW0 = std::sin(w0);
    double cosW0 = std::cos(w0);
    double alpha = sinW0 / (2.0 * Q);

    double b0 = alpha;
    double b1 = 0.0;
    double b2 = -alpha;
    double a0 = 1.0 + alpha;
    double a1 = -2.0 * cosW0;
    double a2 = 1.0 - alpha;

    mB0 = b0 / a0;
    mB1 = b1 / a0;
    mB2 = b2 / a0;
    mA1 = a1 / a0;
    mA2 = a2 / a0;
  }

  //--- Process one sample (Direct Form II Transposed) ---
  float process(float input) {
    double in = static_cast<double>(input);
    double out = mB0 * in + z1;
    z1 = mB1 * in - mA1 * out + z2;
    z2 = mB2 * in - mA2 * out;
    return static_cast<float>(out);
  }

private:
  // Coefficients (normalized)
  double mB0 = 1.0;
  double mB1 = 0.0;
  double mB2 = 0.0;
  double mA1 = 0.0;
  double mA2 = 0.0;

  // Delay elements
  double z1 = 0.0;
  double z2 = 0.0;
};

//------------------------------------------------------------------------
} // namespace TakeMeIn
