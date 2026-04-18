#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <driver/i2s_std.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ======================================================
//                        PINS
// ======================================================

// audio amp
# define I2S_BCLK 19
# define I2S_LRCK 18
# define I2S_DOUT 20

// OLED
# define SDA_PIN 6
# define SCL_PIN 7
# define SCREEN_WIDTH 128
# define SCREEN_HEIGHT 64
# define OLED_ADDR 0x3C

// LEDs
# define LED_LEFT_PIN  11
# define LED_RIGHT_PIN 10

// ======================================================
//                    AUDIO SETTINGS
// ======================================================

static const uint32_t SAMPLE_RATE = 44100;
static const float AMPLITUDE = 0.18f;
static const int FRAMES = 256;

static i2s_chan_handle_t tx_chan;
static int16_t audio_buf[FRAMES * 2];   // stereo interleaved L,R
static float phase = 0.0f;

// Different sounds for each driving state
static const float LEFT_TONE_HZ     = 700.0f;
static const float RIGHT_TONE_HZ    = 500.0f;
static const float STRAIGHT_TONE_HZ = 350.0f;

// ======================================================
//                    OLED OBJECT
// ======================================================

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ======================================================
//                     CAR STATES
// ======================================================

enum CarState {
  CAR_STRAIGHT,
  CAR_TURN_LEFT,
  CAR_TURN_RIGHT
};

volatile CarState currentState = CAR_STRAIGHT;
CarState lastDisplayedState = (CarState)(-1);

// ======================================================
//                  LED BLINK CONTROL
// ======================================================

unsigned long lastBlinkTime = 0;
bool blinkState = false;
const unsigned long BLINK_INTERVAL_MS = 200;

// ======================================================
//                  HELPER FUNCTIONS
// ======================================================

const char* stateToText(CarState state) {
  switch (state) {
    case CAR_TURN_LEFT:  return "TURNING\nLEFT";
    case CAR_TURN_RIGHT: return "TURNING\nRIGHT";
    case CAR_STRAIGHT:   return "GOING\nSTRAIGHT";
    default:             return "UNKNOWN";
  }
}

float stateToTone(CarState state) {
  switch (state) {
    case CAR_TURN_LEFT:  return LEFT_TONE_HZ;
    case CAR_TURN_RIGHT: return RIGHT_TONE_HZ;
    case CAR_STRAIGHT:   return STRAIGHT_TONE_HZ;
    default:             return 0.0f;
  }
}

void showStateOnDisplay(CarState state) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(stateToText(state));
  display.display();
}

void setIndicatorLeds(bool leftOn, bool rightOn) {
  digitalWrite(LED_LEFT_PIN,  leftOn  ? HIGH : LOW);
  digitalWrite(LED_RIGHT_PIN, rightOn ? HIGH : LOW);
}

// ======================================================
//                    I2S SETUP
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
//              PUBLIC API FOR CONTROL SYSTEM
// ======================================================

void setCarState(CarState newState) {
  currentState = newState;
}

void setCarTurningLeft() {
  setCarState(CAR_TURN_LEFT);
}

void setCarTurningRight() {
  setCarState(CAR_TURN_RIGHT);
}

void setCarStraight() {
  setCarState(CAR_STRAIGHT);
}

// ======================================================
//                 VISUAL UPDATE LOGIC
// ======================================================

void updateDisplayAndLeds() {
  CarState state = currentState;

  // Update screen only when state changes
  if (state != lastDisplayedState) {
    showStateOnDisplay(state);
    lastDisplayedState = state;

    // reset blink timing when state changes
    blinkState = false;
    lastBlinkTime = millis();
    setIndicatorLeds(false, false);
  }

  unsigned long now = millis();

  switch (state) {
    case CAR_TURN_LEFT:
      if (now - lastBlinkTime >= BLINK_INTERVAL_MS) {
        lastBlinkTime = now;
        blinkState = !blinkState;
        setIndicatorLeds(blinkState, false);
      }
      break;

    case CAR_TURN_RIGHT:
      if (now - lastBlinkTime >= BLINK_INTERVAL_MS) {
        lastBlinkTime = now;
        blinkState = !blinkState;
        setIndicatorLeds(false, blinkState);
      }
      break;

    case CAR_STRAIGHT:
      setIndicatorLeds(false, false);
      break;
  }
}

// ======================================================
//                  AUDIO UPDATE LOGIC
// ======================================================

void updateAudio() {
  float toneHz = stateToTone(currentState);
  float phase_inc = 2.0f * (float)M_PI * toneHz / (float)SAMPLE_RATE;

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

  size_t bytes_written = 0;
  esp_err_t err = i2s_channel_write(
      tx_chan,
      audio_buf,
      sizeof(audio_buf),
      &bytes_written,
      0
  );

  if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
    Serial.printf("i2s_channel_write error: %d\n", (int)err);
  }
}

// ======================================================
//               SUBSYSTEM INIT + UPDATE
// ======================================================

void initAudioVisualSubsystem() {
  Serial.begin(115200);
  delay(200);

  pinMode(LED_LEFT_PIN, OUTPUT);
  pinMode(LED_RIGHT_PIN, OUTPUT);
  setIndicatorLeds(false, false);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED init failed");
    while (true) {
      delay(10);
    }
  }

  setup_i2s();

  showStateOnDisplay(CAR_STRAIGHT);
  currentState = CAR_STRAIGHT;
  lastDisplayedState = CAR_STRAIGHT;
}

void updateAudioVisualSubsystem() {
  updateDisplayAndLeds();
  updateAudio();
}

// ======================================================
//                       SETUP/LOOP
// ======================================================

void setup() {
  initAudioVisualSubsystem();

  // start straight by default
  setCarStraight();
}

void loop() {
  // ====================================================
  // REAL USAGE:
  // The control subsystem should set the state, then
  // this subsystem keeps updating itself.
  // ====================================================
  updateAudioVisualSubsystem();

  // ====================================================
  // DEMO SECTION ONLY
  // Remove when connected to control subsystem
  // ====================================================
  static unsigned long lastChange = 0;
  static int step = 0;

  if (millis() - lastChange > 3000) {
    lastChange = millis();
    step = (step + 1) % 3;

    if (step == 0) {
      setCarTurningLeft();
    } else if (step == 1) {
      setCarStraight();
    } else {
      setCarTurningRight();
    }
  }
}