#include <Servo.h>
#include "Arduino_LED_Matrix.h"

// --- ピン定義 ---
const int TRIG_PIN = 9;
const int ECHO_PIN = 10;
const int SERVO_PIN = 6;

// --- 距離の設定 ---
const float MIN_DIST = 2.0;
const float MAX_DIST = 200.0;

// --- サーボの設定 ---
const int SERVO_MIN = 0;
const int SERVO_MAX = 180;

Servo myServo;
ArduinoLEDMatrix matrix;  // LEDマトリクスのオブジェクト

// =====================================================
// LEDマトリクス用パターン定義（8行 × 12列）
// 1 = 点灯、0 = 消灯
// =====================================================

// 警告パターン「!!!」（〜10cm）
uint8_t PAT_WARNING[8][12] = {
  {0,1,0,0,0,1,0,0,0,1,0,0},
  {0,1,0,0,0,1,0,0,0,1,0,0},
  {0,1,0,0,0,1,0,0,0,1,0,0},
  {0,1,0,0,0,1,0,0,0,1,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0},
  {0,1,0,0,0,1,0,0,0,1,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0},
};

// バーグラフ 近距離（10〜50cm）左3列
uint8_t PAT_NEAR[8][12] = {
  {1,1,1,0,0,0,0,0,0,0,0,0},
  {1,1,1,0,0,0,0,0,0,0,0,0},
  {1,1,1,0,0,0,0,0,0,0,0,0},
  {1,1,1,0,0,0,0,0,0,0,0,0},
  {1,1,1,0,0,0,0,0,0,0,0,0},
  {1,1,1,0,0,0,0,0,0,0,0,0},
  {1,1,1,0,0,0,0,0,0,0,0,0},
  {1,1,1,0,0,0,0,0,0,0,0,0},
};

// バーグラフ 中距離（50〜100cm）左6列
uint8_t PAT_MID[8][12] = {
  {1,1,1,1,1,1,0,0,0,0,0,0},
  {1,1,1,1,1,1,0,0,0,0,0,0},
  {1,1,1,1,1,1,0,0,0,0,0,0},
  {1,1,1,1,1,1,0,0,0,0,0,0},
  {1,1,1,1,1,1,0,0,0,0,0,0},
  {1,1,1,1,1,1,0,0,0,0,0,0},
  {1,1,1,1,1,1,0,0,0,0,0,0},
  {1,1,1,1,1,1,0,0,0,0,0,0},
};

// バーグラフ 遠距離（100〜200cm）全点灯
uint8_t PAT_FAR[8][12] = {
  {1,1,1,1,1,1,1,1,1,1,1,1},
  {1,1,1,1,1,1,1,1,1,1,1,1},
  {1,1,1,1,1,1,1,1,1,1,1,1},
  {1,1,1,1,1,1,1,1,1,1,1,1},
  {1,1,1,1,1,1,1,1,1,1,1,1},
  {1,1,1,1,1,1,1,1,1,1,1,1},
  {1,1,1,1,1,1,1,1,1,1,1,1},
  {1,1,1,1,1,1,1,1,1,1,1,1},
};

// 範囲外パターン「---」（測定不能）
uint8_t PAT_OUT[8][12] = {
  {0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0},
  {1,1,1,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,1,1,1,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,1,1,1,0},
  {0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0},
};

// =====================================================
// 前回表示したパターンを覚えておく変数
// 同じパターンが続くとき無駄に更新しないため
// =====================================================
int lastPattern = -1;  // -1 = まだ何も表示していない

// パターンを識別する番号
const int PAT_ID_WARNING = 0;
const int PAT_ID_NEAR    = 1;
const int PAT_ID_MID     = 2;
const int PAT_ID_FAR     = 3;
const int PAT_ID_OUT     = 4;

// 警告パターンの点滅用タイマー
bool warningVisible = true;
unsigned long lastBlinkTime = 0;
const int BLINK_INTERVAL = 300;  // 300msごとに点滅

void setup() {
  Serial.begin(9600);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  myServo.attach(SERVO_PIN);
  myServo.write(0);

  // LEDマトリクス初期化
  matrix.begin();

  delay(500);
}

void loop() {
  float distance = measureDistance();
  String status = "OK";
  int patternId;

  // --- 距離に応じて状態を決める ---
  if (distance <= 0 || distance > MAX_DIST) {
    // pulseIn()が0を返したとき = タイムアウト（何も検知できなかった）
    status = "OUT OF RANGE";
    patternId = PAT_ID_OUT;
    distance = MAX_DIST;  // サーボは最大角度にしておく

  } else if (distance < MIN_DIST) {
    status = "TOO CLOSE";
    patternId = PAT_ID_WARNING;
    distance = MIN_DIST;

  } else if (distance < 10.0) {
    status = "WARNING";
    patternId = PAT_ID_WARNING;

  } else if (distance < 50.0) {
    status = "NEAR";
    patternId = PAT_ID_NEAR;

  } else if (distance < 100.0) {
    status = "MID";
    patternId = PAT_ID_MID;

  } else {
    status = "FAR";
    patternId = PAT_ID_FAR;
  }

  // --- サーボ角度を計算して動かす ---
  int angle = map((int)distance, (int)MIN_DIST, (int)MAX_DIST, SERVO_MIN, SERVO_MAX);
  myServo.write(angle);

  // --- LEDマトリクスを更新する ---
  updateLED(patternId);

  // --- シリアルモニタに表示 ---
  Serial.println("====================");
  Serial.print("Distance : ");
  Serial.print(distance, 1);
  Serial.println(" cm");
  Serial.print("Servo    : ");
  Serial.print(angle);
  Serial.println(" deg");
  Serial.print("Status   : ");
  Serial.println(status);
  Serial.println("====================");

  delay(200);
}

// =====================================================
// LEDマトリクス表示を更新する関数
// =====================================================
void updateLED(int patternId) {

  // 警告パターンのときだけ点滅処理をする
  if (patternId == PAT_ID_WARNING) {
    unsigned long now = millis();  // 今の時刻（ミリ秒）を取得

    // BLINK_INTERVAL ミリ秒経過したら表示を切り替える
    if (now - lastBlinkTime >= BLINK_INTERVAL) {
      lastBlinkTime = now;
      warningVisible = !warningVisible;  // true/false を交互に切り替え

      if (warningVisible) {
        matrix.renderBitmap(PAT_WARNING, 8, 12);
      } else {
        matrix.clear();  // 消灯
      }
    }
    lastPattern = PAT_ID_WARNING;
    return;  // ここで終了（以降の処理は不要）
  }

  // 同じパターンが続いているなら更新しない（無駄な処理を省く）
  if (patternId == lastPattern) return;

  // パターンを切り替えて表示
  switch (patternId) {
    case PAT_ID_NEAR: matrix.renderBitmap(PAT_NEAR, 8, 12); break;
    case PAT_ID_MID:  matrix.renderBitmap(PAT_MID,  8, 12); break;
    case PAT_ID_FAR:  matrix.renderBitmap(PAT_FAR,  8, 12); break;
    case PAT_ID_OUT:  matrix.renderBitmap(PAT_OUT,  8, 12); break;
  }

  lastPattern = patternId;
}

// =====================================================
// 距離を測定する関数（前回と同じ）
// =====================================================
float measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  // タイムアウトのとき pulseIn は 0 を返す
  if (duration == 0) return 0;

  return duration * 0.034 / 2.0;
}
