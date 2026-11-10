#include "wled.h"

/*
 * MM Effects: ports of the ARTI-FX scripts in https://github.com/MoonModules/MM-Effects (ARTIFX/wled)
 * that upstream WLED does not already have.
 *
 * Most are Pixelblaze patterns (https://electromage.com/patterns) converted to ARTI-FX by Andrew Tuline.
 * The MM-Effects repo has no license, so this usermod is for personal use only - do not submit upstream.
 *
 * Differences from the originals:
 *  - hue is looked up in the selected palette (default: Rainbow, which matches the original look),
 *    so Christmas or synced random palettes work too
 *  - the Speed slider scales time (128 = original speed)
 */

static constexpr float PB_PI  = 3.14159265f;
static constexpr float PB_PI2 = 6.28318531f;

static void mm_static() { SEGMENT.fill(SEGCOLOR(0)); }
#define MM_FALLBACK { mm_static(); return; }

/////////////////////////////////////////
//  Pixelblaze-style helper functions  //
/////////////////////////////////////////

// speed-scaled clock in ms (speed 128 = real time), kept in SEGENV.data so moving the slider never jumps
struct PBClock { uint32_t last; uint64_t t128; };

static bool pbBegin(uint64_t &clk) {
  if (!SEGENV.allocateData(sizeof(PBClock))) return false;
  PBClock *c = reinterpret_cast<PBClock*>(SEGENV.data);
  if (SEGENV.call == 0) { c->last = strip.now; c->t128 = 0; }
  uint32_t dt = strip.now - c->last;
  if (dt > 1000) dt = 1000; // cap jumps after pauses
  c->last = strip.now;
  c->t128 += uint64_t(dt) * (SEGMENT.speed + 1);
  clk = c->t128 >> 7;
  return true;
}

