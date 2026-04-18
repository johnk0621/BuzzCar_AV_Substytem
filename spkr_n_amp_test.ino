#include <Arduino.h>
#include <driver/i2s_std.h>
#include <math.h>

// ======================================================
//                  I2S PIN MAPPING
// ======================================================
static const int I2S_BCLK = 19;   // MAX98357A BCLK
static const int I2S_LRCK = 18;   // MAX98357A LRC / WS
static const int I2S_DOUT = 20;   // MAX98357A DIN

// ======================================================
//                  AUDIO SETTINGS
// ======================================================
static const uint32_t SAMPLE_RATE = 44100;
static const float AMPLITUDE = 0.80f;   // change from values < 1.00 to change how loud speaker is
static const int FRAMES = 256;

static i2s_chan_handle_t tx_chan;
static int16_t audio_buf[FRAMES * 2];   // stereo interleaved L, R
static float phase = 0.0f;

// ======================================================
//                  I2S SETUP
// ======================================================
void setup_i2s() {
  i2s_chan_config_t chan_cfg =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, nullptr));

  i2s_std_config_t std_cfg = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
    .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(
                  I2S_DATA_BIT_WIDTH_16BIT,
                  I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = (gpio_num_t)I2S_BCLK,
      .ws   = (gpio_num_t)I2S_LRCK,
      .dout = (gpio_num_t)I2S_DOUT,
      .din  = I2S_GPIO_UNUSED
    }
  };

  ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));
  ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));
}

// ======================================================
//            WRITE ONE AUDIO BUFFER
// ======================================================
// toneHz > 0  -> play tone
// toneHz <= 0 -> silence
void writeToneBuffer(float toneHz) {
  if (toneHz <= 0.0f) {
    for (int i = 0; i < FRAMES * 2; i++) {
      audio_buf[i] = 0;
    }
  } else {
    const float phase_inc =
        2.0f * (float)M_PI * toneHz / (float)SAMPLE_RATE;

    for (int i = 0; i < FRAMES; i++) {
      float s = sinf(phase) * AMPLITUDE;
      int16_t sample = (int16_t)(s * 32767.0f);

      audio_buf[i * 2 + 0] = sample;
      audio_buf[i * 2 + 1] = sample;

      phase += phase_inc;
      if (phase >= 2.0f * (float)M_PI) {
        phase -= 2.0f * (float)M_PI;
      }
    }
  }

  size_t bytes_written = 0;
  esp_err_t err = i2s_channel_write(
      tx_chan,
      audio_buf,
      sizeof(audio_buf),
      &bytes_written,
      portMAX_DELAY
  );

  if (err != ESP_OK) {
    Serial.printf("i2s_channel_write error: %d\n", (int)err);
  }
}

// ======================================================
//          PLAY TONE OR SILENCE FOR A DURATION
// ======================================================
void playForMs(float toneHz, unsigned long durationMs) {
  unsigned long start = millis();
  while (millis() - start < durationMs) {
    writeToneBuffer(toneHz);
  }
}

// ======================================================
//                PLAY A BEEP PATTERN
// ======================================================
void beepPattern(float toneHz, int count, unsigned long onMs, unsigned long offMs) {
  for (int i = 0; i < count; i++) {
    playForMs(toneHz, onMs);
    playForMs(0.0f, offMs);
  }
}

// ======================================================
//                       SETUP
// ======================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("Starting I2S speaker output test...");
  Serial.printf("BCLK=%d  LRCK/WS=%d  DOUT=%d\n", I2S_BCLK, I2S_LRCK, I2S_DOUT);

  setup_i2s();

  Serial.println("I2S initialized.");
  Serial.println("Expected pattern:");
  Serial.println("  3 beeps @ 440 Hz");
  Serial.println("  3 beeps @ 660 Hz");
  Serial.println("  3 beeps @ 880 Hz");
  Serial.println("  then 2 seconds silence");
}

// ======================================================
//                        LOOP
// ======================================================
void loop() {
  Serial.println("Testing 440 Hz...");
  beepPattern(440.0f, 3, 250, 150);

  Serial.println("Testing 660 Hz...");
  beepPattern(660.0f, 3, 250, 150);

  Serial.println("Testing 880 Hz...");
  beepPattern(880.0f, 3, 250, 150);

  Serial.println("Silence...");
  playForMs(0.0f, 2000);
}