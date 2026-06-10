//------------------------------------------------------------------------
// TakeMeIn 16-Band Parametric Equalizer - VST3 Plugin
// Copyright(c) 2024 TakeMeIn
//------------------------------------------------------------------------

#include "eqcids.h"
#include "eqcontroller.h"
#include "eqprocessor.h"
#include "version.h"


#include "public.sdk/source/main/pluginfactory.h"

#define stringPluginName "TakeMeIn EQ 16-Band"

using namespace Steinberg::Vst;
using namespace Steinberg;

//------------------------------------------------------------------------
//  VST Plug-in Entry
//------------------------------------------------------------------------

BEGIN_FACTORY_DEF("TakeMeIn", "https://takemein.com",
                  "mailto:contact@takemein.com")

//---First Plug-in included in this factory-------
// its kVstAudioEffectClass component
DEF_CLASS2(INLINE_UID_FROM_FUID(TakeMeIn::kEQProcessorUID),
           PClassInfo::kManyInstances, kVstAudioEffectClass, stringPluginName,
           Vst::kDistributable, TakeMeInEQVST3Category, FULL_VERSION_STR,
           kVstVersionString, TakeMeIn::EQProcessor::createInstance)

// its kVstComponentControllerClass component
DEF_CLASS2(INLINE_UID_FROM_FUID(TakeMeIn::kEQControllerUID),
           PClassInfo::kManyInstances, kVstComponentControllerClass,
           stringPluginName "Controller", 0, "", FULL_VERSION_STR,
           kVstVersionString, TakeMeIn::EQController::createInstance)

END_FACTORY