// Pixelblaze time(interval): sawtooth 0..1 that repeats every interval * 65.536 seconds
static inline float pbTime(uint64_t clk, float interval) {
  uint32_t period = uint32_t(interval * 65536.0f);
  if (period == 0) period = 1;
  return float(clk % period) / float(period);
}
static inline float pbFrac(float x)     { return x - floorf(x); }
static inline float pbSin(float x)      { return sin_approx(x); }
static inline float pbCos(float x)      { return cos_approx(x); }
static inline float pbWave(float x)     { return (1.0f + sin_approx(PB_PI2 * pbFrac(x))) * 0.5f; }
static inline float pbTriangle(float x) { x = pbFrac(x); return x < 0.5f ? 2.0f * x : 2.0f - 2.0f * x; }
static inline float pbClamp(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
static inline float pbRandom()          { return hw_random16() / 65536.0f; }

// Pixelblaze hsv(h, s, v): h wraps around the palette, s < 1 fades toward white, v is brightness
static uint32_t pbColor(float h, float s, float v) {
  uint8_t idx = uint8_t(pbFrac(h) * 256.0f);
  uint32_t c  = SEGMENT.color_from_palette(idx, false, true, 0, 255);
  s = pbClamp(s, 0.0f, 1.0f);
  if (s < 1.0f) c = color_blend(WHITE, c, uint8_t(s * 255.0f));
  return color_fade(c, uint8_t(pbClamp(v, 0.0f, 1.0f) * 255.0f), true);
}
static inline void pbHsv(int i, float h, float s, float v) { SEGMENT.setPixelColor(i, pbColor(h, s, v)); }

// common metadata tail for the Pixelblaze ports: speed slider, palette (default Rainbow), 1D
#define PB_META "@!;;!;1;pal=11,sx=128"

//////////////////////////
//  Pixelblaze patterns //
//////////////////////////

static void mode_mm_color_twinkles() {
  uint64_t clk; if (!pbBegin(clk)) MM_FALLBACK;
  const float t1 = pbTime(clk, .15f) * PB_PI2;
  const float t2 = pbTime(clk, .5f) * PB_PI2;
  for (unsigned i = 0; i < SEGLEN; i++) {
    float a = (1.0f + pbSin(i / 3.0f + PB_PI2 * pbSin(i / 2.0f + t1))) * 0.5f;
    a = a * a * a * a;
    if (a <= .1f) a = 0;
    float b = pbSin(i / 3.0f + PB_PI2 * pbSin(i / 2.0f + t2));
    pbHsv(i, b, 1, a / 2);
  }
}
static const char _data_FX_MODE_MM_COLOR_TWINKLES[] PROGMEM = "MM Color Twinkles" PB_META;

static void mode_mm_color_twinkle_bounce() {
  uint64_t clk; if (!pbBegin(clk)) MM_FALLBACK;
  const float t1 = pbTime(clk, .05f) * PB_PI2;
  const float t3 = pbTime(clk, .1f);
  const float s1 = 5.0f * pbSin(t1);
  for (unsigned i = 0; i < SEGLEN; i++) {
    float a = (1.0f + pbSin(i / 2.0f + s1)) * 0.5f;
    float b = t3 + 1.0f + pbSin(i / 2.0f + s1);
    pbHsv(i, b, 1, a * a * a);
  }
}
static const char _data_FX_MODE_MM_COLOR_TWINKLE_BOUNCE[] PROGMEM = "MM Color Twinkle Bounce" PB_META;

static void mode_mm_color_fade_pulse() {
  uint64_t clk; if (!pbBegin(clk)) MM_FALLBACK;
  const float t1 = pbTime(clk, .01f);
  const float t2 = pbTime(clk, .1f) * PB_PI2;
  const float t3 = pbTime(clk, .02f);
  const float n  = SEGLEN;
  for (unsigned i = 0; i < SEGLEN; i++) {
    float h = i / n * 2.0f - t1;
    float s = (1.0f + pbSin(t2 + i / n * PB_PI)) * 0.5f;
    float v = pbTriangle(t3 + i / n * 4.0f);
    pbHsv(i, h, s, v);
  }
}
static const char _data_FX_MODE_MM_COLOR_FADE_PULSE[] PROGMEM = "MM Color Fade Pulse" PB_META;

static void mode_mm_edge_burst() {
  uint64_t clk; if (!pbBegin(clk)) MM_FALLBACK;
  const float t1 = pbTriangle(pbTime(clk, .1f));
  const float n  = SEGLEN;
  for (unsigned i = 0; i < SEGLEN; i++) {
    float edge = pbClamp(pbTriangle(i / n) + t1 * 4.0f - 2.0f, 0, 1);
    pbHsv(i, edge * edge - .2f, 1, pbTriangle(edge));
  }
}
static const char _data_FX_MODE_MM_EDGE_BURST[] PROGMEM = "MM Edge Burst" PB_META;

static void mode_mm_fast_pulse() {
  uint64_t clk; if (!pbBegin(clk)) MM_FALLBACK;
  const float t1 = pbTime(clk, .1f);
  const float w  = 2.0f * pbWave(t1);
  const float n  = SEGLEN;
  for (unsigned i = 0; i < SEGLEN; i++) {
    float v = pbTriangle(w + i / n);
    v = v * v * v * v * v;
    pbHsv(i, t1, v < .9f ? 1 : 0, v); // white tip
  }
}
static const char _data_FX_MODE_MM_FAST_PULSE[] PROGMEM = "MM Fast Pulse" PB_META;

static void mode_mm_firework_rocket_sparks() {
  uint64_t clk; if (!pbBegin(clk)) MM_FALLBACK;
  const float t1 = pbTime(clk, .05f);
  const float n  = SEGLEN;
  for (unsigned i = 0; i < SEGLEN; i++) {
    float v  = pbWave(t1 + i / n);
    float v2 = pbWave(t1 + (i + 10) / n);
    bool  s  = v2 < .95f;                                  // true: dark sky with random sparks
    v = (v > .95f && hw_random16() > 59000) ? v : 0;
    float h = s ? pbRandom() : fmodf(i / 20.0f, .2f);
    pbHsv(i, h, s ? 0 : 1, (s ? 0 : 1) + v);
  }
}
static const char _data_FX_MODE_MM_FIREWORK_ROCKET_SPARKS[] PROGMEM = "MM Firework Rocket Sparks" PB_META;

static void mode_mm_glitch_bands() {
  uint64_t clk; if (!pbBegin(clk)) MM_FALLBACK;
  const float t1 = pbTime(clk, .1f) * PB_PI2;
  const float t2 = pbTime(clk, .1f);
  const float t3 = pbTime(clk, .5f);
  const float t4 = pbTime(clk, .2f) * PB_PI2;
  const float t5 = pbTime(clk, .05f);
  const float t6 = pbTime(clk, .02f);
  const float n  = SEGLEN;
  const float m  = .3f + pbTriangle(t2) * .2f;
  const float h2 = pbTriangle(t3) * 10.0f + 4.0f * pbSin(t4);
  const float hb = pbSin(t1);
  for (unsigned i = 0; i < SEGLEN; i++) {
    float h  = hb + fmodf((i - n / 2) / n * h2, m);
    float s1 = pbTriangle(fmodf(t5 + i / n * 5.0f, 1.0f));
    s1 = s1 * s1;
    float s2 = pbTriangle(fmodf(t6 - (i - n) / n, 1.0f));
    float s  = 1.0f - pbTriangle(s1 * s2);
    float v  = (s1 > s2 ? 1.0f : 0.0f) + .5f;
    pbHsv(i, h, s, v);
  }
}
static const char _data_FX_MODE_MM_GLITCH_BANDS[] PROGMEM = "MM Glitch Bands" PB_META;

static void mode_mm_block_reflections() {
  uint64_t clk; if (!pbBegin(clk)) MM_FALLBACK;
  const float t2 = pbTime(clk, .1f) * PB_PI2;
  const float t1 = pbTime(clk, .1f);
  const float t3 = pbTime(clk, .5f);
  const float t4 = pbTime(clk, .2f) * PB_PI2;
  const float n  = SEGLEN;
  const float m  = .3f + pbTriangle(t1) * .2f;
  const float k  = pbTriangle(t3) * 10.0f + 4.0f * pbSin(t4);
  const float hb = pbSin(t2);
  for (unsigned i = 0; i < SEGLEN; i++) {
    float h = hb + fmodf((i - n / 2) / n * k, m);
    float v = fmodf(fabsf(h) + fabsf(m) + t1, 1.0f);
    pbHsv(i, h, 1, v * v);
  }
}
static const char _data_FX_MODE_MM_BLOCK_REFLECTIONS[] PROGMEM = "MM Block Reflections" PB_META;

static void mode_mm_green_ripple_reflections() {
  uint64_t clk; if (!pbBegin(clk)) MM_FALLBACK;
  const float t1 = pbTime(clk, .03f) * PB_PI2;
  const float t2 = pbTime(clk, .05f) * PB_PI2;
  const float t3 = pbTime(clk, .04f) * PB_PI2;
  const float n  = SEGLEN;
  const float hue = SEGMENT.custom1 / 255.0f;              // original is fixed green (.3)
  for (unsigned i = 0; i < SEGLEN; i++) {
    float a = pbSin(i * PB_PI2 * 5.0f / n + t1);
    a = a * a;
    float b = pbSin(i * PB_PI2 * 3.0f / n - t2);
    float c = pbTriangle((i * 3.0f / n + 1.0f + pbSin(t3)) / 2.0f);
    float v = (a + b + c) / 3.0f;
    pbHsv(i, hue, a, v * v);
  }
}
static const char _data_FX_MODE_MM_GREEN_RIPPLE_REFLECTIONS[] PROGMEM = "MM Ripple Reflections@!,,Hue;;!;1;pal=11,sx=128,c1=77";

static void mode_mm_two_tone_twinkles() { // "halloween color twinkles": two palette bands (purple/orange on Rainbow)
  uint64_t clk; if (!pbBegin(clk)) MM_FALLBACK;
  const float t1 = pbTime(clk, 1.5f) * PB_PI2;
  const float t2 = pbTime(clk, .35f) * PB_PI2;
  for (unsigned i = 0; i < SEGLEN; i++) {
    float h = pbSin(i / 3.0f + PB_PI2 * pbSin(i / 2.0f + t1));
    float v = pbWave(i / 3.0f / PB_PI2 + pbSin(i / 2.0f + t2));
    v = v * v * v * v;
    if (v <= .1f) v = 0;
    h = (h > 0) ? pbTriangle(h) * .1f + .7f : pbTriangle(h) * .05f + .02f;
    pbHsv(i, h, 1, v);
  }
}
static const char _data_FX_MODE_MM_TWO_TONE_TWINKLES[] PROGMEM = "MM Two Tone Twinkles" PB_META;

static void mode_mm_marching_rainbow() {
  uint64_t clk; if (!pbBegin(clk)) MM_FALLBACK;
  const float t1 = pbTime(clk, .1f);
  const float t2 = pbTime(clk, .05f);
  const float n  = SEGLEN;
  for (unsigned i = 0; i < SEGLEN; i++) {
    float w1 = pbWave(t1 + i / n);
    float w2 = pbWave(t2 - i / n * 10.0f + .2f);
    float h  = pbWave(pbWave(w1) - i / n);               // ha == w1
    pbHsv(i, h, 1, w1 - w2);
  }
}
static const char _data_FX_MODE_MM_MARCHING_RAINBOW[] PROGMEM = "MM Marching Rainbow" PB_META;

static void mode_mm_millipede() {
  uint64_t clk; if (!pbBegin(clk)) MM_FALLBACK;
  const float t1 = pbTime(clk, .05f);
  const float t2 = pbTime(clk, .1f);
  const float n  = SEGLEN;
  const float w1 = pbWave(t1);
  for (unsigned i = 0; i < SEGLEN; i++) {
    float ha = i + t2 * n;
    float h  = fmodf(ha / n * 5.0f, .5f) + i / n + w1;
    float v  = pbWave(h + t2);
    pbHsv(i, h, 1, v * v);
  }
}
static const char _data_FX_MODE_MM_MILLIPEDE[] PROGMEM = "MM Millipede" PB_META;

static void mode_mm_opposites() {
  uint64_t clk; if (!pbBegin(clk)) MM_FALLBACK;
  const float t1 = pbTime(clk, .1f);
  const float t2 = pbTime(clk, .2f);
  const float n  = SEGLEN;
  for (unsigned i = 0; i < SEGLEN; i++) {
    float il = i / n;
    float w1 = pbWave(t1 + il);
    float w2 = pbWave(t2 - il);
    float w3 = pbWave(fmodf(il + w1 + w2, 1.0f));
    float h  = fmodf(w3, .3f);
    h = (h > .15f ? h : h + .5f) + t1;
    float v  = (w1 + .1f) * (w2 + .1f) * (w3 + .1f) * 2.0f;
    pbHsv(i, h, 1, v);
  }
}
static const char _data_FX_MODE_MM_OPPOSITES[] PROGMEM = "MM Opposites" PB_META;

static void mode_mm_rainbow_fonts() {
  uint64_t clk; if (!pbBegin(clk)) MM_FALLBACK;
  const float t1 = pbTime(clk, .1f);
  const float hl = max(SEGLEN / 2.0f, 1.0f);
  for (unsigned i = 0; i < SEGLEN; i++) {
    float c = 1.0f - fabsf(i - hl) / hl;
    pbHsv(i, pbWave(pbWave(c) + t1), 1, 1);
  }
}
static const char _data_FX_MODE_MM_RAINBOW_FONTS[] PROGMEM = "MM Rainbow Fountain" PB_META;

static void mode_mm_rainbow_fonts_2() {
  uint64_t clk; if (!pbBegin(clk)) MM_FALLBACK;
  const float t1 = pbTime(clk, .1f);
  const float n  = SEGLEN;
  const float scale  = max(n / 2.0f, 1.0f);
  const float offset = pbSin(pbTime(clk, .2f) * PB_PI2) * n / 10.0f;
  for (unsigned i = 0; i < SEGLEN; i++) {
    float c = 1.0f - fabsf((i + offset) - scale) / scale;
    pbHsv(i, pbWave(pbWave(c) + t1 + offset / n), 1, 1);
  }
}
static const char _data_FX_MODE_MM_RAINBOW_FONTS_2[] PROGMEM = "MM Rainbow Fountain 2" PB_META;

static void mode_mm_rainbow_melt() {
  uint64_t clk; if (!pbBegin(clk)) MM_FALLBACK;
  const float t1 = pbTime(clk, .1f);
  const float t2 = pbTime(clk, .13f);
  const float h1 = max(SEGLEN / 2.0f, 1.0f);
  for (unsigned i = 0; i < SEGLEN; i++) {
    float c1 = 1.0f - fabsf(i - h1) / h1;
    float c3 = pbWave(pbWave(c1) + t1);
    float v  = pbWave(c3 + t1);
    pbHsv(i, c1 + t2, 1, v * v);
  }
}
static const char _data_FX_MODE_MM_RAINBOW_MELT[] PROGMEM = "MM Rainbow Melt" PB_META;

static void mode_mm_slow_color_shift() {
  uint64_t clk; if (!pbBegin(clk)) MM_FALLBACK;
  const float t1 = pbTime(clk, .15f) * PB_PI2;
  const float t2 = pbTime(clk, .1f);
  const float h4 = SEGLEN * 4.0f;
  const float s1 = 5.0f * pbSin(t1);
  for (unsigned i = 0; i < SEGLEN; i++) {
    float a = (1.0f + pbSin(i / 2.0f + s1)) * 0.5f;
    float b = t2 + 1.0f + pbSin(i / 2.0f + s1) / 5.0f + i / h4;
    pbHsv(i, b, 1, a * a * a * a);
  }
}
static const char _data_FX_MODE_MM_SLOW_COLOR_SHIFT[] PROGMEM = "MM Slow Color Shift" PB_META;

static void mode_mm_spin_cycle() {
  uint64_t clk; if (!pbBegin(clk)) MM_FALLBACK;
  const float t1 = pbTime(clk, .1f);
  const float w  = pbWave(t1);
  const float n  = SEGLEN;
  for (unsigned i = 0; i < SEGLEN; i++) {
    float h = i / n * (5.0f + w * 5.0f) + w * 2.0f;
    h = fmodf(h, .5f) + t1;
    float v = pbTriangle(fmodf(i / n * 5.0f + t1 * 10.0f, 1.0f));
    pbHsv(i, h, 1, v * v * v);
  }
}
static const char _data_FX_MODE_MM_SPIN_CYCLE[] PROGMEM = "MM Spin Cycle" PB_META;

///////////////////////////////
//  ARTI-FX native examples  //
///////////////////////////////

// three dots on beatsin paths, with a fading trail
static void mode_mm_beatmania() {
  const unsigned len = SEGLEN;
  if (SEGENV.call == 0) SEGMENT.fill(BLACK);
  SEGMENT.fadeToBlackBy(SEGMENT.intensity / 8 + 1);
  unsigned locn1  = beatsin16_t(SEGMENT.speed / 3 + 1, 0, len - 1);
  unsigned locn2  = beatsin16_t(SEGMENT.speed / 4 + 1, 0, len - 1);
  uint8_t  colr1  = beatsin8_t(SEGMENT.intensity / 6 + 1);
  uint8_t  colr2  = beatsin8_t(SEGMENT.intensity / 7 + 1);
  uint8_t  bri1   = beatsin8_t(SEGMENT.intensity / 6 + 1, 32, 255);
  uint8_t  bri2   = beatsin8_t(SEGMENT.intensity / 7 + 1, 32, 255);
  SEGMENT.setPixelColor((locn1 + locn2) % len, SEGMENT.color_from_palette(uint8_t(colr1 + colr2), false, true, 0, qadd8(bri1, bri2)));
  SEGMENT.setPixelColor(locn1, SEGMENT.color_from_palette(colr2, false, true, 0, bri1));
  SEGMENT.setPixelColor(locn2 % (len > 1 ? len - 1 : 1), SEGMENT.color_from_palette(colr1, false, true, 0, bri2));
}
static const char _data_FX_MODE_MM_BEATMANIA[] PROGMEM = "MM Beatmania@!,Fade/Color rate;;!;1";

// the palette is squeezed and stretched along the strip
static void mode_mm_phase_shift() {
  if (SEGENV.call == 0) { SEGENV.aux0 = 3; SEGENV.aux1 = 1; SEGENV.step = strip.now; }
  const uint32_t interval = (255 - SEGMENT.speed) / 2 + 5;
  if (strip.now - SEGENV.step >= interval) {
    SEGENV.step = strip.now;
    if (SEGENV.aux1) { if (++SEGENV.aux0 > 15) SEGENV.aux1 = 0; }
    else             { if (--SEGENV.aux0 < 3)  SEGENV.aux1 = 1; }
  }
  for (unsigned i = 0; i < SEGLEN; i++)
    SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(uint8_t(i * SEGENV.aux0), false, true, 0));
}
static const char _data_FX_MODE_MM_PHASE_SHIFT[] PROGMEM = "MM Phase Shift@!;;!;1";

