//------------------------------------------------------------------------
// TakeMeIn 16-Band Parametric Equalizer
// Persistence + Profile management implementation
// Copyright(c) 2024 TakeMeIn — Made by LCaballerot01
//------------------------------------------------------------------------

#include "eqpersist.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

namespace TakeMeIn {

//------------------------------------------------------------------------
static fs::path baseConfigPath() {
#if defined(_WIN32)
  const char *appdata = std::getenv("APPDATA");
  fs::path base = appdata ? fs::path(appdata) : fs::temp_directory_path();
#elif defined(__APPLE__)
  const char *home = std::getenv("HOME");
  fs::path base = home ? (fs::path(home) / "Library" / "Application Support")
                       : fs::temp_directory_path();
#else
  const char *home = std::getenv("HOME");
  fs::path base = home ? (fs::path(home) / ".config") : fs::temp_directory_path();
#endif
  return base / "TakeMeIn" / "EQ16";
}

//------------------------------------------------------------------------
std::string getConfigDir() {
  fs::path dir = baseConfigPath();
  std::error_code ec;
  fs::create_directories(dir, ec);
  return dir.string();
}

//------------------------------------------------------------------------
std::string getProfileDir() {
  fs::path dir = baseConfigPath() / "Profiles";
  std::error_code ec;
  fs::create_directories(dir, ec);
  return dir.string();
}

//------------------------------------------------------------------------
std::string getLastSessionPath() {
  return (baseConfigPath() / "last_session.tmieq").string();
}

//------------------------------------------------------------------------
bool saveStateToFile(const std::string &path, const EQState &s) {
  std::ofstream f(path, std::ios::out | std::ios::trunc);
  if (!f.is_open())
    return false;

  f << "TAKEMEIN_EQ 1\n";
  f << "BANDS " << kNumBands << "\n";
  f << "GAIN";
  for (int i = 0; i < kNumBands; ++i)
    f << ' ' << s.gainDB[i];
  f << "\n";
  f << "Q";
  for (int i = 0; i < kNumBands; ++i)
    f << ' ' << s.q[i];
  f << "\n";
  f << "BYPASS " << s.bypass << "\n";
  f << "OUT " << s.outGainDB << "\n";
  return f.good();
}

//------------------------------------------------------------------------
bool loadStateFromFile(const std::string &path, EQState &s) {
  std::ifstream f(path);
  if (!f.is_open())
    return false;

  std::string token;
  if (!(f >> token) || token != "TAKEMEIN_EQ")
    return false;
  int version = 0;
  f >> version;

  bool gotGain = false;
  while (f >> token) {
    if (token == "BANDS") {
      int n = 0;
      f >> n; // tolerated but layout is fixed at kNumBands
      (void)n;
    } else if (token == "GAIN") {
      for (int i = 0; i < kNumBands; ++i)
        f >> s.gainDB[i];
      gotGain = true;
    } else if (token == "Q") {
      for (int i = 0; i < kNumBands; ++i)
        f >> s.q[i];
    } else if (token == "BYPASS") {
      f >> s.bypass;
    } else if (token == "OUT") {
      f >> s.outGainDB;
    } else {
      // skip rest of unknown line
      std::string rest;
      std::getline(f, rest);
    }
  }
  return gotGain;
}

//------------------------------------------------------------------------
bool saveLastSession(const EQState &s) {
  getConfigDir(); // ensure dir exists
  return saveStateToFile(getLastSessionPath(), s);
}

//------------------------------------------------------------------------
bool loadLastSession(EQState &s) {
  return loadStateFromFile(getLastSessionPath(), s);
}

//------------------------------------------------------------------------
std::string sanitizeName(const std::string &raw) {
  std::string out;
  out.reserve(raw.size());
  for (char c : raw) {
    if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' ||
        c == '"' || c == '<' || c == '>' || c == '|')
      continue;
    out.push_back(c);
  }
  // trim leading/trailing spaces and dots
  size_t b = out.find_first_not_of(" .");
  size_t e = out.find_last_not_of(" .");
  if (b == std::string::npos)
    return "";
  return out.substr(b, e - b + 1);
}

