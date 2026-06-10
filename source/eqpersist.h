//------------------------------------------------------------------------
// TakeMeIn 16-Band Parametric Equalizer
// Persistence + Profile management (last-session recall and named profiles)
// Copyright(c) 2024 TakeMeIn — Made by LCaballerot01
//------------------------------------------------------------------------

#pragma once

#include "eqcids.h"
#include <string>
#include <vector>

namespace TakeMeIn {

//------------------------------------------------------------------------
// Plain snapshot of the full plugin state, stored on disk.
//------------------------------------------------------------------------
struct EQState {
  float gainDB[kNumBands];
  float q[kNumBands];
  int bypass = 0;
  float outGainDB = 0.f;

  EQState() {
    for (int i = 0; i < kNumBands; ++i) {
      gainDB[i] = static_cast<float>(kGainDefaultDB);
      q[i] = static_cast<float>(kQDefault);
    }
  }
};

//------------------------------------------------------------------------
// Built-in factory presets (name + state), shown above user profiles.
//------------------------------------------------------------------------
struct FactoryPreset {
  const char *name;
  EQState state;
};

const std::vector<FactoryPreset> &factoryPresets();

//------------------------------------------------------------------------
// A discovered profile on disk.
//------------------------------------------------------------------------
struct ProfileEntry {
  std::string name; // display name (file stem)
  std::string path; // full path to the .tmieq file
};

//------------------------------------------------------------------------
// Directory + file location helpers (platform aware, %APPDATA% on Windows).
//------------------------------------------------------------------------
std::string getConfigDir();  // .../TakeMeIn/EQ16
std::string getProfileDir(); // .../TakeMeIn/EQ16/Profiles
std::string getLastSessionPath();

//------------------------------------------------------------------------
// Low-level state file I/O (text format, forward compatible).
//------------------------------------------------------------------------
bool saveStateToFile(const std::string &path, const EQState &s);
bool loadStateFromFile(const std::string &path, EQState &s);

//------------------------------------------------------------------------
// Last-session convenience wrappers — used for restore-on-launch.
//------------------------------------------------------------------------
bool saveLastSession(const EQState &s);
bool loadLastSession(EQState &s);

//------------------------------------------------------------------------
// Named profile management.
//------------------------------------------------------------------------
std::vector<ProfileEntry> listProfiles();
bool saveProfile(const std::string &name, const EQState &s);
bool deleteProfile(const std::string &path);
std::string sanitizeName(const std::string &raw);

//------------------------------------------------------------------------
// UI preferences (currently just the zoom factor).
//------------------------------------------------------------------------
double loadUIScale(); // returns 1.0 if unset
bool saveUIScale(double scale);

//------------------------------------------------------------------------
} // namespace TakeMeIn