// every pixel takes a random palette color
static void mode_mm_color_random() {
  const uint32_t interval = (255 - SEGMENT.speed) * 2;
  if (SEGENV.call != 0 && strip.now - SEGENV.step < interval) return;
  SEGENV.step = strip.now;
  for (unsigned i = 0; i < SEGLEN; i++)
    SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(hw_random8(), false, true, 0));
}
static const char _data_FX_MODE_MM_COLOR_RANDOM[] PROGMEM = "MM Color Random@!;;!;1;sx=200";

// one anti-aliased dot sliding on a slow sine wave
static void mode_mm_subpixel() {
  uint64_t clk; if (!pbBegin(clk)) MM_FALLBACK;
  const float len = SEGLEN;
  const float t   = (pbSin(clk / 1000.0f) + 1.0f) * 0.5f * (len - 1);
  const float sharp = 256 - SEGMENT.intensity;               // higher intensity = wider dot
  const uint32_t col = SEGMENT.color_from_palette(uint8_t(t * 255.0f / max(len - 1, 1.0f)), false, false, 0);
  for (unsigned i = 0; i < SEGLEN; i++) {
    float diff = min(fabsf(t - i), 256.0f / sharp);
    SEGMENT.setPixelColor(i, color_fade(col, uint8_t(pbClamp(256.0f - diff * sharp, 0, 255)), true));
  }
}
static const char _data_FX_MODE_MM_SUBPIXEL[] PROGMEM = "MM Subpixel@!,Dot size;;!;1;ix=200";

