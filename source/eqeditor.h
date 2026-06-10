//------------------------------------------------------------------------
// TakeMeIn 16-Band Parametric Equalizer - Custom GUI Editor
// Copyright(c) 2024 TakeMeIn — Made by LCaballerot01
//------------------------------------------------------------------------

#pragma once

#include "eqcids.h"
#include "eqpersist.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "vstgui/lib/cvstguitimer.h"
#include "vstgui/vstgui.h"
#include <atomic>
#include <string>
#include <vector>

namespace TakeMeIn {

//------------------------------------------------------------------------
// Shared spectrum data — processor writes per-band levels, editor reads
//------------------------------------------------------------------------
struct SpectrumData {
  std::atomic<float> bandLevels[kNumBands]{};
  std::atomic<float> outPeak{0.f}; // post-gain output peak per block
};

extern SpectrumData g_spectrumData;

//------------------------------------------------------------------------
// Layout constants
//------------------------------------------------------------------------
static constexpr int kEditorWidth = 1060;
static constexpr int kEditorHeight = 700;

static constexpr int kHeaderHeight = 52;
static constexpr int kSpectrumTop = 62;
static constexpr int kSpectrumHeight = 264;
static constexpr int kBandSliderTop = 372;
static constexpr int kBandSliderBottom = 596;
static constexpr int kFreqLabelY = 640;
static constexpr int kQLabelY = 656;
static constexpr int kBandWidth = 58;
static constexpr int kBandStartX = 42;

// DJ fader cap geometry
static constexpr double kFaderCapW = 30.0; // cap width
static constexpr double kFaderCapH = 17.0; // cap height

class EQController;

//------------------------------------------------------------------------
//  EQMainView — Custom drawn premium EQ editor view
//------------------------------------------------------------------------
class EQMainView : public VSTGUI::CView {
public:
  enum class Tab { Equalizer, Config };

  EQMainView(const VSTGUI::CRect &size, EQController *controller);
  ~EQMainView() override;

  void draw(VSTGUI::CDrawContext *ctx) override;
  VSTGUI::CMouseEventResult
  onMouseDown(VSTGUI::CPoint &where,
              const VSTGUI::CButtonState &buttons) override;
  VSTGUI::CMouseEventResult
  onMouseMoved(VSTGUI::CPoint &where,
               const VSTGUI::CButtonState &buttons) override;
  VSTGUI::CMouseEventResult
  onMouseUp(VSTGUI::CPoint &where,
            const VSTGUI::CButtonState &buttons) override;
  VSTGUI::CMouseEventResult onMouseExited(VSTGUI::CPoint &where,
                                          const VSTGUI::CButtonState &buttons) override;
  bool onWheel(const VSTGUI::CPoint &where, const VSTGUI::CMouseWheelAxis &axis,
               const float &distance,
               const VSTGUI::CButtonState &buttons) override;
  void onKeyboardEvent(VSTGUI::KeyboardEvent &event) override;

  CLASS_METHODS_NOCOPY(EQMainView, CView)

protected:
  void onTimer();

  // ---- shared chrome ----
  void drawBackground(VSTGUI::CDrawContext *ctx);
  void drawRGBOutline(VSTGUI::CDrawContext *ctx);
  void drawHeader(VSTGUI::CDrawContext *ctx);
  void drawTabs(VSTGUI::CDrawContext *ctx);
  void drawBypassButton(VSTGUI::CDrawContext *ctx);
  void drawRangeButton(VSTGUI::CDrawContext *ctx);

  // ---- EQ page ----
  void drawSpectrum(VSTGUI::CDrawContext *ctx);
  void drawEQCurve(VSTGUI::CDrawContext *ctx, const VSTGUI::CRect &area);
  void drawBands(VSTGUI::CDrawContext *ctx);
  void drawSingleBand(VSTGUI::CDrawContext *ctx, int band, VSTGUI::CCoord x);
  void drawFaderCap(VSTGUI::CDrawContext *ctx, VSTGUI::CCoord cx,
                    VSTGUI::CCoord cy, float emphasis, bool active);
  void drawOutputFader(VSTGUI::CDrawContext *ctx);
  void drawABButtons(VSTGUI::CDrawContext *ctx);

  // ---- Config page ----
  void drawConfigPage(VSTGUI::CDrawContext *ctx);
  void drawConfigButton(VSTGUI::CDrawContext *ctx, const VSTGUI::CRect &r,
                        const char *label, bool hovered, bool primary,
                        float clickAnim);
  void drawProfileList(VSTGUI::CDrawContext *ctx, const VSTGUI::CRect &area);

  // ---- hit testing ----
  int hitTestBand(VSTGUI::CPoint &where);
  bool hitTestBypass(VSTGUI::CPoint &where);
  bool hitTestRange(VSTGUI::CPoint &where);
  int hitTestTab(VSTGUI::CPoint &where); // -1 none, 0 EQ, 1 Config
  bool hitTestOut(VSTGUI::CPoint &where);
  VSTGUI::CRect tabRect(int idx) const;
  VSTGUI::CRect bypassRect() const;
  VSTGUI::CRect rangeRect() const;
  VSTGUI::CRect abRect() const;
  VSTGUI::CRect copyRect() const;
  VSTGUI::CRect spectrumInnerRect() const; // curve-drag interaction area
  int nearestBandForX(VSTGUI::CCoord x) const;
  float curveYToGainNorm(VSTGUI::CCoord y) const;
  void sendBandGain(int band, float norm, bool begin, bool end);
  void toggleSolo(int band);

