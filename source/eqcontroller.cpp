#include "eqcontroller.h"
#include "eqcids.h"
#include "eqeditor.h"
#include "eqpersist.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"

#include <cstdio>
#include <cstring>

using namespace Steinberg;

namespace TakeMeIn {

//------------------------------------------------------------------------
static double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

static double gainDBToNorm(double db) {
  return clamp01((db - kGainMinDB) / (kGainMaxDB - kGainMinDB));
}

static double qToNorm(double q) {
  return clamp01((q - kQMin) / (kQMax - kQMin));
}

//------------------------------------------------------------------------
tresult PLUGIN_API EQController::initialize(FUnknown *context) {
  tresult result = EditControllerEx1::initialize(context);
  if (result != kResultOk)
    return result;

  parameters.addParameter(STR16("Bypass"), nullptr, 1, 0,
                          Vst::ParameterInfo::kCanAutomate |
                              Vst::ParameterInfo::kIsBypass,
                          EQParams::kBypassId);

  for (int i = 0; i < kNumBands; ++i) {
    char title[64];
    snprintf(title, sizeof(title), "%s", kBandLabels[i]);
    Vst::String128 titleStr;
    for (int c = 0; c < 128 && title[c]; ++c)
      titleStr[c] = static_cast<Vst::TChar>(title[c]);
    size_t len = strlen(title);
    if (len < 128)
      titleStr[len] = 0;

    char shortTitle[32];
    snprintf(shortTitle, sizeof(shortTitle), "G%d", i + 1);
    Vst::String128 shortStr;
    for (int c = 0; c < 128 && shortTitle[c]; ++c)
      shortStr[c] = static_cast<Vst::TChar>(shortTitle[c]);
    size_t slen = strlen(shortTitle);
    if (slen < 128)
      shortStr[slen] = 0;

    double defaultNorm = gainDBToNorm(kGainDefaultDB);
    parameters.addParameter(titleStr, STR16("dB"), 0, defaultNorm,
                            Vst::ParameterInfo::kCanAutomate,
                            EQParams::kBandGainFirst + i, 0, shortStr);
  }

  for (int i = 0; i < kNumBands; ++i) {
    char title[64];
    snprintf(title, sizeof(title), "Q %s", kBandLabels[i]);
    Vst::String128 titleStr;
    for (int c = 0; c < 128 && title[c]; ++c)
      titleStr[c] = static_cast<Vst::TChar>(title[c]);
    size_t len = strlen(title);
    if (len < 128)
      titleStr[len] = 0;

    char shortTitle[32];
    snprintf(shortTitle, sizeof(shortTitle), "Q%d", i + 1);
    Vst::String128 shortStr;
    for (int c = 0; c < 128 && shortTitle[c]; ++c)
      shortStr[c] = static_cast<Vst::TChar>(shortTitle[c]);
    size_t slen = strlen(shortTitle);
    if (slen < 128)
      shortStr[slen] = 0;

    double defaultNorm = qToNorm(kQDefault);
    parameters.addParameter(titleStr, STR16("Q"), 0, defaultNorm,
                            Vst::ParameterInfo::kCanAutomate,
                            EQParams::kBandQFirst + i, 0, shortStr);
  }

  parameters.addParameter(STR16("Output Gain"), STR16("dB"), 0, 0.5,
                          Vst::ParameterInfo::kCanAutomate,
                          EQParams::kOutputGainId);

  // Solo: 0 = off, 1..16 = band. Transient monitoring control.
  parameters.addParameter(STR16("Solo Band"), nullptr, kNumBands, 0, 0,
                          EQParams::kSoloBandId);

  // Restore last session so the mix is ready at launch. If the host loads a
  // saved project afterwards, setComponentState overrides this — which is the
  // desired priority.
  EQState last;
  if (loadLastSession(last)) {
    for (int i = 0; i < kNumBands; ++i) {
      setParamNormalized(EQParams::kBandGainFirst + i, gainDBToNorm(last.gainDB[i]));
      setParamNormalized(EQParams::kBandQFirst + i, qToNorm(last.q[i]));
    }
    setParamNormalized(EQParams::kBypassId, last.bypass ? 1.0 : 0.0);
    setParamNormalized(EQParams::kOutputGainId,
                       clamp01((last.outGainDB - kOutGainMinDB) /
                               (kOutGainMaxDB - kOutGainMinDB)));
  }

  return kResultOk;
}

//------------------------------------------------------------------------
void EQController::applyEQState(const EQState &s) {
  for (int i = 0; i < kNumBands; ++i) {
    Vst::ParamID gid = EQParams::kBandGainFirst + i;
    double gn = gainDBToNorm(s.gainDB[i]);
    beginEdit(gid);
    setParamNormalized(gid, gn);
    performEdit(gid, gn);
    endEdit(gid);

    Vst::ParamID qid = EQParams::kBandQFirst + i;
    double qn = qToNorm(s.q[i]);
    beginEdit(qid);
    setParamNormalized(qid, qn);
    performEdit(qid, qn);
    endEdit(qid);
  }
  double bn = s.bypass ? 1.0 : 0.0;
  beginEdit(EQParams::kBypassId);
  setParamNormalized(EQParams::kBypassId, bn);
  performEdit(EQParams::kBypassId, bn);
  endEdit(EQParams::kBypassId);

  double on = clamp01((s.outGainDB - kOutGainMinDB) /
                      (kOutGainMaxDB - kOutGainMinDB));
  beginEdit(EQParams::kOutputGainId);
  setParamNormalized(EQParams::kOutputGainId, on);
  performEdit(EQParams::kOutputGainId, on);
  endEdit(EQParams::kOutputGainId);
}

//------------------------------------------------------------------------
void EQController::setUIScale(double scale) {
  if (mActiveEditor)
    mActiveEditor->setZoomFactor(scale);
  saveUIScale(scale);
}

//------------------------------------------------------------------------
tresult PLUGIN_API EQController::terminate() {
  mActiveView = nullptr;
  return EditControllerEx1::terminate();
}

//------------------------------------------------------------------------
tresult PLUGIN_API EQController::setComponentState(IBStream *state) {
  if (!state)
    return kResultFalse;
  IBStreamer streamer(state, kLittleEndian);

  for (int i = 0; i < kNumBands; ++i) {
    float val = 0.f;
    if (streamer.readFloat(val) == false)
      return kResultFalse;
    setParamNormalized(EQParams::kBandGainFirst + i, gainDBToNorm(val));
  }
  for (int i = 0; i < kNumBands; ++i) {
    float val = 1.f;
    if (streamer.readFloat(val) == false)
      return kResultFalse;
    setParamNormalized(EQParams::kBandQFirst + i, qToNorm(val));
  }
  int32 bypassState = 0;
  if (streamer.readInt32(bypassState) == false)
    return kResultFalse;
  setParamNormalized(EQParams::kBypassId, bypassState ? 1.0 : 0.0);

  // Output gain — appended in v1.1; tolerate older streams without it
  float outDB = 0.f;
  if (streamer.readFloat(outDB)) {
    setParamNormalized(EQParams::kOutputGainId,
                       clamp01((outDB - kOutGainMinDB) /
                               (kOutGainMaxDB - kOutGainMinDB)));
  }

  return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API EQController::setState(IBStream *state) {
  return kResultTrue;
}
tresult PLUGIN_API EQController::getState(IBStream *state) {
  return kResultTrue;
}

//------------------------------------------------------------------------
IPlugView *PLUGIN_API EQController::createView(FIDString name) {
  if (FIDStringsEqual(name, Vst::ViewType::kEditor)) {
    auto *editor = new VSTGUI::VST3Editor(this, "view", "eqeditor.uidesc");
    editor->setDelegate(this);
    editor->setAllowedZoomFactors({0.75, 1.0, 1.25});
    editor->setZoomFactor(loadUIScale());
    mActiveEditor = editor;
    return editor;
  }
  return nullptr;
}

//------------------------------------------------------------------------
VSTGUI::CView *EQController::createCustomView(
    VSTGUI::UTF8StringPtr name, const VSTGUI::UIAttributes &attributes,
    const VSTGUI::IUIDescription *description, VSTGUI::VST3Editor *editor) {
  if (VSTGUI::UTF8StringView(name) == "EQMainView") {
    mActiveView =
        new EQMainView(VSTGUI::CRect(0, 0, kEditorWidth, kEditorHeight), this);
    return mActiveView;
  }
  return nullptr;
}

//------------------------------------------------------------------------
void EQController::willClose(VSTGUI::VST3Editor *editor) {
  mActiveView = nullptr;
  if (mActiveEditor == editor)
    mActiveEditor = nullptr;
}

//------------------------------------------------------------------------
} // namespace TakeMeIn