// audio: AudioReactive data if present, otherwise WLED's sound simulation
static um_data_t* mmAudio() {
  um_data_t *um_data;
  if (!UsermodManager::getUMData(&um_data, USERMOD_ID_AUDIOREACTIVE)) um_data = simulateSound(SEGMENT.soundSim);
  return um_data;
}

// whole segment fades from color 2 to color 1 with the volume
static void mode_mm_bright_pulse() {
  um_data_t *um_data = mmAudio();
  float volume = *(float*)um_data->u_data[0];
  uint8_t lum  = constrain(int(volume * 256.0f / (256 - SEGMENT.speed)), 0, 255);
  SEGMENT.fill(color_blend(SEGCOLOR(1), SEGCOLOR(0), lum));
}
static const char _data_FX_MODE_MM_BRIGHT_PULSE[] PROGMEM = "MM Bright Pulse@Sensitivity;!,!;;1v;sx=128";

// the 16 frequency bands spread along the segment, brightness = band level
static void mode_mm_fft_bars() {
  um_data_t *um_data = mmAudio();
  uint8_t *fftResult = (uint8_t*)um_data->u_data[2];
  const unsigned len = SEGLEN;
  for (unsigned i = 0; i < len; i++) {
    unsigned band = (i * 16) / len;
    SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(band * 16, false, false, 0, fftResult[band]));
  }
}
static const char _data_FX_MODE_MM_FFT_BARS[] PROGMEM = "MM FFT Bars@;;!;1f;pal=11";