//------------------------------------------------------------------------
std::vector<ProfileEntry> listProfiles() {
  std::vector<ProfileEntry> out;
  std::error_code ec;
  fs::path dir = getProfileDir();
  if (!fs::exists(dir, ec))
    return out;
  for (auto &de : fs::directory_iterator(dir, ec)) {
    if (ec)
      break;
    if (!de.is_regular_file())
      continue;
    if (de.path().extension() != ".tmieq")
      continue;
    ProfileEntry e;
    e.name = de.path().stem().string();
    e.path = de.path().string();
    out.push_back(std::move(e));
  }
  std::sort(out.begin(), out.end(), [](const ProfileEntry &a, const ProfileEntry &b) {
    return a.name < b.name;
  });
  return out;
}

//------------------------------------------------------------------------
bool saveProfile(const std::string &name, const EQState &s) {
  std::string clean = sanitizeName(name);
  if (clean.empty())
    return false;
  fs::path path = fs::path(getProfileDir()) / (clean + ".tmieq");
  return saveStateToFile(path.string(), s);
}

//------------------------------------------------------------------------
bool deleteProfile(const std::string &path) {
  std::error_code ec;
  return fs::remove(path, ec);
}

//------------------------------------------------------------------------
static EQState makePreset(std::initializer_list<float> gains,
                          std::initializer_list<float> qs) {
  EQState s;
  int i = 0;
  for (float g : gains)
    if (i < kNumBands) s.gainDB[i++] = g;
  i = 0;
  for (float q : qs)
    if (i < kNumBands) s.q[i++] = q;
  return s;
}

const std::vector<FactoryPreset> &factoryPresets() {
  // Band order: 25 40 63 100 160 250 400 630 1k 1.6k 2.5k 4k 6.3k 10k 16k 20k
  static const std::vector<FactoryPreset> presets = {
      {"Vocal Clarity",
       makePreset({0, 0, -2, -3, -2, -1.5f, 0, 1, 2, 3, 3.5f, 3, 2, 1.5f, 1, 0},
                  {1, 1, 1.2f, 1.4f, 1.4f, 1.2f, 1, 1, 1, 1.2f, 1.4f, 1.4f, 1.2f, 1, 1, 1})},
      {"Bass Boost",
       makePreset({4, 5.5f, 6, 5, 3.5f, 2, 0.5f, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                  {0.8f, 0.8f, 0.9f, 1, 1.1f, 1.2f, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1})},
      {"Air",
       makePreset({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.5f, 1.5f, 3, 4.5f, 5.5f, 6},
                  {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0.9f, 0.8f, 0.8f, 0.7f, 0.7f})},
      {"De-Mud",
       makePreset({0, 0, -1, -2.5f, -4, -4.5f, -3, -1.5f, 0, 0, 0, 0, 0, 0, 0, 0},
                  {1, 1, 1.4f, 1.8f, 2, 2, 1.8f, 1.4f, 1, 1, 1, 1, 1, 1, 1, 1})},
      {"Loudness Smile",
       makePreset({4.5f, 4, 3, 2, 0.5f, -0.5f, -1.5f, -2, -2, -1.5f, -0.5f, 1, 2, 3, 4, 4.5f},
                  {0.9f, 0.9f, 1, 1, 1, 1.1f, 1.1f, 1.1f, 1.1f, 1.1f, 1, 1, 1, 1, 0.9f, 0.9f})},
  };
  return presets;
}

//------------------------------------------------------------------------
static std::string uiPrefPath() {
  return (baseConfigPath() / "ui.cfg").string();
}

double loadUIScale() {
  std::ifstream f(uiPrefPath());
  std::string token;
  double v = 1.0;
  while (f >> token) {
    if (token == "SCALE")
      f >> v;
    else {
      std::string rest;
      std::getline(f, rest);
    }
  }
  if (v < 0.5 || v > 2.0)
    v = 1.0;
  return v;
}

bool saveUIScale(double scale) {
  getConfigDir();
  std::ofstream f(uiPrefPath(), std::ios::out | std::ios::trunc);
  if (!f.is_open())
    return false;
  f << "SCALE " << scale << "\n";
  return f.good();
}

//------------------------------------------------------------------------
} // namespace TakeMeIn
