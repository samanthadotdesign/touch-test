#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>
#include "lofi.h"

/*
  Audio path on the 2.8" ESP32-S3 board (from the LCDwiki schematic):
    ESP32 --I2S--> ES8311 codec --> SC8002B amplifier --> SPEAKER connector
  Codec is configured over the shared I2C bus (same as touch) at 0x18.
*/

// ---------- Audio pins ----------
#define I2S_MCLK   4
#define I2S_BCLK   5
#define I2S_LRCK   7
#define I2S_DOUT   8      // ESP32 -> codec (codec DSDIN)
#define I2S_DIN    6      // codec ASDOUT (mic) -> ESP32
#define AMP_EN     1      // LOW = amplifier on
#define ES8311_ADDR 0x18

#define SR        16000   // sample rate
#define I2S_PORT  I2S_NUM_0

// Codec volume: 0xBF = 0 dB, each step down = -0.5 dB. Lower this if too loud.
#define CODEC_VOLUME 0xB4

static TaskHandle_t audioTask = nullptr;
static volatile bool playing = false;
static volatile float micPeak = 0;

// ---------- ES8311 ----------
static bool esWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static bool es8311Init() {
  Wire.beginTransmission(ES8311_ADDR);
  if (Wire.endTransmission() != 0) return false;

  // Reset
  esWrite(0x00, 0x1F); delay(20);
  esWrite(0x00, 0x00);

  // Base setup (ESP-ADF sequence)
  esWrite(0x01, 0x30);
  esWrite(0x02, 0x00);
  esWrite(0x03, 0x10);
  esWrite(0x16, 0x24);
  esWrite(0x04, 0x10);
  esWrite(0x05, 0x00);
  esWrite(0x0B, 0x00);
  esWrite(0x0C, 0x00);
  esWrite(0x10, 0x1F);
  esWrite(0x11, 0x7F);
  esWrite(0x00, 0x80);           // slave mode, powered on

  // Clocks: MCLK pin = 256 x 16 kHz = 4.096 MHz
  esWrite(0x01, 0x3F);           // all clocks on, MCLK from pin
  esWrite(0x02, 0x00);           // pre_div 1, pre_multi x1
  esWrite(0x05, 0x00);           // adc/dac div 1
  esWrite(0x03, 0x10);           // single speed, adc osr
  esWrite(0x04, 0x10);           // dac osr
  esWrite(0x07, 0x00);           // LRCK divider high
  esWrite(0x08, 0xFF);           // LRCK divider low (256)
  esWrite(0x06, 0x03);           // BCLK divider

  esWrite(0x13, 0x10);
  esWrite(0x1B, 0x0A);
  esWrite(0x1C, 0x6A);

  // Serial data format: I2S, 16-bit. DAC input and ADC (mic) output both on.
  esWrite(0x09, 0x0C);
  esWrite(0x0A, 0x0C);

  // Power up analog + DAC
  esWrite(0x17, 0xBF);
  esWrite(0x0E, 0x02);
  esWrite(0x12, 0x00);
  esWrite(0x14, 0x1A);
  esWrite(0x0D, 0x01);
  esWrite(0x15, 0x40);
  esWrite(0x37, 0x08);
  esWrite(0x45, 0x00);

  esWrite(0x31, 0x00);           // DAC unmute
  esWrite(0x32, CODEC_VOLUME);   // DAC volume
  return true;
}

// ---------- Tiny synth ----------
static float SINE[1024];

static uint32_t rng = 0x12345678;
static inline float noise() {
  rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
  return (int32_t)rng * (1.0f / 2147483648.0f);
}

static inline float sineAt(uint32_t phase) { return SINE[phase >> 22]; }

static inline uint32_t incFor(float hz) { return (uint32_t)(hz * (4294967296.0f / SR)); }
static inline float midiHz(int m) { return 440.0f * powf(2.0f, (m - 69) / 12.0f); }
static inline float decayFor(float seconds) { return expf(-1.0f / (seconds * SR)); }

enum { V_KEYS, V_BASS, V_KICK, V_SNARE, V_HAT };

struct Voice {
  bool on;
  uint8_t type;
  uint32_t phase, inc, minInc;
  float amp, decay, pitchMul, lp;
  int32_t wait;                   // samples before this voice starts (for strums)
};

#define NUM_VOICES 24
static Voice voices[NUM_VOICES];

static Voice *grabVoice() {
  Voice *best = &voices[0];
  for (int i = 0; i < NUM_VOICES; i++) {
    if (!voices[i].on) return &voices[i];
    if (voices[i].amp < best->amp) best = &voices[i];
  }
  return best;                    // steal the quietest
}

