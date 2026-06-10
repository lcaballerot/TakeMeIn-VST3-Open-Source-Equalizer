//------------------------------------------------------------------------
// TakeMeIn 16-Band Parametric Equalizer - VST3 Plugin
// Copyright(c) 2024 TakeMeIn
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "vstgui/plugin-bindings/vst3editor.h"

namespace TakeMeIn {

class EQMainView; // forward

//------------------------------------------------------------------------
//  EQController - Parameter definitions + GUI delegate
//------------------------------------------------------------------------
class EQController : public Steinberg::Vst::EditControllerEx1,
                     public VSTGUI::VST3EditorDelegate {
public:
  EQController() = default;
  ~EQController() SMTG_OVERRIDE = default;

  static Steinberg::FUnknown *createInstance(void * /*context*/) {
    return (Steinberg::Vst::IEditController *)new EQController;
  }

  // IPluginBase
  Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown *context)
      SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API terminate() SMTG_OVERRIDE;

  // EditController
  Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream *state)
      SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream *state)
      SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream *state)
      SMTG_OVERRIDE;
  Steinberg::IPlugView *PLUGIN_API createView(Steinberg::FIDString name)
      SMTG_OVERRIDE;

  // VST3EditorDelegate
  VSTGUI::CView *createCustomView(VSTGUI::UTF8StringPtr name,
                                  const VSTGUI::UIAttributes &attributes,
                                  const VSTGUI::IUIDescription *description,
                                  VSTGUI::VST3Editor *editor) override;
  void willClose(VSTGUI::VST3Editor *editor) override;

  // Push a full EQ state to host + processor (used by profile load / reset)
  void applyEQState(const struct EQState &s);

  // Resize the editor (75% / 100% / 125%) and persist the preference
  void setUIScale(double scale);

  //---Interface---------
  DEFINE_INTERFACES
  END_DEFINE_INTERFACES(EditController)
  DELEGATE_REFCOUNT(EditController)

protected:
  EQMainView *mActiveView = nullptr;
  VSTGUI::VST3Editor *mActiveEditor = nullptr;
};

//------------------------------------------------------------------------
} // namespace TakeMeIn