//////////
//  2D  //
//////////

static void mode_mm_2D_pulse() {
  if (!strip.isMatrix || !SEGMENT.is2D()) MM_FALLBACK;
  uint64_t clk; if (!pbBegin(clk)) MM_FALLBACK;
  const float t1 = pbTime(clk, .05f) * PB_PI2;
  const float t2 = pbTime(clk, .09f) * PB_PI2;
  const float z  = 1.0f + pbWave(pbTime(clk, .2f)) * 5.0f;
  const float w = SEG_W, hgt = SEG_H;
  for (unsigned x = 0; x < SEG_W; x++)
    for (unsigned y = 0; y < SEG_H; y++) {
      float h = (1.0f + pbSin(x / w * z + t1) + pbCos(y / hgt * z + t2)) * 0.5f;
      SEGMENT.setPixelColorXY(int(x), int(y), pbColor(h, 1, h * h * h / 2.0f));
    }
}
static const char _data_FX_MODE_MM_2D_PULSE[] PROGMEM = "MM 2D Pulse@!;;!;2;pal=11,sx=128";

// smiley face: color 1 = face, color 2 = eyes, nose and mouth
static void mode_mm_2D_smiley() {
  if (!strip.isMatrix || !SEGMENT.is2D()) MM_FALLBACK;
  const int w = SEG_W, h = SEG_H;
  const uint32_t face = SEGCOLOR(0), features = SEGCOLOR(1);
  SEGMENT.fadeToBlackBy(SEGMENT.intensity / 8 + 1);
  const uint8_t r = uint8_t(min(w, h) * 0.4f);
  SEGMENT.fillCircle(w / 2, h / 2, r, face);
  SEGMENT.drawCircle(w / 2, h / 2, r, features);
  SEGMENT.drawLine(w * .35f, h * .32f, w * .45f, h * .32f, features); // left eye
  SEGMENT.drawLine(w * .55f, h * .32f, w * .65f, h * .32f, features); // right eye
  SEGMENT.drawLine(w * .50f, h * .45f, w * .50f, h * .55f, features); // nose
  SEGMENT.drawLine(w * .35f, h * .68f, w * .65f, h * .68f, features); // mouth
}
static const char _data_FX_MODE_MM_2D_SMILEY[] PROGMEM = "MM Smiley@,Fade;Face,Features;;2;ix=80";