static void addVoice(uint8_t type, float hz, float amp, float decaySec, int32_t wait = 0) {
  Voice *v = grabVoice();
  *v = Voice{};
  v->on = true; v->type = type; v->inc = incFor(hz); v->minInc = v->inc;
  v->amp = amp; v->decay = decayFor(decaySec); v->pitchMul = 1.0f; v->wait = wait;
  if (type == V_KICK) {           // pitch drops 110 Hz -> 45 Hz
    v->minInc = incFor(45);
    v->pitchMul = powf(45.0f / 110.0f, 1.0f / (0.07f * SR));
  }
}

// Chord progression: Dm9 - G13 - Cmaj9 - Am9 (rootless voicings), with bass roots
static const int CHORDS[5][4] = {
  {53, 57, 60, 64},   // Dm9   F A C E
  {53, 57, 59, 64},   // G13   F A B E
  {52, 55, 59, 62},   // Cmaj9 E G B D
  {55, 59, 60, 64},   // Am9   G B C E
  {52, 55, 59, 62},   // ending on Cmaj9
};
static const int BASS[5] = {38, 43, 36, 45, 36};

static void strum(int chord, float vel) {
  for (int j = 0; j < 4; j++)
    addVoice(V_KEYS, midiHz(CHORDS[chord][j]), 0.11f * vel, 1.8f, j * (SR * 18 / 1000));
}

static void triggerStep(int bar, int step) {
  if (bar < 4) {
    const float hatVel = (step & 1) ? 0.025f : 0.05f;
    addVoice(V_HAT, 0, hatVel, 0.025f);
    switch (step) {
      case 0: strum(bar, 1.0f); addVoice(V_BASS, midiHz(BASS[bar]), 0.38f, 0.9f);
              addVoice(V_KICK, 110, 0.7f, 0.18f); break;
      case 2: addVoice(V_SNARE, 185, 0.22f, 0.09f); break;
      case 4: addVoice(V_BASS, midiHz(BASS[bar]), 0.28f, 0.7f); break;
      case 5: strum(bar, 0.45f); addVoice(V_KICK, 110, 0.55f, 0.18f); break;
      case 6: addVoice(V_SNARE, 185, 0.22f, 0.09f); break;
    }
  } else if (bar == 4 && step == 0) {
    strum(4, 0.9f);
    addVoice(V_BASS, midiHz(BASS[4]), 0.35f, 1.5f);
    addVoice(V_KICK, 110, 0.6f, 0.18f);
  }
}

static void playTune() {
  const float BPM = 72.0f;
  const float beatSamples = SR * 60.0f / BPM;
  const int32_t total = (int32_t)(4 * 4 * beatSamples) + SR * 3;   // 4 bars + 3 s tail
  const int32_t fadeStart = total - SR * 3 / 2;

  memset(voices, 0, sizeof(voices));
  int step = 0;
  auto stepSample = [&](int k) {
    int bar = k / 8, i = k % 8;
    float beat = (i >> 1) + ((i & 1) ? 0.6f : 0.0f);   // swung 8ths
    return (int32_t)((bar * 4 + beat) * beatSamples);
  };
  int32_t nextStep = 0;

  float masterLp = 0, crackle = 0;
  const float lpA = 1.0f - expf(-2.0f * PI * 3200.0f / SR);
  const float crackleDecay = decayFor(0.0015f);

  const int FRAMES = 256;
  int16_t buf[FRAMES * 2];

  for (int32_t n = 0; n < total; ) {
    for (int f = 0; f < FRAMES; f++, n++) {
      while (n >= nextStep && step < 8 * 5) {
        triggerStep(step / 8, step % 8);
        step++;
        nextStep = stepSample(step);
      }

      float t = (float)n / SR;
      float wow = 1.0f + 0.0025f * sinf(2 * PI * 0.6f * t);       // tape wobble
      float trem = 1.0f + 0.12f * sinf(2 * PI * 4.2f * t);        // e-piano tremolo

      float keys = 0, mix = 0;
      for (int i = 0; i < NUM_VOICES; i++) {
        Voice &v = voices[i];
        if (!v.on) continue;
        if (v.wait > 0) { v.wait--; continue; }
        float s = 0;
        switch (v.type) {
          case V_KEYS:
            s = sineAt(v.phase) + 0.22f * sineAt(v.phase << 1) + 0.05f * sineAt(v.phase * 3);
            v.phase += (uint32_t)(v.inc * wow);
            keys += s * v.amp;
            break;
          case V_BASS:
            s = sineAt(v.phase) + 0.12f * sineAt(v.phase << 1);
            v.phase += (uint32_t)(v.inc * wow);
            mix += s * v.amp;
            break;
          case V_KICK:
            s = sineAt(v.phase);
            v.phase += v.inc;
            if (v.inc > v.minInc) v.inc = (uint32_t)(v.inc * v.pitchMul);
            mix += s * v.amp;
            break;
          case V_SNARE:
            v.lp += 0.35f * (noise() - v.lp);
            s = v.lp * 1.6f + 0.4f * sineAt(v.phase);
            v.phase += v.inc;
            mix += s * v.amp;
            break;
          case V_HAT: {
            float nz = noise();
            v.lp += 0.5f * (nz - v.lp);
            mix += (nz - v.lp) * v.amp;
            break;
          }
        }
        v.amp *= v.decay;
        if (v.amp < 0.0004f) v.on = false;
      }
      mix += keys * trem;

      // vinyl crackle + hiss
      if ((rng & 0xFFFF) < 40) crackle = 0.18f * noise();
      crackle *= crackleDecay;
      mix += crackle + 0.004f * noise();

      // warm low-pass, fade-out, soft clip
      masterLp += lpA * (mix - masterLp);
      float out = masterLp;
      if (n > fadeStart) out *= 1.0f - (float)(n - fadeStart) / (total - fadeStart);
      out = out / (1.0f + fabsf(out));
      int16_t s16 = (int16_t)(out * 30000.0f);
      buf[f * 2] = s16;
      buf[f * 2 + 1] = s16;
    }
    size_t written;
    i2s_write(I2S_PORT, buf, sizeof(buf), &written, portMAX_DELAY);
  }
}