  // Config page rects
  VSTGUI::CRect cfgSaveProfileRect() const;
  VSTGUI::CRect cfgExportRect() const;
  VSTGUI::CRect cfgImportRect() const;
  VSTGUI::CRect cfgResetRect() const;
  VSTGUI::CRect cfgListArea() const;
  VSTGUI::CRect cfgRowRect(int visibleIdx) const;
  int cfgRowAt(VSTGUI::CPoint &where); // visible row index or -1
  VSTGUI::CRect cfgScaleRect(int idx) const; // 0=75%, 1=100%, 2=125%

  float yToGainNorm(VSTGUI::CCoord y);
  VSTGUI::CCoord gainNormToY(float norm);

  // ---- state collection / application ----
  EQState collectState() const;
  void applyState(const EQState &s);
  void refreshProfiles();
  void doSaveProfileDialog();
  void doExportDialog();
  void doImportDialog();
  void doResetFlat();

  EQController *mController = nullptr;
  VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> mTimer;

  Tab mTab = Tab::Equalizer;
  float mTabAnim = 0.f; // 0 = EQ, 1 = Config (animated)

  float mGainNorm[kNumBands]{};
  float mQNorm[kNumBands]{};
  bool mBypass = false;

  int mDragBand = -1;
  bool mDragging = false;
  int mHoverBand = -1;
  float mBandEmphasis[kNumBands]{}; // hover/drag emphasis per band, animated

  // Fine-drag (shift) support
  bool mFineDrag = false;
  float mFineAnchorNorm = 0.f;
  VSTGUI::CCoord mFineAnchorY = 0;

  // Button animation
  bool mBypassHover = false;
  float mBypassAnimPos = 0.f;
  float mBypassGlow = 0.f;

  // Smooth spectrum display levels
  float mDisplayLevels[kNumBands]{};
  float mPeakHold[kNumBands]{};
  float mPeakTimer[kNumBands]{};

  // Range toggle (±24dB / ±12dB)
  bool mRangeIs12dB = false;
  bool mRangeHover = false;
  float mRangeAnimPos = 0.f;
  float mRangeGlow = 0.f;
  float mRangeClickAnim = 0.f;

  // Tab hover
  int mTabHover = -1;

  // RGB outline animation
  float mFrameCounter = 0.f;

  // ---- Output gain + clip indicator ----
  float mOutGainNorm = 0.5f;
  bool mDraggingOut = false;
  bool mOutFineDrag = false;
  float mOutEmphasis = 0.f;
  bool mOutHover = false;
  float mClipTimer = 0.f; // >0 → clip LED lit (sticky)

  // ---- A/B compare ----
  EQState mSlotA, mSlotB;
  bool mSlotIsB = false;
  bool mSlotsInit = false; // capture current state into both slots once
  bool mABHover = false, mCopyHover = false;
  float mCopyClickAnim = 0.f;

  // ---- Solo (Ctrl+click a band) ----
  int mSoloBand = -1;

  // ---- Curve-drag editing ----
  bool mCurveDragging = false; // mDragBand holds the grabbed band

  // ---- UI scale ----
  double mUIScale = 1.0;
  int mScaleHover = -1;

  // ---- Config page state ----
  // List rows: factory presets (factoryIdx >= 0) followed by user profiles
  struct ListRow {
    std::string name;
    std::string path;    // empty for factory rows
    int factoryIdx = -1; // index into factoryPresets(), -1 = user profile
  };
  std::vector<ListRow> mProfiles;
  int mCfgHoverRow = -1;       // visible row hover
  int mCfgHoverZone = 0;       // 0 row, 1 load btn, 2 delete btn
  int mCfgScroll = 0;          // first visible profile index
  int mCfgHoverButton = -1;    // 0 save, 1 export, 2 import, 3 reset
  float mCfgBtnClickAnim[4]{};
  std::string mStatusMsg;
  float mStatusTimer = 0.f;    // seconds remaining for status message
  bool mStatusIsError = false;

  // Inline name entry for "Save Profile"
  bool mNaming = false;
  std::string mNameBuffer;
  float mCaretBlink = 0.f;

  // dirty tracking for last-session autosave (debounced)
  float mAutosaveCooldown = 0.f;
  bool mAutosaveDirty = false;

  // True once the timer has pulled real parameter values from the controller
  bool mSyncedOnce = false;

  // Adaptive frame rate: 60 fps while the host window is focused, 15 when not
  int mTimerIntervalMs = 16;
  float mDt = 1.f / 60.f; // seconds per frame, follows the active interval
  float mFocusCheckAccum = 0.f;
};

//------------------------------------------------------------------------
} // namespace TakeMeIn
