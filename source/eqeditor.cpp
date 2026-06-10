//------------------------------------------------------------------------
// TakeMeIn 16-Band Parametric Equalizer - Custom GUI Editor
// Copyright(c) 2024 TakeMeIn — Made by LCaballerot01
//------------------------------------------------------------------------

#include "eqeditor.h"
#include "eqcids.h"
#include "eqcontroller.h"
#include "vstgui/lib/cfileselector.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

#if defined(_WIN32)
#include <windows.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace VSTGUI;

namespace TakeMeIn {

SpectrumData g_spectrumData;

//------------------------------------------------------------------------
// Palette — deep graphite console with cyan signal accents
//------------------------------------------------------------------------
static const CColor kBgTop(8, 11, 18, 255);
static const CColor kBgBottom(17, 23, 36, 255);
static const CColor kPanelBg(10, 14, 22, 255);
static const CColor kPanelBg2(13, 18, 28, 255);
static const CColor kPanelBorder(38, 50, 70, 120);
static const CColor kAccent(6, 182, 212, 255);
static const CColor kAccentBright(94, 234, 252, 255);
static const CColor kAccentDim(6, 182, 212, 60);
static const CColor kCut(251, 146, 60, 255); // amber for cuts
static const CColor kTextPrimary(241, 245, 249, 255);
static const CColor kTextSecondary(148, 163, 184, 255);
static const CColor kTextDim(100, 116, 139, 255);
static const CColor kTextFaint(71, 85, 105, 255);
static const CColor kBypassOn(34, 197, 94, 255);
static const CColor kBypassOff(239, 68, 68, 255);
static const CColor kGridLine(30, 41, 59, 80);
static const CColor kZeroLine(120, 140, 165, 60);
static const CColor kRangeA(168, 85, 247, 255);
static const CColor kRangeB(59, 130, 246, 255);
static const CColor kDanger(239, 68, 68, 255);

static const char *kFontName = "Segoe UI";

//------------------------------------------------------------------------
static double normToGainDB(double n) {
  return kGainMinDB + n * (kGainMaxDB - kGainMinDB);
}
static double normToQ(double n) { return kQMin + n * (kQMax - kQMin); }
static float lerp(float a, float b, float t) { return a + (b - a) * t; }
static float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

//------------------------------------------------------------------------
// HSL to RGB helper for the RGB outline
//------------------------------------------------------------------------
static CColor hslToColor(float h, float s, float l, uint8_t alpha) {
  auto hue2rgb = [](float p, float q, float t) {
    if (t < 0.f) t += 1.f;
    if (t > 1.f) t -= 1.f;
    if (t < 1.f / 6.f) return p + (q - p) * 6.f * t;
    if (t < 1.f / 2.f) return q;
    if (t < 2.f / 3.f) return p + (q - p) * (2.f / 3.f - t) * 6.f;
    return p;
  };
  float q = (l < 0.5f) ? l * (1.f + s) : l + s - l * s;
  float p = 2.f * l - q;
  float r = hue2rgb(p, q, h + 1.f / 3.f);
  float g = hue2rgb(p, q, h);
  float b = hue2rgb(p, q, h - 1.f / 3.f);
  return CColor(static_cast<uint8_t>(r * 255), static_cast<uint8_t>(g * 255),
                static_cast<uint8_t>(b * 255), alpha);
}

//------------------------------------------------------------------------
// Peaking biquad magnitude in dB at frequency f (for the response curve)
//------------------------------------------------------------------------
static double peakingMagDB(double f, double fc, double gainDB, double Q,
                           double sr) {
  double A = std::pow(10.0, gainDB / 40.0);
  double w0 = 2.0 * M_PI * fc / sr;
  double alpha = std::sin(w0) / (2.0 * Q);

  double b0 = 1.0 + alpha * A;
  double b1 = -2.0 * std::cos(w0);
  double b2 = 1.0 - alpha * A;
  double a0 = 1.0 + alpha / A;
  double a1 = b1;
  double a2 = 1.0 - alpha / A;

  double w = 2.0 * M_PI * f / sr;
  double cw = std::cos(w), c2w = std::cos(2.0 * w);
  double sw = std::sin(w), s2w = std::sin(2.0 * w);

  double nr = b0 + b1 * cw + b2 * c2w;
  double ni = -(b1 * sw + b2 * s2w);
  double dr = a0 + a1 * cw + a2 * c2w;
  double di = -(a1 * sw + a2 * s2w);

  double num = nr * nr + ni * ni;
  double den = dr * dr + di * di;
  if (den < 1e-20) return 0.0;
  return 10.0 * std::log10(num / den);
}

//------------------------------------------------------------------------
EQMainView::EQMainView(const CRect &size, EQController *controller)
    : CView(size), mController(controller) {
  setWantsFocus(true);
  for (int i = 0; i < kNumBands; ++i) {
    mGainNorm[i] = 0.5f;
    mQNorm[i] = static_cast<float>((kQDefault - kQMin) / (kQMax - kQMin));
    mDisplayLevels[i] = 0.f;
    mPeakHold[i] = 0.f;
    mPeakTimer[i] = 0.f;
  }
  mUIScale = loadUIScale();
  refreshProfiles();
  mTimer = makeOwned<CVSTGUITimer>([this](CVSTGUITimer *) { this->onTimer(); },
                                   mTimerIntervalMs);
}

EQMainView::~EQMainView() {
  if (mTimer)
    mTimer->stop();
  // Final snapshot so the next launch starts exactly where the user left off.
  // Only after at least one timer tick — before that our values are defaults,
  // not the real parameters, and saving would clobber the previous session.
  if (mController && mSyncedOnce)
    saveLastSession(collectState());
}

//------------------------------------------------------------------------
EQState EQMainView::collectState() const {
  EQState s;
  for (int i = 0; i < kNumBands; ++i) {
    s.gainDB[i] = static_cast<float>(normToGainDB(mGainNorm[i]));
    s.q[i] = static_cast<float>(normToQ(mQNorm[i]));
  }
  s.bypass = mBypass ? 1 : 0;
  s.outGainDB = static_cast<float>(kOutGainMinDB +
                                   mOutGainNorm * (kOutGainMaxDB - kOutGainMinDB));
  return s;
}

//------------------------------------------------------------------------
void EQMainView::applyState(const EQState &s) {
  if (auto *c = mController)
    c->applyEQState(s);
  mAutosaveDirty = true;
  mAutosaveCooldown = 1.0f;
}

//------------------------------------------------------------------------
void EQMainView::refreshProfiles() {
  mProfiles.clear();
  const auto &factory = factoryPresets();
  for (int i = 0; i < (int)factory.size(); ++i) {
    ListRow row;
    row.name = factory[i].name;
    row.factoryIdx = i;
    mProfiles.push_back(std::move(row));
  }
  for (auto &p : listProfiles()) {
    ListRow row;
    row.name = p.name;
    row.path = p.path;
    mProfiles.push_back(std::move(row));
  }
  if (mCfgScroll > (int)mProfiles.size() - 1)
    mCfgScroll = std::max(0, (int)mProfiles.size() - 1);
}

//------------------------------------------------------------------------
void EQMainView::onTimer() {
  if (!mController)
    return;

  // ---- Adaptive frame rate: 60 fps focused, 15 fps unfocused ----
  // Checked ~4x per second to keep the polling negligible.
  mFocusCheckAccum += mDt;
  if (mFocusCheckAccum >= 0.25f) {
    mFocusCheckAccum = 0.f;
    bool focused = true;
#if defined(_WIN32)
    DWORD fgPid = 0;
    if (HWND fg = GetForegroundWindow()) {
      GetWindowThreadProcessId(fg, &fgPid);
      focused = (fgPid == GetCurrentProcessId());
    }
#endif
    int wantMs = focused ? 16 : 66; // ~60 Hz / ~15 Hz
    if (wantMs != mTimerIntervalMs) {
      mTimerIntervalMs = wantMs;
      mDt = wantMs / 1000.f;
      if (mTimer)
        mTimer->setFireTime(static_cast<uint32_t>(wantMs));
    }
  }

  // Frame-rate independent smoothing: convert a per-33ms lerp factor into an
  // equivalent factor for the current dt.
  auto sm = [this](float f33) {
    return 1.f - std::pow(1.f - f33, mDt / 0.033f);
  };
  float decay85 = std::pow(0.85f, mDt / 0.033f);

  for (int i = 0; i < kNumBands; ++i) {
    mGainNorm[i] = static_cast<float>(
        mController->getParamNormalized(EQParams::kBandGainFirst + i));
    mQNorm[i] = static_cast<float>(
        mController->getParamNormalized(EQParams::kBandQFirst + i));

    float raw = g_spectrumData.bandLevels[i].load(std::memory_order_relaxed);
    float db = (raw > 0.0001f) ? (20.f * std::log10(raw) + 60.f) / 60.f : 0.f;
    db = clamp01(db);
    if (db > mDisplayLevels[i])
      mDisplayLevels[i] = lerp(mDisplayLevels[i], db, sm(0.7f)); // fast attack
    else
      mDisplayLevels[i] = lerp(mDisplayLevels[i], db, sm(0.20f)); // decay

    // Peak hold: hold ~1.2s then fall
    if (mDisplayLevels[i] >= mPeakHold[i]) {
      mPeakHold[i] = mDisplayLevels[i];
      mPeakTimer[i] = 1.2f;
    } else {
      mPeakTimer[i] -= mDt;
      if (mPeakTimer[i] <= 0.f)
        mPeakHold[i] = std::max(0.f, mPeakHold[i] - 0.36f * mDt);
    }

    bool emph = (i == mHoverBand) || (mDragging && i == mDragBand);
    mBandEmphasis[i] = lerp(mBandEmphasis[i], emph ? 1.f : 0.f, sm(0.25f));
  }
  mBypass = mController->getParamNormalized(EQParams::kBypassId) > 0.5;
  mOutGainNorm = static_cast<float>(
      mController->getParamNormalized(EQParams::kOutputGainId));
  {
    int s = static_cast<int>(
                mController->getParamNormalized(EQParams::kSoloBandId) *
                    kNumBands + 0.5f) - 1;
    mSoloBand = (s >= 0 && s < kNumBands) ? s : -1;
  }
  if (!mSlotsInit) {
    mSlotA = mSlotB = collectState();
    mSlotsInit = true;
  }
  mSyncedOnce = true;

  // Clip detection: sticky LED for ~2.5s
  if (g_spectrumData.outPeak.load(std::memory_order_relaxed) >= 0.999f)
    mClipTimer = 2.5f;
  else if (mClipTimer > 0.f)
    mClipTimer -= mDt;
  mOutEmphasis = lerp(mOutEmphasis,
                      (mOutHover || mDraggingOut) ? 1.f : 0.f, sm(0.25f));
  mBypassAnimPos = lerp(mBypassAnimPos, mBypass ? 1.f : 0.f, sm(0.18f));
  mBypassGlow = lerp(mBypassGlow, mBypassHover ? 1.f : 0.f, sm(0.15f));
  mRangeAnimPos = lerp(mRangeAnimPos, mRangeIs12dB ? 1.f : 0.f, sm(0.18f));
  mRangeGlow = lerp(mRangeGlow, mRangeHover ? 1.f : 0.f, sm(0.15f));
  mRangeClickAnim *= decay85;
  mCopyClickAnim *= decay85;
  mTabAnim = lerp(mTabAnim, mTab == Tab::Config ? 1.f : 0.f, sm(0.22f));
  for (float &a : mCfgBtnClickAnim)
    a *= decay85;
  mFrameCounter += 0.30f * mDt; // full RGB cycle every ~3.3s
  if (mFrameCounter > 1.f)
    mFrameCounter -= 1.f;
  mCaretBlink += mDt;
  if (mCaretBlink > 1.f)
    mCaretBlink -= 1.f;
  if (mStatusTimer > 0.f)
    mStatusTimer -= mDt;

  // Debounced last-session autosave
  if (mAutosaveDirty) {
    mAutosaveCooldown -= mDt;
    if (mAutosaveCooldown <= 0.f) {
      saveLastSession(collectState());
      mAutosaveDirty = false;
    }
  }
  invalid();
}

//------------------------------------------------------------------------
void EQMainView::draw(CDrawContext *ctx) {
  drawBackground(ctx);
  drawHeader(ctx);
  drawTabs(ctx);
  drawBypassButton(ctx);
  drawRangeButton(ctx);
  drawABButtons(ctx);

  if (mTab == Tab::Equalizer) {
    drawSpectrum(ctx);
    drawBands(ctx);
    drawOutputFader(ctx);
  } else {
    drawConfigPage(ctx);
  }
  drawRGBOutline(ctx);
  setDirty(false);
}

//------------------------------------------------------------------------
void EQMainView::drawBackground(CDrawContext *ctx) {
  CRect r = getViewSize();
  auto path = owned(ctx->createGraphicsPath());
  path->addRect(r);
  auto grad = owned(CGradient::create(0.0, 1.0, kBgTop, kBgBottom));
  ctx->fillLinearGradient(path, *grad, r.getTopLeft(), r.getBottomLeft());

  // Subtle radial-feel sheen at the top (drawn as a faded band)
  CRect sheen(r.left, r.top, r.right, r.top + 140);
  auto sp = owned(ctx->createGraphicsPath());
  sp->addRect(sheen);
  auto sg = owned(CGradient::create(0.0, 1.0, CColor(80, 110, 160, 14),
                                    CColor(80, 110, 160, 0)));
  ctx->fillLinearGradient(sp, *sg, sheen.getTopLeft(), sheen.getBottomLeft());
}

//------------------------------------------------------------------------
// Flowing rainbow perimeter on a perfectly square frame. Each edge is drawn
// as filled rect segments (no line caps -> no dot artifacts), hue advancing
// along the perimeter and animated over time, with a travelling highlight.
//------------------------------------------------------------------------
void EQMainView::drawRGBOutline(CDrawContext *ctx) {
  CRect r = getViewSize();
  const double L = r.left, T = r.top, R = r.right, B = r.bottom;
  const double w = R - L, h = B - T;
  const double total = 2.0 * (w + h);

  float comet = std::fmod(mFrameCounter * 2.f, 1.f);

  // pass 0: inner soft glow band, pass 1: crisp core band
  const double widths[2] = {5.0, 2.0};
  const double segLen = 14.0;

  for (int pass = 0; pass < 2; ++pass) {
    double bw = widths[pass];
    double dist = 0.0;
    // Walk the perimeter clockwise: top, right, bottom, left. Each edge is
    // split into segments; each segment is a filled rect extending inward.
    for (int edge = 0; edge < 4; ++edge) {
      double edgeLen = (edge % 2 == 0) ? w : h;
      int n = std::max(1, static_cast<int>(edgeLen / segLen));
      for (int i = 0; i < n; ++i) {
        double d0 = edgeLen * i / n;
        double d1 = edgeLen * (i + 1) / n;
        double t = (dist + (d0 + d1) * 0.5) / total; // perimeter position

        float hue = std::fmod((float)t + mFrameCounter, 1.f);
        float dc = std::fabs((float)t - comet);
        dc = std::min(dc, 1.f - dc);
        float boost = std::max(0.f, 1.f - dc * 12.f);

        uint8_t alpha;
        float light;
        if (pass == 0) {
          alpha = static_cast<uint8_t>(22 + boost * 55);
          light = 0.55f;
        } else {
          alpha = static_cast<uint8_t>(140 + boost * 115);
          light = 0.55f + boost * 0.25f;
        }
        ctx->setFillColor(hslToColor(hue, 0.9f, light, alpha));

        CRect seg;
        switch (edge) {
        case 0: seg = CRect(L + d0, T, L + d1, T + bw); break;  // top
        case 1: seg = CRect(R - bw, T + d0, R, T + d1); break;  // right
        case 2: seg = CRect(R - d1, B - bw, R - d0, B); break;  // bottom
        default: seg = CRect(L, B - d1, L + bw, B - d0); break; // left
        }
        ctx->drawRect(seg, kDrawFilled);
      }
      dist += edgeLen;
    }
  }
}

//------------------------------------------------------------------------
void EQMainView::drawHeader(CDrawContext *ctx) {
  CRect r = getViewSize();

  // Header strip with a faint bottom separation line
  ctx->setLineWidth(1);
  ctx->setFrameColor(CColor(38, 50, 70, 90));
  ctx->drawLine(CPoint(r.left + 16, r.top + kHeaderHeight),
                CPoint(r.right - 16, r.top + kHeaderHeight));

  // Brand mark: glowing cyan dot
  CCoord dx = r.left + 18, dy = r.top + 20;
  CRect dotGlow(dx - 5, dy - 5, dx + 11, dy + 11);
  ctx->setFillColor(CColor(6, 182, 212, 50));
  ctx->drawEllipse(dotGlow, kDrawFilled);
  CRect dot(dx, dy, dx + 6, dy + 6);
  ctx->setFillColor(kAccentBright);
  ctx->drawEllipse(dot, kDrawFilled);

  // Title: heavy + light segments
  auto tf = makeOwned<CFontDesc>(kFontName, 18, kBoldFace);
  ctx->setFont(tf);
  ctx->setFontColor(kTextPrimary);
  ctx->drawString("TAKEMEIN",
                  CRect(r.left + 32, r.top + 10, r.left + 150, r.top + 36),
                  kLeftText);
  auto tl = makeOwned<CFontDesc>(kFontName, 18);
  ctx->setFont(tl);
  ctx->setFontColor(kAccent);
  ctx->drawString("EQ16",
                  CRect(r.left + 138, r.top + 10, r.left + 200, r.top + 36),
                  kLeftText);

  auto cf = makeOwned<CFontDesc>(kFontName, 9);
  ctx->setFont(cf);
  ctx->setFontColor(kTextFaint);
  ctx->drawString("16-BAND PARAMETRIC  \xC2\xB7  Made by LCaballerot01",
                  CRect(r.left + 32, r.top + 32, r.left + 320, r.top + 46),
                  kLeftText);
}

//------------------------------------------------------------------------
CRect EQMainView::tabRect(int idx) const {
  CRect r = getViewSize();
  CCoord cx = (r.left + r.right) / 2;
  CCoord w = 108, h = 28, gap = 2;
  CCoord top = r.top + 12;
  if (idx == 0)
    return CRect(cx - w - gap, top, cx - gap, top + h);
  return CRect(cx + gap, top, cx + w + gap, top + h);
}

CRect EQMainView::bypassRect() const {
  CRect r = getViewSize();
  return CRect(r.right - 135, r.top + 11, r.right - 20, r.top + 41);
}

CRect EQMainView::rangeRect() const {
  CRect r = getViewSize();
  return CRect(r.right - 245, r.top + 11, r.right - 150, r.top + 41);
}

CRect EQMainView::abRect() const {
  CRect r = getViewSize();
  return CRect(r.right - 340, r.top + 11, r.right - 296, r.top + 41);
}

CRect EQMainView::copyRect() const {
  CRect r = getViewSize();
  return CRect(r.right - 290, r.top + 11, r.right - 252, r.top + 41);
}

//------------------------------------------------------------------------
// A/B compare: toggle shows the active slot letter; the arrow button copies
// the current sound into the inactive slot.
//------------------------------------------------------------------------
void EQMainView::drawABButtons(CDrawContext *ctx) {
  CRect ab = abRect();
  auto bp = owned(ctx->createGraphicsPath());
  bp->addRoundRect(ab, 8);
  ctx->setFillColor(mABHover ? CColor(24, 34, 50, 255) : CColor(16, 23, 35, 255));
  ctx->drawGraphicsPath(bp, CDrawContext::kPathFilled);
  ctx->setLineWidth(mABHover ? 1.6 : 1.0);
  ctx->setFrameColor(CColor(6, 182, 212, mABHover ? 220 : 120));
  ctx->drawGraphicsPath(bp, CDrawContext::kPathStroked);

  // Both letters, active highlighted
  auto f = makeOwned<CFontDesc>(kFontName, 12, kBoldFace);
  ctx->setFont(f);
  CRect aHalf(ab.left, ab.top, (ab.left + ab.right) / 2, ab.bottom);
  CRect bHalf((ab.left + ab.right) / 2, ab.top, ab.right, ab.bottom);
  ctx->setFontColor(!mSlotIsB ? kAccentBright : kTextFaint);
  ctx->drawString("A", aHalf, kCenterText);
  ctx->setFontColor(mSlotIsB ? kAccentBright : kTextFaint);
  ctx->drawString("B", bHalf, kCenterText);

  // Copy button
  CRect cp = copyRect();
  auto cpp = owned(ctx->createGraphicsPath());
  cpp->addRoundRect(cp, 8);
  ctx->setFillColor(mCopyHover ? CColor(24, 34, 50, 255) : CColor(16, 23, 35, 255));
  ctx->drawGraphicsPath(cpp, CDrawContext::kPathFilled);
  if (mCopyClickAnim > 0.03f) {
    ctx->setFillColor(CColor(255, 255, 255, static_cast<uint8_t>(mCopyClickAnim * 60)));
    ctx->drawGraphicsPath(cpp, CDrawContext::kPathFilled);
  }
  ctx->setLineWidth(mCopyHover ? 1.6 : 1.0);
  ctx->setFrameColor(CColor(70, 85, 110, mCopyHover ? 220 : 120));
  ctx->drawGraphicsPath(cpp, CDrawContext::kPathStroked);
  auto cf = makeOwned<CFontDesc>(kFontName, 11, kBoldFace);
  ctx->setFont(cf);
  ctx->setFontColor(mCopyHover ? kTextPrimary : kTextDim);
  // copy arrow: current slot -> other slot
  ctx->drawString(mSlotIsB ? "\xE2\x86\x92" "A" : "\xE2\x86\x92" "B", cp,
                  kCenterText);
}

//------------------------------------------------------------------------
void EQMainView::drawTabs(CDrawContext *ctx) {
  // Pill container
  CRect t0 = tabRect(0), t1 = tabRect(1);
  CRect cont(t0.left - 5, t0.top - 5, t1.right + 5, t0.bottom + 5);
  auto cp = owned(ctx->createGraphicsPath());
  cp->addRoundRect(cont, 19);
  ctx->setFillColor(CColor(8, 11, 18, 220));
  ctx->drawGraphicsPath(cp, CDrawContext::kPathFilled);
  ctx->setLineWidth(1);
  ctx->setFrameColor(kPanelBorder);
  ctx->drawGraphicsPath(cp, CDrawContext::kPathStroked);

  // Sliding active indicator
  float a = mTabAnim;
  CRect ind(lerp((float)t0.left, (float)t1.left, a), (float)t0.top,
            lerp((float)t0.right, (float)t1.right, a), (float)t0.bottom);
  auto ip = owned(ctx->createGraphicsPath());
  ip->addRoundRect(ind, 14);
  auto ig = owned(CGradient::create(0.0, 1.0, CColor(10, 90, 110, 255),
                                    CColor(6, 60, 78, 255)));
  ctx->fillLinearGradient(ip, *ig, ind.getTopLeft(), ind.getBottomLeft());
  ctx->setLineWidth(1);
  ctx->setFrameColor(CColor(6, 182, 212, 160));
  ctx->drawGraphicsPath(ip, CDrawContext::kPathStroked);

  auto f = makeOwned<CFontDesc>(kFontName, 11, kBoldFace);
  ctx->setFont(f);
  const char *labels[2] = {"EQUALIZER", "CONFIG"};
  for (int i = 0; i < 2; ++i) {
    CRect tr = tabRect(i);
    bool active = (mTab == Tab::Config) == (i == 1);
    bool hov = mTabHover == i;
    ctx->setFontColor(active ? kTextPrimary
                             : (hov ? kTextSecondary : kTextDim));
    ctx->drawString(labels[i], tr, kCenterText);
  }
}

//------------------------------------------------------------------------
void EQMainView::drawBypassButton(CDrawContext *ctx) {
  CRect btnRect = bypassRect();
  CCoord btnX = btnRect.left, btnY = btnRect.top;

  uint8_t rr =
      static_cast<uint8_t>(lerp(kBypassOn.red, kBypassOff.red, mBypassAnimPos));
  uint8_t gg = static_cast<uint8_t>(
      lerp(kBypassOn.green, kBypassOff.green, mBypassAnimPos));
  uint8_t bb = static_cast<uint8_t>(
      lerp(kBypassOn.blue, kBypassOff.blue, mBypassAnimPos));

  if (mBypassGlow > 0.02f) {
    CRect glowR(btnRect.left - 4, btnRect.top - 4, btnRect.right + 4,
                btnRect.bottom + 4);
    auto gp = owned(ctx->createGraphicsPath());
    gp->addRoundRect(glowR, 20);
    ctx->setFillColor(CColor(rr, gg, bb, static_cast<uint8_t>(40 * mBypassGlow)));
    ctx->drawGraphicsPath(gp, CDrawContext::kPathFilled);
  }

  auto bp = owned(ctx->createGraphicsPath());
  bp->addRoundRect(btnRect, 15);
  ctx->setFillColor(CColor(rr / 4, gg / 4, bb / 4,
                           static_cast<uint8_t>(lerp(180.f, 200.f, mBypassGlow))));
  ctx->drawGraphicsPath(bp, CDrawContext::kPathFilled);

  ctx->setLineWidth(lerp(1.0f, 2.0f, mBypassGlow));
  ctx->setFrameColor(
      CColor(rr, gg, bb, static_cast<uint8_t>(lerp(100.f, 220.f, mBypassGlow))));
  ctx->drawGraphicsPath(bp, CDrawContext::kPathStroked);

  CCoord indicX = btnX + 15 + mBypassAnimPos * 84;
  CRect indicOuter(indicX - 7, btnY + 8, indicX + 7, btnY + 22);
  ctx->setFillColor(CColor(rr, gg, bb, 255));
  ctx->drawEllipse(indicOuter, kDrawFilled);
  CRect indicInner(indicX - 3, btnY + 12, indicX + 3, btnY + 18);
  ctx->setFillColor(CColor(255, 255, 255, 200));
  ctx->drawEllipse(indicInner, kDrawFilled);

  auto bf = makeOwned<CFontDesc>(kFontName, 10, kBoldFace);
  ctx->setFont(bf);
  ctx->setFontColor(kTextPrimary);
  ctx->drawString(mBypassAnimPos > 0.5f ? "BYPASS" : "ACTIVE",
                  CRect(btnX + 20, btnY + 5, btnX + 95, btnY + 25), kCenterText);
}

//------------------------------------------------------------------------
void EQMainView::drawRangeButton(CDrawContext *ctx) {
  CRect btnRect = rangeRect();

  uint8_t rr = static_cast<uint8_t>(lerp(kRangeA.red, kRangeB.red, mRangeAnimPos));
  uint8_t gg = static_cast<uint8_t>(lerp(kRangeA.green, kRangeB.green, mRangeAnimPos));
  uint8_t bb = static_cast<uint8_t>(lerp(kRangeA.blue, kRangeB.blue, mRangeAnimPos));

  float clickBright = mRangeClickAnim;

  if (mRangeGlow > 0.02f || clickBright > 0.02f) {
    float intensity = std::max(mRangeGlow * 0.3f, clickBright * 0.6f);
    CRect glowR(btnRect.left - 3, btnRect.top - 3, btnRect.right + 3,
                btnRect.bottom + 3);
    auto gp = owned(ctx->createGraphicsPath());
    gp->addRoundRect(glowR, 10);
    ctx->setFillColor(CColor(rr, gg, bb, static_cast<uint8_t>(intensity * 150)));
    ctx->drawGraphicsPath(gp, CDrawContext::kPathFilled);
  }

  auto bp = owned(ctx->createGraphicsPath());
  bp->addRoundRect(btnRect, 8);
  uint8_t bgBright = static_cast<uint8_t>(
      std::min(255.f, lerp(180.f, 210.f, mRangeGlow) + clickBright * 60.f));
  ctx->setFillColor(CColor(
      static_cast<uint8_t>(std::min(255, rr / 3 + (int)(clickBright * 80))),
      static_cast<uint8_t>(std::min(255, gg / 3 + (int)(clickBright * 80))),
      static_cast<uint8_t>(std::min(255, bb / 3 + (int)(clickBright * 80))),
      bgBright));
  ctx->drawGraphicsPath(bp, CDrawContext::kPathFilled);

  ctx->setLineWidth(lerp(1.0f, 2.2f, std::max(mRangeGlow, clickBright)));
  uint8_t ba = static_cast<uint8_t>(
      std::min(255.f, lerp(120.f, 240.f, mRangeGlow) + clickBright * 80.f));
  ctx->setFrameColor(CColor(rr, gg, bb, ba));
  ctx->drawGraphicsPath(bp, CDrawContext::kPathStroked);

  auto bf = makeOwned<CFontDesc>(kFontName, 11, kBoldFace);
  ctx->setFont(bf);
  ctx->setFontColor(CColor(255, 255, 255, 235));
  const char *label = mRangeAnimPos > 0.5f ? "\xC2\xB1" "12 dB" : "\xC2\xB1" "24 dB";
  ctx->drawString(label, btnRect, kCenterText);
}

//------------------------------------------------------------------------
void EQMainView::drawSpectrum(CDrawContext *ctx) {
  CRect r = getViewSize();
  CRect sr(r.left + 15, r.top + kSpectrumTop, r.right - 15,
           r.top + kSpectrumTop + kSpectrumHeight);

  auto sp = owned(ctx->createGraphicsPath());
  sp->addRoundRect(sr, 10);
  auto pg = owned(CGradient::create(0.0, 1.0, kPanelBg, kPanelBg2));
  ctx->fillLinearGradient(sp, *pg, sr.getTopLeft(), sr.getBottomLeft());
  ctx->setLineWidth(1.2);
  ctx->setFrameColor(CColor(6, 182, 212, 45));
  ctx->drawGraphicsPath(sp, CDrawContext::kPathStroked);

  CCoord innerL = sr.left + 36;
  CCoord innerR = sr.right - 10;
  CCoord innerT = sr.top + 24;
  CCoord innerB = sr.bottom - 18;
  CCoord innerH = innerB - innerT;

  // dB grid + labels (display range -60..0 dBFS)
  auto gf = makeOwned<CFontDesc>(kFontName, 7);
  ctx->setFont(gf);
  ctx->setLineWidth(0.5);
  for (int g = 0; g <= 4; ++g) {
    CCoord gy = innerT + innerH * g / 4.0;
    ctx->setFrameColor(CColor(40, 50, 65, g == 0 ? 50 : 30));
    ctx->drawLine(CPoint(innerL, gy), CPoint(innerR, gy));
    char lbl[8];
    snprintf(lbl, sizeof(lbl), "%d", -15 * g);
    ctx->setFontColor(CColor(80, 95, 115, 150));
    ctx->drawString(lbl, CRect(sr.left + 4, gy - 5, innerL - 4, gy + 6),
                    kRightText);
  }

  CCoord totalW = innerR - innerL;
  CCoord barW = totalW / kNumBands;
  CCoord gap = 5;

  for (int b = 0; b < kNumBands; ++b) {
    float level = mDisplayLevels[b];
    CCoord barH = level * innerH;
    if (barH < 1)
      barH = 1;

    CCoord x1 = innerL + b * barW + gap / 2;
    CCoord x2 = innerL + (b + 1) * barW - gap / 2;
    CCoord yTop = innerB - barH;

    auto barPath = owned(ctx->createGraphicsPath());
    barPath->addRoundRect(CRect(x1, yTop, x2, innerB), 3);
    int alpha = static_cast<int>(70 + level * 185);
    auto barGrad = owned(CGradient::create(
        0.0, 1.0, CColor(6, 90, 120, static_cast<uint8_t>(alpha / 2)),
        CColor(20, 210, 240, static_cast<uint8_t>(alpha))));
    ctx->fillLinearGradient(barPath, *barGrad, CPoint(x1, innerB),
                            CPoint(x1, yTop));

    if (barH > 4) {
      CRect cap(x1, yTop, x2, yTop + 2);
      ctx->setFillColor(CColor(94, 234, 252, static_cast<uint8_t>(140 + level * 115)));
      ctx->drawRect(cap, kDrawFilled);
    }

    // Peak-hold tick
    if (mPeakHold[b] > 0.02f) {
      CCoord py = innerB - mPeakHold[b] * innerH;
      ctx->setFillColor(CColor(255, 255, 255, 110));
      ctx->drawRect(CRect(x1 + 1, py - 1, x2 - 1, py), kDrawFilled);
    }

    auto ff = makeOwned<CFontDesc>(kFontName, 7);
    ctx->setFont(ff);
    ctx->setFontColor(CColor(100, 116, 139, 140));
    ctx->drawString(kBandLabels[b], CRect(x1 - 6, innerB + 3, x2 + 6, innerB + 14),
                    kCenterText);
  }

  // EQ response curve overlay
  drawEQCurve(ctx, CRect(innerL, innerT, innerR, innerB));

  // Caption
  auto wf = makeOwned<CFontDesc>(kFontName, 8, kBoldFace);
  ctx->setFont(wf);
  ctx->setFontColor(CColor(100, 130, 160, 190));
  ctx->drawString("SPECTRUM ANALYZER  +  EQ CURVE",
                  CRect(sr.left + 12, sr.top + 6, sr.left + 280, sr.top + 19),
                  kLeftText);
}

//------------------------------------------------------------------------
// Combined frequency response of all 16 peaking filters, drawn over the
// analyzer. ±27 dB maps to the area height.
//------------------------------------------------------------------------
void EQMainView::drawEQCurve(CDrawContext *ctx, const CRect &area) {
  const int N = 96;
  const double sr = 48000.0;
  const double fLo = 20.0, fHi = 20000.0;
  const double dbRange = 27.0;

  double gains[kNumBands], qs[kNumBands];
  bool flat = true;
  for (int b = 0; b < kNumBands; ++b) {
    gains[b] = normToGainDB(mGainNorm[b]);
    qs[b] = normToQ(mQNorm[b]);
    if (std::fabs(gains[b]) > 0.05)
      flat = false;
  }

  CCoord midY = (area.top + area.bottom) / 2.0;

  auto yFor = [&](double db) {
    double t = db / dbRange; // -1..1
    t = std::max(-1.0, std::min(1.0, t));
    return midY - t * (area.getHeight() / 2.0 - 2.0);
  };

  // Build the curve points
  CPoint pts[N];
  for (int i = 0; i < N; ++i) {
    double t = (double)i / (N - 1);
    double f = fLo * std::pow(fHi / fLo, t);
    double db = 0.0;
    if (!flat) {
      for (int b = 0; b < kNumBands; ++b) {
        if (std::fabs(gains[b]) < 0.02)
          continue;
        db += peakingMagDB(f, kBandFrequencies[b], gains[b], qs[b], sr);
      }
    }
    pts[i](area.left + t * area.getWidth(), yFor(db));
  }

  uint8_t curveAlpha = mBypass ? 60 : 255;

  // Filled area between curve and zero line
  auto fillPath = owned(ctx->createGraphicsPath());
  fillPath->beginSubpath(CPoint(area.left, midY));
  for (int i = 0; i < N; ++i)
    fillPath->addLine(pts[i]);
  fillPath->addLine(CPoint(area.right, midY));
  fillPath->closeSubpath();
  ctx->setFillColor(CColor(6, 182, 212, mBypass ? 8 : 22));
  ctx->drawGraphicsPath(fillPath, CDrawContext::kPathFilled);

  // Soft glow pass + crisp line
  auto curvePath = owned(ctx->createGraphicsPath());
  curvePath->beginSubpath(pts[0]);
  for (int i = 1; i < N; ++i)
    curvePath->addLine(pts[i]);

  ctx->setLineStyle(CLineStyle(CLineStyle::kLineCapRound, CLineStyle::kLineJoinRound));
  ctx->setLineWidth(5);
  ctx->setFrameColor(CColor(6, 182, 212, mBypass ? 14 : 45));
  ctx->drawGraphicsPath(curvePath, CDrawContext::kPathStroked);
  ctx->setLineWidth(2);
  ctx->setFrameColor(CColor(94, 234, 252, curveAlpha));
  ctx->drawGraphicsPath(curvePath, CDrawContext::kPathStroked);
}

//------------------------------------------------------------------------
void EQMainView::drawBands(CDrawContext *ctx) {
  CRect r = getViewSize();

  // Console panel behind the faders
  CRect panel(r.left + 15, r.top + kSpectrumTop + kSpectrumHeight + 12,
              r.right - 15, r.bottom - 14);
  auto pp = owned(ctx->createGraphicsPath());
  pp->addRoundRect(panel, 10);
  auto pg = owned(CGradient::create(0.0, 1.0, CColor(14, 19, 30, 255),
                                    CColor(10, 14, 22, 255)));
  ctx->fillLinearGradient(pp, *pg, panel.getTopLeft(), panel.getBottomLeft());
  ctx->setLineWidth(1);
  ctx->setFrameColor(kPanelBorder);
  ctx->drawGraphicsPath(pp, CDrawContext::kPathStroked);

  auto sf = makeOwned<CFontDesc>(kFontName, 8);
  ctx->setFont(sf);
  ctx->setFontColor(kTextDim);

  int maxDB = mRangeIs12dB ? 12 : 24;
  char topLbl[8], botLbl[8];
  snprintf(topLbl, sizeof(topLbl), "+%d", maxDB);
  snprintf(botLbl, sizeof(botLbl), "-%d", maxDB);

  CCoord lx = r.left + 6;
  ctx->drawString(topLbl, CRect(lx, kBandSliderTop - 5, lx + 28, kBandSliderTop + 7),
                  kRightText);
  CCoord my = (kBandSliderTop + kBandSliderBottom) / 2.0;
  ctx->drawString("0", CRect(lx, my - 5, lx + 28, my + 7), kRightText);
  ctx->drawString(botLbl,
                  CRect(lx, kBandSliderBottom - 5, lx + 28, kBandSliderBottom + 7),
                  kRightText);

  // Zero line across the console
  ctx->setLineWidth(0.8);
  ctx->setFrameColor(kZeroLine);
  ctx->drawLine(CPoint(kBandStartX - 4, my), CPoint(r.right - 24, my));

  for (int i = 0; i < kNumBands; ++i)
    drawSingleBand(ctx, i, kBandStartX + i * kBandWidth + kBandWidth / 2.0);
}

//------------------------------------------------------------------------
// DJ-mixer style line fader cap: wide rounded body, graphite gradient,
// grip ridges and a glowing center indicator line.
//------------------------------------------------------------------------
void EQMainView::drawFaderCap(CDrawContext *ctx, CCoord cx, CCoord cy,
                              float emphasis, bool active) {
  double w = kFaderCapW + emphasis * 3.0;
  double h = kFaderCapH + emphasis * 2.0;
  CRect cap(cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2);

  // Drop shadow
  CRect sh = cap;
  sh.offset(0, 2);
  auto shp = owned(ctx->createGraphicsPath());
  shp->addRoundRect(sh, 4);
  ctx->setFillColor(CColor(0, 0, 0, 110));
  ctx->drawGraphicsPath(shp, CDrawContext::kPathFilled);

  // Hover glow
  if (emphasis > 0.05f) {
    CRect gl(cap.left - 5, cap.top - 5, cap.right + 5, cap.bottom + 5);
    auto glp = owned(ctx->createGraphicsPath());
    glp->addRoundRect(gl, 7);
    ctx->setFillColor(CColor(6, 182, 212, static_cast<uint8_t>(30 * emphasis)));
    ctx->drawGraphicsPath(glp, CDrawContext::kPathFilled);
  }

  // Body: vertical graphite gradient
  auto bp = owned(ctx->createGraphicsPath());
  bp->addRoundRect(cap, 4);
  auto bg = owned(CGradient::create(0.0, 1.0, CColor(62, 70, 84, 255),
                                    CColor(24, 28, 36, 255)));
  ctx->fillLinearGradient(bp, *bg, cap.getTopLeft(), cap.getBottomLeft());

  // Edge highlight + outline
  ctx->setLineWidth(1);
  ctx->setFrameColor(CColor(110, 122, 140, emphasis > 0.3f ? 200 : 130));
  ctx->drawGraphicsPath(bp, CDrawContext::kPathStroked);

  // Grip ridges (two thin lines above + below center)
  ctx->setLineWidth(1);
  double inset = 6;
  for (int i = -1; i <= 1; i += 2) {
    double ry = cy + i * (h * 0.27);
    ctx->setFrameColor(CColor(0, 0, 0, 150));
    ctx->drawLine(CPoint(cap.left + inset, ry), CPoint(cap.right - inset, ry));
    ctx->setFrameColor(CColor(150, 160, 175, 60));
    ctx->drawLine(CPoint(cap.left + inset, ry + 1), CPoint(cap.right - inset, ry + 1));
  }

  // Center indicator line — the classic DJ fader stripe
  CColor stripe = active ? kAccentBright : CColor(190, 200, 215, 230);
  CRect mid(cap.left + 3, cy - 1.2, cap.right - 3, cy + 1.2);
  ctx->setFillColor(stripe);
  ctx->drawRect(mid, kDrawFilled);
  if (active) {
    // glow on the stripe while dragging/hovering
    CRect mglow(cap.left + 2, cy - 3, cap.right - 2, cy + 3);
    ctx->setFillColor(CColor(94, 234, 252, 50));
    ctx->drawRect(mglow, kDrawFilled);
  }
}

//------------------------------------------------------------------------
void EQMainView::drawSingleBand(CDrawContext *ctx, int band, CCoord x) {
  float emph = mBandEmphasis[band];

  // Track groove: dark inset slot with side highlight (like a mixer slot)
  CRect slot(x - 2.5, kBandSliderTop, x + 2.5, kBandSliderBottom);
  auto slp = owned(ctx->createGraphicsPath());
  slp->addRoundRect(slot, 2.5);
  ctx->setFillColor(CColor(4, 6, 10, 255));
  ctx->drawGraphicsPath(slp, CDrawContext::kPathFilled);
  ctx->setLineWidth(0.8);
  ctx->setFrameColor(CColor(70, 82, 100, 50));
  ctx->drawGraphicsPath(slp, CDrawContext::kPathStroked);

  // Ruler ticks beside the slot
  ctx->setLineWidth(0.6);
  ctx->setFrameColor(CColor(70, 82, 100, 60));
  for (int t = 0; t <= 8; ++t) {
    CCoord ty = kBandSliderTop + (kBandSliderBottom - kBandSliderTop) * t / 8.0;
    CCoord tw = (t == 4) ? 7 : 4;
    ctx->drawLine(CPoint(x - 8 - tw, ty), CPoint(x - 8, ty));
    ctx->drawLine(CPoint(x + 8, ty), CPoint(x + 8 + tw, ty));
  }

  CCoord thumbY = gainNormToY(mGainNorm[band]);
  CCoord midY = (kBandSliderTop + kBandSliderBottom) / 2.0;

  // Gain level line from center to cap — cyan boost, amber cut
  double db = normToGainDB(mGainNorm[band]);
  bool isCut = db < -0.05;
  bool isBoost = db > 0.05;
  if (isBoost || isCut) {
    CColor lvl = isBoost ? kAccent : kCut;
    ctx->setLineWidth(3);
    ctx->setFrameColor(CColor(lvl.red, lvl.green, lvl.blue,
                              static_cast<uint8_t>(120 + emph * 80)));
    ctx->drawLine(CPoint(x, midY), CPoint(x, thumbY));
  }

  // Q width arc around the cap while hovered/dragged: low Q (wide band) shows
  // a wide arc, high Q a narrow one.
  if (emph > 0.25f) {
    float qn = mQNorm[band];
    double spanDeg = 30.0 + (1.0 - qn) * 210.0;
    double a0 = -90.0 - spanDeg / 2.0, a1 = -90.0 + spanDeg / 2.0;
    double rad = 19.0 + emph * 2.0;
    auto arc = owned(ctx->createGraphicsPath());
    arc->addArc(CRect(x - rad, thumbY - rad, x + rad, thumbY + rad), a0, a1, true);
    ctx->setLineWidth(2);
    ctx->setFrameColor(CColor(94, 234, 252, static_cast<uint8_t>(150 * emph)));
    ctx->drawGraphicsPath(arc, CDrawContext::kPathStroked);
  }

  // The DJ fader cap
  bool active = emph > 0.4f;
  drawFaderCap(ctx, x, thumbY, emph, active);

  // Solo badge: amber ring + label on the soloed band
  if (mSoloBand == band) {
    double rad = 13.0;
    CRect ring(x - kFaderCapW / 2 - 4, thumbY - kFaderCapH / 2 - 4,
               x + kFaderCapW / 2 + 4, thumbY + kFaderCapH / 2 + 4);
    auto rp = owned(ctx->createGraphicsPath());
    rp->addRoundRect(ring, 6);
    ctx->setLineWidth(1.6);
    ctx->setFrameColor(kCut);
    ctx->drawGraphicsPath(rp, CDrawContext::kPathStroked);
    (void)rad;
    auto sf2 = makeOwned<CFontDesc>(kFontName, 7, kBoldFace);
    ctx->setFont(sf2);
    ctx->setFontColor(kCut);
    ctx->drawString("SOLO", CRect(x - 20, ring.bottom + 2, x + 20, ring.bottom + 13),
                    kCenterText);
  }

  // dB readout
  char dbt[12];
  snprintf(dbt, sizeof(dbt), "%+.1f", db);
  auto vf = makeOwned<CFontDesc>(kFontName, 8, kBoldFace);
  ctx->setFont(vf);
  CColor valCol = isBoost ? kAccentBright : (isCut ? kCut : kTextSecondary);
  if (emph > 0.3f)
    valCol = kTextPrimary;
  ctx->setFontColor(valCol);
  ctx->drawString(dbt, CRect(x - 22, kBandSliderTop - 16, x + 22, kBandSliderTop - 4),
                  kCenterText);

  // Frequency label
  auto ff = makeOwned<CFontDesc>(kFontName, 8, emph > 0.3f ? kBoldFace : 0);
  ctx->setFont(ff);
  ctx->setFontColor(emph > 0.3f ? kTextPrimary : kTextSecondary);
  ctx->drawString(kBandLabels[band], CRect(x - 24, kFreqLabelY, x + 24, kFreqLabelY + 12),
                  kCenterText);

  // Q label
  double qv = normToQ(mQNorm[band]);
  char qt[12];
  snprintf(qt, sizeof(qt), "Q %.1f", qv);
  auto qf = makeOwned<CFontDesc>(kFontName, 7);
  ctx->setFont(qf);
  ctx->setFontColor(kTextDim);
  ctx->drawString(qt, CRect(x - 20, kQLabelY, x + 20, kQLabelY + 11), kCenterText);
}

//------------------------------------------------------------------------
// Master output fader with clip LED, to the right of the 16 bands.
//------------------------------------------------------------------------
static constexpr double kOutFaderX = 1008.0;

void EQMainView::drawOutputFader(CDrawContext *ctx) {
  CRect r = getViewSize();
  CCoord x = r.left + kOutFaderX;

  // Separator between EQ bands and master section
  ctx->setLineWidth(1);
  ctx->setFrameColor(CColor(38, 50, 70, 110));
  ctx->drawLine(CPoint(r.left + 980, kBandSliderTop - 20),
                CPoint(r.left + 980, kBandSliderBottom + 40));

  // Slot
  CRect slot(x - 2.5, kBandSliderTop, x + 2.5, kBandSliderBottom);
  auto slp = owned(ctx->createGraphicsPath());
  slp->addRoundRect(slot, 2.5);
  ctx->setFillColor(CColor(4, 6, 10, 255));
  ctx->drawGraphicsPath(slp, CDrawContext::kPathFilled);
  ctx->setLineWidth(0.8);
  ctx->setFrameColor(CColor(70, 82, 100, 50));
  ctx->drawGraphicsPath(slp, CDrawContext::kPathStroked);

  // Ruler ticks
  ctx->setLineWidth(0.6);
  ctx->setFrameColor(CColor(70, 82, 100, 60));
  for (int t = 0; t <= 8; ++t) {
    CCoord ty = kBandSliderTop + (kBandSliderBottom - kBandSliderTop) * t / 8.0;
    CCoord tw = (t == 4) ? 7 : 4;
    ctx->drawLine(CPoint(x - 8 - tw, ty), CPoint(x - 8, ty));
    ctx->drawLine(CPoint(x + 8, ty), CPoint(x + 8 + tw, ty));
  }

  CCoord midY = (kBandSliderTop + kBandSliderBottom) / 2.0;
  CCoord thumbY = kBandSliderTop +
                  (1.0 - mOutGainNorm) * (kBandSliderBottom - kBandSliderTop);

  double db = kOutGainMinDB + mOutGainNorm * (kOutGainMaxDB - kOutGainMinDB);
  bool isBoost = db > 0.05, isCut = db < -0.05;
  if (isBoost || isCut) {
    CColor lvl = isBoost ? kAccent : kCut;
    ctx->setLineWidth(3);
    ctx->setFrameColor(CColor(lvl.red, lvl.green, lvl.blue,
                              static_cast<uint8_t>(120 + mOutEmphasis * 80)));
    ctx->drawLine(CPoint(x, midY), CPoint(x, thumbY));
  }

  drawFaderCap(ctx, x, thumbY, mOutEmphasis, mOutEmphasis > 0.4f);

  // dB readout
  char dbt[12];
  snprintf(dbt, sizeof(dbt), "%+.1f", db);
  auto vf = makeOwned<CFontDesc>(kFontName, 8, kBoldFace);
  ctx->setFont(vf);
  ctx->setFontColor(isBoost ? kAccentBright : (isCut ? kCut : kTextSecondary));
  ctx->drawString(dbt, CRect(x - 24, kBandSliderTop - 16, x + 24, kBandSliderTop - 4),
                  kCenterText);

  // OUT label
  auto ff = makeOwned<CFontDesc>(kFontName, 8, kBoldFace);
  ctx->setFont(ff);
  ctx->setFontColor(kTextSecondary);
  ctx->drawString("OUT", CRect(x - 24, kFreqLabelY, x + 24, kFreqLabelY + 12),
                  kCenterText);

  // Clip LED (click to reset)
  bool lit = mClipTimer > 0.f;
  CRect led(x - 5, kQLabelY + 1, x + 5, kQLabelY + 11);
  if (lit) {
    CRect glow(led.left - 4, led.top - 4, led.right + 4, led.bottom + 4);
    ctx->setFillColor(CColor(239, 68, 68, 70));
    ctx->drawEllipse(glow, kDrawFilled);
  }
  ctx->setFillColor(lit ? CColor(255, 70, 70, 255) : CColor(60, 24, 24, 255));
  ctx->drawEllipse(led, kDrawFilled);
  ctx->setLineWidth(0.8);
  ctx->setFrameColor(CColor(120, 50, 50, 160));
  ctx->drawEllipse(led, kDrawStroked);
  auto cf = makeOwned<CFontDesc>(kFontName, 6);
  ctx->setFont(cf);
  ctx->setFontColor(lit ? CColor(255, 130, 130, 255) : kTextFaint);
  ctx->drawString("CLIP", CRect(x - 20, kQLabelY + 13, x + 20, kQLabelY + 23),
                  kCenterText);
}

//------------------------------------------------------------------------
//  CONFIG PAGE
//------------------------------------------------------------------------
static constexpr int kCfgTop = kSpectrumTop;
static constexpr int kCfgRowH = 38;
static constexpr int kCfgVisibleRows = 10;

CRect EQMainView::cfgListArea() const {
  CRect r = getViewSize();
  return CRect(r.left + 30, r.top + kCfgTop + 60, r.left + 560,
               r.top + kCfgTop + 60 + kCfgVisibleRows * kCfgRowH);
}

CRect EQMainView::cfgRowRect(int visibleIdx) const {
  CRect a = cfgListArea();
  return CRect(a.left + 6, a.top + 6 + visibleIdx * kCfgRowH, a.right - 6,
               a.top + 6 + visibleIdx * kCfgRowH + kCfgRowH - 6);
}

CRect EQMainView::cfgSaveProfileRect() const {
  CRect r = getViewSize();
  return CRect(r.left + 600, r.top + kCfgTop + 60, r.right - 30,
               r.top + kCfgTop + 102);
}
CRect EQMainView::cfgExportRect() const {
  CRect r = cfgSaveProfileRect();
  r.offset(0, 56);
  return r;
}
CRect EQMainView::cfgImportRect() const {
  CRect r = cfgSaveProfileRect();
  r.offset(0, 112);
  return r;
}
CRect EQMainView::cfgResetRect() const {
  CRect r = cfgSaveProfileRect();
  r.offset(0, 168);
  return r;
}

CRect EQMainView::cfgScaleRect(int idx) const {
  // Three pills under the session info card
  CRect base = cfgResetRect();
  CCoord top = base.bottom + 26 + 96 + 16 + 20; // below the info card + heading
  CCoord left = base.left + idx * 100;
  return CRect(left, top, left + 88, top + 34);
}

//------------------------------------------------------------------------
void EQMainView::drawConfigButton(CDrawContext *ctx, const CRect &r,
                                  const char *label, bool hovered, bool primary,
                                  float clickAnim) {
  auto bp = owned(ctx->createGraphicsPath());
  bp->addRoundRect(r, 9);

  CColor base = primary ? CColor(8, 70, 90, 255) : CColor(20, 27, 40, 255);
  CColor baseHi = primary ? CColor(10, 95, 120, 255) : CColor(28, 37, 54, 255);
  auto g = owned(CGradient::create(0.0, 1.0, hovered ? baseHi : base,
                                   CColor(base.red / 2 + 4, base.green / 2 + 4,
                                          base.blue / 2 + 8, 255)));
  ctx->fillLinearGradient(bp, *g, r.getTopLeft(), r.getBottomLeft());

  if (clickAnim > 0.03f) {
    ctx->setFillColor(CColor(255, 255, 255, static_cast<uint8_t>(clickAnim * 50)));
    ctx->drawGraphicsPath(bp, CDrawContext::kPathFilled);
  }

  ctx->setLineWidth(hovered ? 1.6 : 1.0);
  CColor border = primary ? CColor(6, 182, 212, hovered ? 220 : 130)
                          : CColor(70, 85, 110, hovered ? 200 : 110);
  ctx->setFrameColor(border);
  ctx->drawGraphicsPath(bp, CDrawContext::kPathStroked);

  auto f = makeOwned<CFontDesc>(kFontName, 11, kBoldFace);
  ctx->setFont(f);
  ctx->setFontColor(hovered ? kTextPrimary : kTextSecondary);
  ctx->drawString(label, r, kCenterText);
}

//------------------------------------------------------------------------
void EQMainView::drawProfileList(CDrawContext *ctx, const CRect &area) {
  auto pp = owned(ctx->createGraphicsPath());
  pp->addRoundRect(area, 10);
  auto pg = owned(CGradient::create(0.0, 1.0, kPanelBg2, kPanelBg));
  ctx->fillLinearGradient(pp, *pg, area.getTopLeft(), area.getBottomLeft());
  ctx->setLineWidth(1);
  ctx->setFrameColor(kPanelBorder);
  ctx->drawGraphicsPath(pp, CDrawContext::kPathStroked);

  if (mProfiles.empty()) {
    auto f = makeOwned<CFontDesc>(kFontName, 11);
    ctx->setFont(f);
    ctx->setFontColor(kTextDim);
    ctx->drawString("No profiles yet — save your current mix on the right.",
                    area, kCenterText);
    return;
  }

  int count = std::min((int)mProfiles.size() - mCfgScroll, kCfgVisibleRows);
  for (int v = 0; v < count; ++v) {
    int idx = mCfgScroll + v;
    CRect row = cfgRowRect(v);
    bool hov = (mCfgHoverRow == v);

    auto rp = owned(ctx->createGraphicsPath());
    rp->addRoundRect(row, 7);
    ctx->setFillColor(hov ? CColor(28, 38, 56, 255) : CColor(16, 22, 33, 255));
    ctx->drawGraphicsPath(rp, CDrawContext::kPathFilled);
    if (hov) {
      ctx->setLineWidth(1);
      ctx->setFrameColor(CColor(6, 182, 212, 90));
      ctx->drawGraphicsPath(rp, CDrawContext::kPathStroked);
    }

    bool isFactory = mProfiles[idx].factoryIdx >= 0;

    // Profile name
    auto nf = makeOwned<CFontDesc>(kFontName, 11, hov ? kBoldFace : 0);
    ctx->setFont(nf);
    ctx->setFontColor(hov ? kTextPrimary : kTextSecondary);
    ctx->drawString(mProfiles[idx].name.c_str(),
                    CRect(row.left + 14, row.top, row.right - 220, row.bottom),
                    kLeftText);

    // FACTORY badge for built-in presets
    if (isFactory) {
      CRect badge(row.right - 210, row.top + 8, row.right - 140, row.bottom - 8);
      auto bp2 = owned(ctx->createGraphicsPath());
      bp2->addRoundRect(badge, 8);
      ctx->setFillColor(CColor(60, 45, 12, 200));
      ctx->drawGraphicsPath(bp2, CDrawContext::kPathFilled);
      ctx->setLineWidth(1);
      ctx->setFrameColor(CColor(251, 146, 60, 120));
      ctx->drawGraphicsPath(bp2, CDrawContext::kPathStroked);
      auto bf2 = makeOwned<CFontDesc>(kFontName, 7, kBoldFace);
      ctx->setFont(bf2);
      ctx->setFontColor(CColor(251, 180, 110, 230));
      ctx->drawString("FACTORY", badge, kCenterText);
    }

    // LOAD pill
    CRect loadBtn(row.right - 130, row.top + 5, row.right - 56, row.bottom - 5);
    bool loadHov = hov && mCfgHoverZone == 1;
    auto lp = owned(ctx->createGraphicsPath());
    lp->addRoundRect(loadBtn, 11);
    ctx->setFillColor(loadHov ? CColor(8, 95, 120, 255) : CColor(8, 55, 70, 255));
    ctx->drawGraphicsPath(lp, CDrawContext::kPathFilled);
    ctx->setLineWidth(1);
    ctx->setFrameColor(CColor(6, 182, 212, loadHov ? 230 : 110));
    ctx->drawGraphicsPath(lp, CDrawContext::kPathStroked);
    auto lf = makeOwned<CFontDesc>(kFontName, 9, kBoldFace);
    ctx->setFont(lf);
    ctx->setFontColor(loadHov ? kTextPrimary : CColor(140, 220, 235, 255));
    ctx->drawString("LOAD", loadBtn, kCenterText);

    // Delete × (user profiles only)
    if (isFactory)
      continue;
    CRect delBtn(row.right - 44, row.top + 5, row.right - 12, row.bottom - 5);
    bool delHov = hov && mCfgHoverZone == 2;
    if (delHov) {
      auto dp = owned(ctx->createGraphicsPath());
      dp->addRoundRect(delBtn, 8);
      ctx->setFillColor(CColor(120, 30, 30, 160));
      ctx->drawGraphicsPath(dp, CDrawContext::kPathFilled);
    }
    auto df = makeOwned<CFontDesc>(kFontName, 12, kBoldFace);
    ctx->setFont(df);
    ctx->setFontColor(delHov ? CColor(255, 160, 160, 255) : kTextDim);
    ctx->drawString("\xC3\x97", delBtn, kCenterText); // ×
  }

  // Scroll hint
  if ((int)mProfiles.size() > kCfgVisibleRows) {
    auto f = makeOwned<CFontDesc>(kFontName, 8);
    ctx->setFont(f);
    ctx->setFontColor(kTextFaint);
    char hint[64];
    snprintf(hint, sizeof(hint), "%d / %d  \xE2\x80\x94  scroll for more",
             mCfgScroll + 1, (int)mProfiles.size());
    ctx->drawString(hint, CRect(area.left, area.bottom - 16, area.right - 10,
                                area.bottom - 2),
                    kRightText);
  }
}

//------------------------------------------------------------------------
void EQMainView::drawConfigPage(CDrawContext *ctx) {
  CRect r = getViewSize();

  // Section headings
  auto hf = makeOwned<CFontDesc>(kFontName, 12, kBoldFace);
  ctx->setFont(hf);
  ctx->setFontColor(kTextSecondary);
  ctx->drawString("PROFILES",
                  CRect(r.left + 32, r.top + kCfgTop + 30, r.left + 300,
                        r.top + kCfgTop + 50),
                  kLeftText);
  ctx->drawString("ACTIONS",
                  CRect(r.left + 602, r.top + kCfgTop + 30, r.left + 900,
                        r.top + kCfgTop + 50),
                  kLeftText);

  drawProfileList(ctx, cfgListArea());

  // Action buttons
  if (mNaming) {
    // Inline name entry replaces the save button
    CRect nr = cfgSaveProfileRect();
    auto np = owned(ctx->createGraphicsPath());
    np->addRoundRect(nr, 9);
    ctx->setFillColor(CColor(6, 12, 20, 255));
    ctx->drawGraphicsPath(np, CDrawContext::kPathFilled);
    ctx->setLineWidth(1.5);
    ctx->setFrameColor(CColor(6, 182, 212, 220));
    ctx->drawGraphicsPath(np, CDrawContext::kPathStroked);

    std::string display = mNameBuffer;
    if (mCaretBlink < 0.5f)
      display += "|";
    auto f = makeOwned<CFontDesc>(kFontName, 12);
    ctx->setFont(f);
    ctx->setFontColor(kTextPrimary);
    if (mNameBuffer.empty() && mCaretBlink >= 0.5f) {
      ctx->setFontColor(kTextFaint);
      display = "type profile name\xE2\x80\xA6";
    }
    ctx->drawString(display.c_str(),
                    CRect(nr.left + 14, nr.top, nr.right - 10, nr.bottom),
                    kLeftText);

    auto sf = makeOwned<CFontDesc>(kFontName, 8);
    ctx->setFont(sf);
    ctx->setFontColor(kTextDim);
    ctx->drawString("ENTER to save  \xC2\xB7  ESC to cancel",
                    CRect(nr.left, nr.bottom + 4, nr.right, nr.bottom + 18),
                    kLeftText);
  } else {
    drawConfigButton(ctx, cfgSaveProfileRect(), "SAVE CURRENT AS PROFILE",
                     mCfgHoverButton == 0, true, mCfgBtnClickAnim[0]);
  }
  drawConfigButton(ctx, cfgExportRect(), "EXPORT TO FILE\xE2\x80\xA6",
                   mCfgHoverButton == 1, false, mCfgBtnClickAnim[1]);
  drawConfigButton(ctx, cfgImportRect(), "IMPORT FROM FILE\xE2\x80\xA6",
                   mCfgHoverButton == 2, false, mCfgBtnClickAnim[2]);
  drawConfigButton(ctx, cfgResetRect(), "RESET TO FLAT",
                   mCfgHoverButton == 3, false, mCfgBtnClickAnim[3]);

  // Session info card
  CRect info(r.left + 600, cfgResetRect().bottom + 26, r.right - 30,
             cfgResetRect().bottom + 122);
  auto ip = owned(ctx->createGraphicsPath());
  ip->addRoundRect(info, 10);
  ctx->setFillColor(CColor(12, 17, 27, 255));
  ctx->drawGraphicsPath(ip, CDrawContext::kPathFilled);
  ctx->setLineWidth(1);
  ctx->setFrameColor(kPanelBorder);
  ctx->drawGraphicsPath(ip, CDrawContext::kPathStroked);

  auto inf = makeOwned<CFontDesc>(kFontName, 9, kBoldFace);
  ctx->setFont(inf);
  ctx->setFontColor(CColor(100, 130, 160, 200));
  ctx->drawString("SESSION RECALL",
                  CRect(info.left + 14, info.top + 8, info.right, info.top + 24),
                  kLeftText);
  auto inb = makeOwned<CFontDesc>(kFontName, 9);
  ctx->setFont(inb);
  ctx->setFontColor(kTextDim);
  ctx->drawString("Your EQ is saved automatically while you tweak it and",
                  CRect(info.left + 14, info.top + 28, info.right - 10, info.top + 42),
                  kLeftText);
  ctx->drawString("restored on the next launch \xE2\x80\x94 your mix is always ready.",
                  CRect(info.left + 14, info.top + 42, info.right - 10, info.top + 56),
                  kLeftText);
  ctx->setFontColor(kTextFaint);
  std::string dirLine = std::string("Store: ") + getConfigDir();
  ctx->drawString(dirLine.c_str(),
                  CRect(info.left + 14, info.top + 62, info.right - 10, info.top + 78),
                  kLeftText);

  // UI scale section
  auto shf = makeOwned<CFontDesc>(kFontName, 12, kBoldFace);
  ctx->setFont(shf);
  ctx->setFontColor(kTextSecondary);
  CRect scaleHdr(info.left + 2, info.bottom + 14, info.right, info.bottom + 32);
  ctx->drawString("UI SCALE", scaleHdr, kLeftText);

  static const double scales[3] = {0.75, 1.0, 1.25};
  static const char *scaleLbl[3] = {"75%", "100%", "125%"};
  for (int i = 0; i < 3; ++i) {
    CRect sr2 = cfgScaleRect(i);
    bool active = std::fabs(mUIScale - scales[i]) < 0.01;
    bool hov = mScaleHover == i;
    auto sp2 = owned(ctx->createGraphicsPath());
    sp2->addRoundRect(sr2, 9);
    ctx->setFillColor(active ? CColor(8, 70, 90, 255)
                             : (hov ? CColor(24, 34, 50, 255)
                                    : CColor(16, 23, 35, 255)));
    ctx->drawGraphicsPath(sp2, CDrawContext::kPathFilled);
    ctx->setLineWidth(active || hov ? 1.6 : 1.0);
    ctx->setFrameColor(active ? CColor(6, 182, 212, 220)
                              : CColor(70, 85, 110, hov ? 200 : 110));
    ctx->drawGraphicsPath(sp2, CDrawContext::kPathStroked);
    auto sf3 = makeOwned<CFontDesc>(kFontName, 11, kBoldFace);
    ctx->setFont(sf3);
    ctx->setFontColor(active || hov ? kTextPrimary : kTextDim);
    ctx->drawString(scaleLbl[i], sr2, kCenterText);
  }

  // Status toast
  if (mStatusTimer > 0.f && !mStatusMsg.empty()) {
    float fade = std::min(1.f, mStatusTimer);
    CRect toast((r.left + r.right) / 2 - 220, r.bottom - 56,
                (r.left + r.right) / 2 + 220, r.bottom - 24);
    auto tp = owned(ctx->createGraphicsPath());
    tp->addRoundRect(toast, 16);
    CColor tbg = mStatusIsError ? CColor(70, 18, 18, 255) : CColor(8, 60, 76, 255);
    tbg.alpha = static_cast<uint8_t>(230 * fade);
    ctx->setFillColor(tbg);
    ctx->drawGraphicsPath(tp, CDrawContext::kPathFilled);
    ctx->setLineWidth(1);
    CColor tbr = mStatusIsError ? kDanger : kAccent;
    tbr.alpha = static_cast<uint8_t>(180 * fade);
    ctx->setFrameColor(tbr);
    ctx->drawGraphicsPath(tp, CDrawContext::kPathStroked);
    auto tf = makeOwned<CFontDesc>(kFontName, 10, kBoldFace);
    ctx->setFont(tf);
    CColor tc = kTextPrimary;
    tc.alpha = static_cast<uint8_t>(255 * fade);
    ctx->setFontColor(tc);
    ctx->drawString(mStatusMsg.c_str(), toast, kCenterText);
  }
}

//------------------------------------------------------------------------
//  Curve-drag + solo helpers
//------------------------------------------------------------------------
CRect EQMainView::spectrumInnerRect() const {
  CRect r = getViewSize();
  CRect sr(r.left + 15, r.top + kSpectrumTop, r.right - 15,
           r.top + kSpectrumTop + kSpectrumHeight);
  return CRect(sr.left + 36, sr.top + 24, sr.right - 10, sr.bottom - 18);
}

int EQMainView::nearestBandForX(CCoord x) const {
  CRect inner = spectrumInnerRect();
  double t = (x - inner.left) / inner.getWidth();
  t = std::max(0.0, std::min(1.0, t));
  double f = 20.0 * std::pow(1000.0, t); // 20 Hz .. 20 kHz log scale
  int best = 0;
  double bestDist = 1e30;
  for (int b = 0; b < kNumBands; ++b) {
    double d = std::fabs(std::log(f) - std::log(kBandFrequencies[b]));
    if (d < bestDist) {
      bestDist = d;
      best = b;
    }
  }
  return best;
}

float EQMainView::curveYToGainNorm(CCoord y) const {
  // Inverse of drawEQCurve's mapping: ±27 dB across the inner height
  CRect inner = spectrumInnerRect();
  double midY = (inner.top + inner.bottom) / 2.0;
  double db = (midY - y) / (inner.getHeight() / 2.0 - 2.0) * 27.0;
  db = std::max(kGainMinDB, std::min(kGainMaxDB, db));
  return static_cast<float>((db - kGainMinDB) / (kGainMaxDB - kGainMinDB));
}

void EQMainView::sendBandGain(int band, float norm, bool begin, bool end) {
  mGainNorm[band] = norm;
  if (mController) {
    auto pid = static_cast<Steinberg::Vst::ParamID>(EQParams::kBandGainFirst + band);
    if (begin)
      mController->beginEdit(pid);
    mController->setParamNormalized(pid, norm);
    mController->performEdit(pid, norm);
    if (end)
      mController->endEdit(pid);
  }
}

void EQMainView::toggleSolo(int band) {
  mSoloBand = (mSoloBand == band) ? -1 : band;
  if (mController) {
    double norm = (mSoloBand + 1) / static_cast<double>(kNumBands);
    auto pid = static_cast<Steinberg::Vst::ParamID>(EQParams::kSoloBandId);
    mController->beginEdit(pid);
    mController->setParamNormalized(pid, norm);
    mController->performEdit(pid, norm);
    mController->endEdit(pid);
  }
}

//------------------------------------------------------------------------
//  Hit testing
//------------------------------------------------------------------------
int EQMainView::hitTestBand(CPoint &where) {
  if (mTab != Tab::Equalizer)
    return -1;
  for (int i = 0; i < kNumBands; ++i) {
    CCoord x = kBandStartX + i * kBandWidth + kBandWidth / 2.0;
    if (where.x >= x - kBandWidth / 2.0 && where.x <= x + kBandWidth / 2.0 &&
        where.y >= kBandSliderTop - 18 && where.y <= kBandSliderBottom + 12)
      return i;
  }
  return -1;
}

bool EQMainView::hitTestBypass(CPoint &where) {
  return bypassRect().pointInside(where);
}

bool EQMainView::hitTestRange(CPoint &where) {
  return rangeRect().pointInside(where);
}

bool EQMainView::hitTestOut(CPoint &where) {
  if (mTab != Tab::Equalizer)
    return false;
  CRect r = getViewSize();
  CCoord x = r.left + kOutFaderX;
  return where.x >= x - 24 && where.x <= x + 24 &&
         where.y >= kBandSliderTop - 18 && where.y <= kBandSliderBottom + 12;
}

int EQMainView::hitTestTab(CPoint &where) {
  if (tabRect(0).pointInside(where))
    return 0;
  if (tabRect(1).pointInside(where))
    return 1;
  return -1;
}

int EQMainView::cfgRowAt(CPoint &where) {
  if (mTab != Tab::Config)
    return -1;
  int count = std::min((int)mProfiles.size() - mCfgScroll, kCfgVisibleRows);
  for (int v = 0; v < count; ++v) {
    if (cfgRowRect(v).pointInside(where))
      return v;
  }
  return -1;
}

//------------------------------------------------------------------------
float EQMainView::yToGainNorm(CCoord y) {
  float raw = 1.0f - static_cast<float>((y - kBandSliderTop) /
                                        (kBandSliderBottom - kBandSliderTop));
  raw = clamp01(raw);
  if (mRangeIs12dB)
    return 0.25f + raw * 0.5f;
  return raw;
}

CCoord EQMainView::gainNormToY(float norm) {
  float display = norm;
  if (mRangeIs12dB) {
    display = (norm - 0.25f) / 0.5f;
    display = clamp01(display);
  }
  return kBandSliderTop + (1.0 - display) * (kBandSliderBottom - kBandSliderTop);
}

//------------------------------------------------------------------------
//  Config actions
//------------------------------------------------------------------------
void EQMainView::doSaveProfileDialog() {
  mNaming = true;
  mNameBuffer.clear();
}

void EQMainView::doExportDialog() {
  if (auto *fs = CNewFileSelector::create(getFrame(), CNewFileSelector::kSelectSaveFile)) {
    fs->setTitle("Export EQ Profile");
    fs->setDefaultSaveName("MyMix.tmieq");
    fs->setDefaultExtension(CFileExtension("TakeMeIn EQ Profile", "tmieq"));
    fs->run([this](CNewFileSelector *sel) {
      if (sel->getNumSelectedFiles() > 0) {
        if (saveStateToFile(sel->getSelectedFile(0), collectState())) {
          mStatusMsg = "Profile exported";
          mStatusIsError = false;
        } else {
          mStatusMsg = "Export failed";
          mStatusIsError = true;
        }
        mStatusTimer = 2.5f;
      }
    });
    fs->forget();
  }
}

void EQMainView::doImportDialog() {
  if (auto *fs = CNewFileSelector::create(getFrame(), CNewFileSelector::kSelectFile)) {
    fs->setTitle("Import EQ Profile");
    fs->addFileExtension(CFileExtension("TakeMeIn EQ Profile", "tmieq"));
    fs->run([this](CNewFileSelector *sel) {
      if (sel->getNumSelectedFiles() > 0) {
        EQState s;
        if (loadStateFromFile(sel->getSelectedFile(0), s)) {
          applyState(s);
          mStatusMsg = "Profile imported";
          mStatusIsError = false;
        } else {
          mStatusMsg = "Could not read that file";
          mStatusIsError = true;
        }
        mStatusTimer = 2.5f;
      }
    });
    fs->forget();
  }
}

void EQMainView::doResetFlat() {
  EQState s; // defaults are flat
  applyState(s);
  mStatusMsg = "Reset to flat";
  mStatusIsError = false;
  mStatusTimer = 2.0f;
}

//------------------------------------------------------------------------
//  Mouse handling
//------------------------------------------------------------------------
CMouseEventResult EQMainView::onMouseDown(CPoint &where,
                                          const CButtonState &buttons) {
  if (!(buttons & kLButton))
    return kMouseEventNotHandled;

  // Header controls work on both pages
  int tab = hitTestTab(where);
  if (tab >= 0) {
    Tab newTab = tab == 1 ? Tab::Config : Tab::Equalizer;
    if (newTab != mTab) {
      mTab = newTab;
      mNaming = false;
      if (mTab == Tab::Config)
        refreshProfiles();
    }
    invalid();
    return kMouseEventHandled;
  }

  if (hitTestBypass(where)) {
    mBypass = !mBypass;
    if (mController) {
      auto pid = EQParams::kBypassId;
      mController->beginEdit(pid);
      mController->setParamNormalized(pid, mBypass ? 1.0 : 0.0);
      mController->performEdit(pid, mBypass ? 1.0 : 0.0);
      mController->endEdit(pid);
    }
    mAutosaveDirty = true;
    mAutosaveCooldown = 1.0f;
    invalid();
    return kMouseEventHandled;
  }

  if (hitTestRange(where)) {
    mRangeIs12dB = !mRangeIs12dB;
    mRangeClickAnim = 1.0f;
    invalid();
    return kMouseEventHandled;
  }

  // A/B compare: park the current sound in the active slot, recall the other
  if (abRect().pointInside(where)) {
    if (mSlotIsB) {
      mSlotB = collectState();
      mSlotIsB = false;
      applyState(mSlotA);
    } else {
      mSlotA = collectState();
      mSlotIsB = true;
      applyState(mSlotB);
    }
    mStatusMsg = mSlotIsB ? "Slot B active" : "Slot A active";
    mStatusIsError = false;
    mStatusTimer = 1.5f;
    invalid();
    return kMouseEventHandled;
  }

  // Copy current sound into the inactive slot
  if (copyRect().pointInside(where)) {
    if (mSlotIsB)
      mSlotA = collectState();
    else
      mSlotB = collectState();
    mCopyClickAnim = 1.f;
    mStatusMsg = mSlotIsB ? "Copied to A" : "Copied to B";
    mStatusIsError = false;
    mStatusTimer = 1.5f;
    invalid();
    return kMouseEventHandled;
  }

  if (mTab == Tab::Config) {
    // Cancel naming when clicking elsewhere
    if (mNaming && !cfgSaveProfileRect().pointInside(where))
      mNaming = false;

    if (!mNaming && cfgSaveProfileRect().pointInside(where)) {
      mCfgBtnClickAnim[0] = 1.f;
      doSaveProfileDialog();
      invalid();
      return kMouseEventHandled;
    }
    if (cfgExportRect().pointInside(where)) {
      mCfgBtnClickAnim[1] = 1.f;
      doExportDialog();
      return kMouseEventHandled;
    }
    if (cfgImportRect().pointInside(where)) {
      mCfgBtnClickAnim[2] = 1.f;
      doImportDialog();
      return kMouseEventHandled;
    }
    if (cfgResetRect().pointInside(where)) {
      mCfgBtnClickAnim[3] = 1.f;
      doResetFlat();
      return kMouseEventHandled;
    }

    static const double scaleVals[3] = {0.75, 1.0, 1.25};
    for (int i = 0; i < 3; ++i) {
      if (cfgScaleRect(i).pointInside(where)) {
        mUIScale = scaleVals[i];
        if (mController)
          mController->setUIScale(mUIScale);
        invalid();
        return kMouseEventHandled;
      }
    }

    int row = cfgRowAt(where);
    if (row >= 0) {
      int idx = mCfgScroll + row;
      if (idx < (int)mProfiles.size()) {
        CRect rr = cfgRowRect(row);
        CRect loadBtn(rr.right - 130, rr.top + 5, rr.right - 56, rr.bottom - 5);
        CRect delBtn(rr.right - 44, rr.top + 5, rr.right - 12, rr.bottom - 5);
        bool isFactory = mProfiles[idx].factoryIdx >= 0;
        if (isFactory) {
          if (loadBtn.pointInside(where) || rr.pointInside(where)) {
            applyState(factoryPresets()[mProfiles[idx].factoryIdx].state);
            mStatusMsg = "Loaded \"" + mProfiles[idx].name + "\"";
            mStatusIsError = false;
            mStatusTimer = 2.5f;
          }
          invalid();
          return kMouseEventHandled;
        }
        if (delBtn.pointInside(where)) {
          if (deleteProfile(mProfiles[idx].path)) {
            mStatusMsg = "Deleted \"" + mProfiles[idx].name + "\"";
            mStatusIsError = false;
          } else {
            mStatusMsg = "Delete failed";
            mStatusIsError = true;
          }
          mStatusTimer = 2.5f;
          refreshProfiles();
        } else if (loadBtn.pointInside(where) || rr.pointInside(where)) {
          EQState s;
          if (loadStateFromFile(mProfiles[idx].path, s)) {
            applyState(s);
            mStatusMsg = "Loaded \"" + mProfiles[idx].name + "\"";
            mStatusIsError = false;
          } else {
            mStatusMsg = "Could not load profile";
            mStatusIsError = true;
          }
          mStatusTimer = 2.5f;
        }
        invalid();
        return kMouseEventHandled;
      }
    }
    return kMouseEventNotHandled;
  }

  // ---- EQ page ----

  // Clip LED click clears the indicator
  {
    CRect r = getViewSize();
    CCoord lx = r.left + kOutFaderX;
    CRect ledArea(lx - 14, kQLabelY - 4, lx + 14, kQLabelY + 26);
    if (mClipTimer > 0.f && ledArea.pointInside(where)) {
      mClipTimer = 0.f;
      invalid();
      return kMouseEventHandled;
    }
  }

  // Output fader
  if (hitTestOut(where)) {
    if (buttons & kDoubleClick) {
      mOutGainNorm = 0.5f;
      if (mController) {
        auto pid = static_cast<Steinberg::Vst::ParamID>(EQParams::kOutputGainId);
        mController->beginEdit(pid);
        mController->setParamNormalized(pid, 0.5);
        mController->performEdit(pid, 0.5);
        mController->endEdit(pid);
      }
      mAutosaveDirty = true;
      mAutosaveCooldown = 1.0f;
      invalid();
      return kMouseEventHandled;
    }
    mDraggingOut = true;
    mOutFineDrag = (buttons & kShift) != 0;
    if (mOutFineDrag) {
      mFineAnchorNorm = mOutGainNorm;
      mFineAnchorY = where.y;
    }
    float norm = mOutFineDrag
                     ? mOutGainNorm
                     : clamp01(1.0f - static_cast<float>(
                                          (where.y - kBandSliderTop) /
                                          (kBandSliderBottom - kBandSliderTop)));
    mOutGainNorm = norm;
    if (mController) {
      auto pid = static_cast<Steinberg::Vst::ParamID>(EQParams::kOutputGainId);
      mController->beginEdit(pid);
      mController->setParamNormalized(pid, norm);
      mController->performEdit(pid, norm);
    }
    invalid();
    return kMouseEventHandled;
  }

  // Curve-drag editing: grab the response curve in the analyzer.
  // The slider system stays the primary control; this is an extra direct way.
  if (spectrumInnerRect().pointInside(where)) {
    int cb = nearestBandForX(where.x);
    if (buttons & kControl) {
      toggleSolo(cb);
      invalid();
      return kMouseEventHandled;
    }
    if (buttons & kDoubleClick) {
      sendBandGain(cb, 0.5f, true, true);
      mAutosaveDirty = true;
      mAutosaveCooldown = 1.0f;
      invalid();
      return kMouseEventHandled;
    }
    mCurveDragging = true;
    mDragBand = cb;
    mDragging = true;
    sendBandGain(cb, curveYToGainNorm(where.y), true, false);
    invalid();
    return kMouseEventHandled;
  }

  // ---- fader interaction ----
  int band = hitTestBand(where);
  if (band >= 0) {
    // Ctrl+click toggles solo on the band
    if (buttons & kControl) {
      toggleSolo(band);
      invalid();
      return kMouseEventHandled;
    }
    // Double-click resets the band to 0 dB
    if (buttons & kDoubleClick) {
      if (mController) {
        auto pid =
            static_cast<Steinberg::Vst::ParamID>(EQParams::kBandGainFirst + band);
        mController->beginEdit(pid);
        mController->setParamNormalized(pid, 0.5);
        mController->performEdit(pid, 0.5);
        mController->endEdit(pid);
      }
      mGainNorm[band] = 0.5f;
      mAutosaveDirty = true;
      mAutosaveCooldown = 1.0f;
      invalid();
      return kMouseEventHandled;
    }

    mDragBand = band;
    mDragging = true;
    mFineDrag = (buttons & kShift) != 0;
    if (mFineDrag) {
      mFineAnchorNorm = mGainNorm[band];
      mFineAnchorY = where.y;
    }
    float norm = mFineDrag ? mGainNorm[band] : yToGainNorm(where.y);
    mGainNorm[band] = norm;
    if (mController) {
      auto pid =
          static_cast<Steinberg::Vst::ParamID>(EQParams::kBandGainFirst + band);
      mController->beginEdit(pid);
      mController->setParamNormalized(pid, norm);
      mController->performEdit(pid, norm);
    }
    invalid();
    return kMouseEventHandled;
  }
  return kMouseEventNotHandled;
}

CMouseEventResult EQMainView::onMouseMoved(CPoint &where,
                                           const CButtonState &buttons) {
  bool wasBypassHover = mBypassHover;
  mBypassHover = hitTestBypass(where);
  bool wasRangeHover = mRangeHover;
  mRangeHover = hitTestRange(where);
  int wasTabHover = mTabHover;
  mTabHover = hitTestTab(where);
  bool wasABHover = mABHover, wasCopyHover = mCopyHover, wasOutHover = mOutHover;
  mABHover = abRect().pointInside(where);
  mCopyHover = copyRect().pointInside(where);
  mOutHover = !mDraggingOut && hitTestOut(where);

  int wasHoverBand = mHoverBand;
  mHoverBand = (mDragging || mTab != Tab::Equalizer) ? -1 : hitTestBand(where);

  // Config hover state
  int wasCfgRow = mCfgHoverRow, wasCfgZone = mCfgHoverZone;
  int wasCfgBtn = mCfgHoverButton;
  if (mTab == Tab::Config) {
    mCfgHoverRow = cfgRowAt(where);
    mCfgHoverZone = 0;
    if (mCfgHoverRow >= 0) {
      CRect rr = cfgRowRect(mCfgHoverRow);
      CRect loadBtn(rr.right - 130, rr.top + 5, rr.right - 56, rr.bottom - 5);
      CRect delBtn(rr.right - 44, rr.top + 5, rr.right - 12, rr.bottom - 5);
      if (loadBtn.pointInside(where))
        mCfgHoverZone = 1;
      else if (delBtn.pointInside(where))
        mCfgHoverZone = 2;
    }
    mCfgHoverButton = -1;
    if (cfgSaveProfileRect().pointInside(where))
      mCfgHoverButton = 0;
    else if (cfgExportRect().pointInside(where))
      mCfgHoverButton = 1;
    else if (cfgImportRect().pointInside(where))
      mCfgHoverButton = 2;
    else if (cfgResetRect().pointInside(where))
      mCfgHoverButton = 3;
    int wasScaleHover = mScaleHover;
    mScaleHover = -1;
    for (int i = 0; i < 3; ++i)
      if (cfgScaleRect(i).pointInside(where))
        mScaleHover = i;
    if (wasScaleHover != mScaleHover)
      invalid();
  } else {
    mCfgHoverRow = -1;
    mCfgHoverButton = -1;
    mScaleHover = -1;
  }

  if (wasBypassHover != mBypassHover || wasRangeHover != mRangeHover ||
      wasTabHover != mTabHover || wasHoverBand != mHoverBand ||
      wasCfgRow != mCfgHoverRow || wasCfgZone != mCfgHoverZone ||
      wasCfgBtn != mCfgHoverButton || wasABHover != mABHover ||
      wasCopyHover != mCopyHover || wasOutHover != mOutHover)
    invalid();

  // Output fader drag
  if (mDraggingOut) {
    float norm;
    if (mOutFineDrag) {
      float fullRange = static_cast<float>(kBandSliderBottom - kBandSliderTop);
      float dNorm = -static_cast<float>(where.y - mFineAnchorY) / fullRange;
      norm = clamp01(mFineAnchorNorm + dNorm * 0.2f);
    } else {
      norm = clamp01(1.0f - static_cast<float>(
                                (where.y - kBandSliderTop) /
                                (kBandSliderBottom - kBandSliderTop)));
    }
    mOutGainNorm = norm;
    if (mController) {
      auto pid = static_cast<Steinberg::Vst::ParamID>(EQParams::kOutputGainId);
      mController->setParamNormalized(pid, norm);
      mController->performEdit(pid, norm);
    }
    invalid();
    return kMouseEventHandled;
  }

  // Curve drag: vertical movement edits the grabbed band's gain
  if (mCurveDragging && mDragBand >= 0) {
    sendBandGain(mDragBand, curveYToGainNorm(where.y), false, false);
    invalid();
    return kMouseEventHandled;
  }

  if (mDragging && mDragBand >= 0) {
    float norm;
    if (mFineDrag) {
      // 1/5 sensitivity relative to the press anchor
      float fullRange = static_cast<float>(kBandSliderBottom - kBandSliderTop);
      float dNorm = -static_cast<float>(where.y - mFineAnchorY) / fullRange;
      if (mRangeIs12dB)
        dNorm *= 0.5f;
      norm = clamp01(mFineAnchorNorm + dNorm * 0.2f);
    } else {
      norm = yToGainNorm(where.y);
    }
    mGainNorm[mDragBand] = norm;
    if (mController) {
      auto pid = static_cast<Steinberg::Vst::ParamID>(EQParams::kBandGainFirst +
                                                      mDragBand);
      mController->setParamNormalized(pid, norm);
      mController->performEdit(pid, norm);
    }
    invalid();
    return kMouseEventHandled;
  }
  return kMouseEventNotHandled;
}

CMouseEventResult EQMainView::onMouseUp(CPoint &where,
                                        const CButtonState &buttons) {
  if (mDraggingOut) {
    if (mController) {
      auto pid = static_cast<Steinberg::Vst::ParamID>(EQParams::kOutputGainId);
      mController->endEdit(pid);
    }
    mDraggingOut = false;
    mOutFineDrag = false;
    mAutosaveDirty = true;
    mAutosaveCooldown = 1.0f;
    return kMouseEventHandled;
  }
  if (mDragging && mDragBand >= 0) {
    mCurveDragging = false;
    if (mController) {
      auto pid = static_cast<Steinberg::Vst::ParamID>(EQParams::kBandGainFirst +
                                                      mDragBand);
      mController->endEdit(pid);
    }
    mDragging = false;
    mDragBand = -1;
    mFineDrag = false;
    mAutosaveDirty = true;
    mAutosaveCooldown = 1.0f;
    return kMouseEventHandled;
  }
  return kMouseEventNotHandled;
}

CMouseEventResult EQMainView::onMouseExited(CPoint &where,
                                            const CButtonState &buttons) {
  mHoverBand = -1;
  mBypassHover = false;
  mRangeHover = false;
  mTabHover = -1;
  mCfgHoverRow = -1;
  mCfgHoverButton = -1;
  invalid();
  return kMouseEventHandled;
}

//------------------------------------------------------------------------
bool EQMainView::onWheel(const CPoint &where, const CMouseWheelAxis &axis,
                         const float &distance, const CButtonState &buttons) {
  if (mTab == Tab::Config) {
    if ((int)mProfiles.size() > kCfgVisibleRows && cfgListArea().pointInside(where)) {
      mCfgScroll -= (distance > 0 ? 1 : -1);
      mCfgScroll = std::max(0, std::min((int)mProfiles.size() - kCfgVisibleRows,
                                        mCfgScroll));
      invalid();
      return true;
    }
    return false;
  }

  CPoint mw = where;
  int band = hitTestBand(mw);
  if (band < 0 && spectrumInnerRect().pointInside(mw))
    band = nearestBandForX(mw.x); // Q-wheel directly over the curve
  if (band < 0)
    return false;

  float delta = distance * 0.02f;
  mQNorm[band] = clamp01(mQNorm[band] + delta);
  if (mController) {
    auto pid = static_cast<Steinberg::Vst::ParamID>(EQParams::kBandQFirst + band);
    mController->beginEdit(pid);
    mController->setParamNormalized(pid, mQNorm[band]);
    mController->performEdit(pid, mQNorm[band]);
    mController->endEdit(pid);
  }
  mAutosaveDirty = true;
  mAutosaveCooldown = 1.0f;
  invalid();
  return true;
}

//------------------------------------------------------------------------
void EQMainView::onKeyboardEvent(KeyboardEvent &event) {
  if (!mNaming || event.type != EventType::KeyDown)
    return;

  if (event.virt == VirtualKey::Return || event.virt == VirtualKey::Enter) {
    std::string clean = sanitizeName(mNameBuffer);
    if (!clean.empty()) {
      if (saveProfile(clean, collectState())) {
        mStatusMsg = "Saved \"" + clean + "\"";
        mStatusIsError = false;
        refreshProfiles();
      } else {
        mStatusMsg = "Save failed";
        mStatusIsError = true;
      }
      mStatusTimer = 2.5f;
      mNaming = false;
      mNameBuffer.clear();
    }
    event.consumed = true;
    return;
  }
  if (event.virt == VirtualKey::Escape) {
    mNaming = false;
    mNameBuffer.clear();
    event.consumed = true;
    return;
  }
  if (event.virt == VirtualKey::Back) {
    if (!mNameBuffer.empty()) {
      // remove last UTF-8 codepoint
      size_t i = mNameBuffer.size();
      do {
        --i;
      } while (i > 0 && (mNameBuffer[i] & 0xC0) == 0x80);
      mNameBuffer.erase(i);
    }
    event.consumed = true;
    return;
  }
  if (event.character >= 32 && mNameBuffer.size() < 40) {
    // encode char32_t to UTF-8
    char32_t c = event.character;
    if (c < 0x80) {
      mNameBuffer.push_back(static_cast<char>(c));
    } else if (c < 0x800) {
      mNameBuffer.push_back(static_cast<char>(0xC0 | (c >> 6)));
      mNameBuffer.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    } else if (c < 0x10000) {
      mNameBuffer.push_back(static_cast<char>(0xE0 | (c >> 12)));
      mNameBuffer.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
      mNameBuffer.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    }
    event.consumed = true;
  }
}

//------------------------------------------------------------------------
} // namespace TakeMeIn
