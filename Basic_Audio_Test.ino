#define SPKR_PIN 10
#define FREQ_HZ 2000 // 2 kHz
#define ON_MS 300
#define OFF_MS 300
#define RES_BITS 10

void setup() {
  Serial.begin(115200);
  delay(200);
  bool ok = ledcAttach(SPKR_PIN, FREQ_HZ, RES_BITS);
  Serial.println(ok ? "LEDC attached OK" : "LEDC attach FAILED"); // check if properly attached
  ledcWrite(SPKR_PIN, 0);
}

void loop() { // Periodic sound output
  ledcWriteTone(SPKR_PIN, FREQ_HZ); // tone ON at 50% duty
  delay(ON_MS);
  ledcWriteTone(SPKR_PIN, 0); // tone OFF (freq=0 => duty=0)
  delay(OFF_MS);
}
