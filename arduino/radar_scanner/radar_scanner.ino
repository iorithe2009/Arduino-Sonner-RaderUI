#include <Servo.h>

// Wiring based on README
constexpr int TRIG_PIN = 9;
constexpr int ECHO_PIN = 10;
constexpr int SERVO_PIN = 6;

// SG90 practical limits (avoid hard stop hit)
constexpr int SERVO_MIN_DEG = 10;
constexpr int SERVO_MAX_DEG = 170;
constexpr int STEP_DEG = 2;

// Timing
constexpr unsigned long SERVO_SETTLE_MS = 25;
constexpr unsigned long ECHO_TIMEOUT_US = 25000; // ~4m

Servo scannerServo;
int currentAngle = SERVO_MIN_DEG;
int direction = STEP_DEG;

float readDistanceCm() {
  // Trigger 10us pulse
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long durationUs = pulseIn(ECHO_PIN, HIGH, ECHO_TIMEOUT_US);
  if (durationUs == 0) {
    return -1.0f; // timeout / out-of-range
  }

  // Speed of sound: 343m/s => 29.1us/cm round-trip factor /2
  return durationUs / 58.0f;
}

float readMedianDistanceCm() {
  float samples[3];
  for (int i = 0; i < 3; ++i) {
    samples[i] = readDistanceCm();
    delay(5);
  }

  // If any invalid sample exists, return first valid quickly
  for (int i = 0; i < 3; ++i) {
    if (samples[i] < 0) {
      for (int j = 0; j < 3; ++j) {
        if (samples[j] >= 0) return samples[j];
      }
      return -1.0f;
    }
  }

  // Sort 3 elements (simple swaps) and return median
  if (samples[0] > samples[1]) {
    float t = samples[0]; samples[0] = samples[1]; samples[1] = t;
  }
  if (samples[1] > samples[2]) {
    float t = samples[1]; samples[1] = samples[2]; samples[2] = t;
  }
  if (samples[0] > samples[1]) {
    float t = samples[0]; samples[0] = samples[1]; samples[1] = t;
  }

  return samples[1];
}

void setup() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  scannerServo.attach(SERVO_PIN);
  scannerServo.write(currentAngle);

  Serial.begin(115200);
  delay(300);
}

void loop() {
  scannerServo.write(currentAngle);
  delay(SERVO_SETTLE_MS);

  const float distanceCm = readMedianDistanceCm();
  const unsigned long ts = millis();

  // CSV format: angle,distance,timestamp
  Serial.print(currentAngle);
  Serial.print(',');
  Serial.print(distanceCm, 1);
  Serial.print(',');
  Serial.println(ts);

  currentAngle += direction;

  if (currentAngle >= SERVO_MAX_DEG) {
    currentAngle = SERVO_MAX_DEG;
    direction = -STEP_DEG;
  } else if (currentAngle <= SERVO_MIN_DEG) {
    currentAngle = SERVO_MIN_DEG;
    direction = STEP_DEG;
  }
}