/////////////////////
//  UserMod Class  //
/////////////////////

class MMEffectsUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_mm_color_twinkles,           _data_FX_MODE_MM_COLOR_TWINKLES);
    strip.addEffect(255, &mode_mm_color_twinkle_bounce,     _data_FX_MODE_MM_COLOR_TWINKLE_BOUNCE);
    strip.addEffect(255, &mode_mm_color_fade_pulse,         _data_FX_MODE_MM_COLOR_FADE_PULSE);
    strip.addEffect(255, &mode_mm_edge_burst,               _data_FX_MODE_MM_EDGE_BURST);
    strip.addEffect(255, &mode_mm_fast_pulse,               _data_FX_MODE_MM_FAST_PULSE);
    strip.addEffect(255, &mode_mm_firework_rocket_sparks,   _data_FX_MODE_MM_FIREWORK_ROCKET_SPARKS);
    strip.addEffect(255, &mode_mm_glitch_bands,             _data_FX_MODE_MM_GLITCH_BANDS);
    strip.addEffect(255, &mode_mm_block_reflections,        _data_FX_MODE_MM_BLOCK_REFLECTIONS);
    strip.addEffect(255, &mode_mm_green_ripple_reflections, _data_FX_MODE_MM_GREEN_RIPPLE_REFLECTIONS);
    strip.addEffect(255, &mode_mm_two_tone_twinkles,        _data_FX_MODE_MM_TWO_TONE_TWINKLES);
    strip.addEffect(255, &mode_mm_marching_rainbow,         _data_FX_MODE_MM_MARCHING_RAINBOW);
    strip.addEffect(255, &mode_mm_millipede,                _data_FX_MODE_MM_MILLIPEDE);
    strip.addEffect(255, &mode_mm_opposites,                _data_FX_MODE_MM_OPPOSITES);
    strip.addEffect(255, &mode_mm_rainbow_fonts,            _data_FX_MODE_MM_RAINBOW_FONTS);
    strip.addEffect(255, &mode_mm_rainbow_fonts_2,          _data_FX_MODE_MM_RAINBOW_FONTS_2);
    strip.addEffect(255, &mode_mm_rainbow_melt,             _data_FX_MODE_MM_RAINBOW_MELT);
    strip.addEffect(255, &mode_mm_slow_color_shift,         _data_FX_MODE_MM_SLOW_COLOR_SHIFT);
    strip.addEffect(255, &mode_mm_spin_cycle,               _data_FX_MODE_MM_SPIN_CYCLE);
    strip.addEffect(255, &mode_mm_beatmania,                _data_FX_MODE_MM_BEATMANIA);
    strip.addEffect(255, &mode_mm_phase_shift,              _data_FX_MODE_MM_PHASE_SHIFT);
    strip.addEffect(255, &mode_mm_color_random,             _data_FX_MODE_MM_COLOR_RANDOM);
    strip.addEffect(255, &mode_mm_subpixel,                 _data_FX_MODE_MM_SUBPIXEL);
    strip.addEffect(255, &mode_mm_bright_pulse,             _data_FX_MODE_MM_BRIGHT_PULSE);
    strip.addEffect(255, &mode_mm_fft_bars,                 _data_FX_MODE_MM_FFT_BARS);
    strip.addEffect(255, &mode_mm_2D_pulse,                 _data_FX_MODE_MM_2D_PULSE);
    strip.addEffect(255, &mode_mm_2D_smiley,                _data_FX_MODE_MM_2D_SMILEY);
  }
  void loop() override {}
};

static MMEffectsUsermod mm_effects;
REGISTER_USERMOD(mm_effects);