static void audioTaskFn(void *) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    digitalWrite(AMP_EN, LOW);          // amp on
    delay(20);
    playTune();
    i2s_zero_dma_buffer(I2S_PORT);
    delay(100);
    digitalWrite(AMP_EN, HIGH);         // amp off (no hiss when idle)
    playing = false;
  }
}

// Reads the mic continuously and records the loudest sample per block.
static void micTaskFn(void *) {
  const int FRAMES = 256;
  static int16_t buf[FRAMES * 2];
  for (;;) {
    size_t got = 0;
    i2s_read(I2S_PORT, buf, sizeof(buf), &got, portMAX_DELAY);
    int n = got / 4;
    if (n == 0) continue;

    // The codec may put the mic on either slot: measure both, keep the louder.
    int32_t sumL = 0, sumR = 0;
    for (int i = 0; i < n; i++) { sumL += buf[i * 2]; sumR += buf[i * 2 + 1]; }
    int32_t dcL = sumL / n, dcR = sumR / n;          // remove DC offset
    int32_t pk = 0;
    for (int i = 0; i < n; i++) {
      int32_t l = abs(buf[i * 2] - dcL), r = abs(buf[i * 2 + 1] - dcR);
      if (l > pk) pk = l;
      if (r > pk) pk = r;
    }
    float p = pk / 32768.0f;
    if (p > micPeak) micPeak = p;
  }
}

// ---------- Public ----------
bool audioBegin() {
  for (int i = 0; i < 1024; i++) SINE[i] = sinf(2 * PI * i / 1024.0f);

  pinMode(AMP_EN, OUTPUT);
  digitalWrite(AMP_EN, HIGH);           // amp off until we play

  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
  cfg.sample_rate = SR;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 6;
  cfg.dma_buf_len = 256;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;
  cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  cfg.bits_per_chan = I2S_BITS_PER_CHAN_16BIT;

  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_MCLK;
  pins.bck_io_num = I2S_BCLK;
  pins.ws_io_num = I2S_LRCK;
  pins.data_out_num = I2S_DOUT;
  pins.data_in_num = I2S_DIN;

  if (i2s_driver_install(I2S_PORT, &cfg, 0, nullptr) != ESP_OK) {
    Serial.println("I2S install failed");
    return false;
  }
  i2s_set_pin(I2S_PORT, &pins);
  i2s_zero_dma_buffer(I2S_PORT);
  delay(10);                            // let MCLK settle before codec setup

  bool ok = es8311Init();
  Serial.println(ok ? "ES8311 audio codec found at 0x18" : "ES8311 audio codec NOT found");

  xTaskCreatePinnedToCore(audioTaskFn, "lofi", 6144, nullptr, 2, &audioTask, 0);
  xTaskCreatePinnedToCore(micTaskFn, "mic", 4096, nullptr, 3, nullptr, 0);
  return ok;
}

float micLevel() {
  float p = micPeak;
  micPeak = 0;
  return p;
}

void lofiPlay() {
  if (!audioTask || playing) return;
  playing = true;
  xTaskNotifyGive(audioTask);
}

bool lofiIsPlaying() { return playing; }
