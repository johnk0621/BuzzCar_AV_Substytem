#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SDA_PIN 6
#define SCL_PIN 7

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

#define LED_LEFT_PIN  11
#define LED_RIGHT_PIN 10

#define BLINK_ON_MS   150
#define BLINK_OFF_MS  150
#define DELAY_BETWEEN_LEFT_RIGHT_MS 2000
#define DELAY_AFTER_RIGHT_MS        2000

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void showMessage(const char *msg) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(msg);
  display.display();
}

void blinkLedNTimes(int pin, int n) {
  for (int i = 0; i < n; i++) {
    digitalWrite(pin, HIGH);
    delay(BLINK_ON_MS);
    digitalWrite(pin, LOW);
    delay(BLINK_OFF_MS);
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(LED_LEFT_PIN, OUTPUT);
  pinMode(LED_RIGHT_PIN, OUTPUT);
  digitalWrite(LED_LEFT_PIN, LOW);
  digitalWrite(LED_RIGHT_PIN, LOW);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (true) delay(10);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("OLED OK!");
  display.println("ESP32-C6");
  display.display();

  delay(500);
}

void loop() {
  showMessage("turning\nleft");
  blinkLedNTimes(LED_LEFT_PIN, 5);

  delay(DELAY_BETWEEN_LEFT_RIGHT_MS);

  showMessage("turning\nright");
  blinkLedNTimes(LED_RIGHT_PIN, 5);

  delay(DELAY_AFTER_RIGHT_MS);
}
