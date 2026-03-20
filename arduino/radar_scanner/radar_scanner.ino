// ============================================================
// Radar Scanner Phase 1 Final
// - HC-SR04 (TRIG=D9, ECHO=D10) + SG90 (SIG=D6)
// - サーボ 10〜170° 往復スキャン
// - LED マトリクス 12列×8行のファン（扇状）距離プロファイル表示
//   列 = 角度ゾーン (0〜11) / 行数 = 距離に応じた棒グラフ（下から）
//   現在スキャン列は縦ラインで強調 / WARNING 列は点滅
// - シリアルモニタへ 角度・距離・状態 を出力
// ============================================================

#include <Servo.h>
#include "Arduino_LED_Matrix.h"

// ====== ピン定義 ======
const int TRIG_PIN  = 9;
const int ECHO_PIN  = 10;
const int SERVO_PIN = 6;

// ====== 距離閾値 (cm) ======
const float DIST_WARNING = 10.0f;
const float DIST_NEAR    = 30.0f;
const float DIST_MID     = 50.0f;
const float DIST_MAX     = 100.0f;

// ====== スキャン設定 ======
// 12 列 × 15° ≈ 165° をカバー（端打ち防止で 10〜170°）
const int SCAN_MIN   = 10;
const int SCAN_MAX   = 170;
const int SCAN_STEP  = 15;  // 1 ステップの角度増分
const int STEP_DELAY = 200;  // サーボ整定 + 次計測まで待機 (ms)

// ====== LED マトリクス ======
const int COLS = 12;
const int ROWS = 8;

uint8_t  dispBuf[ROWS][COLS];  // 描画バッファ
uint8_t  colFill[COLS];        // 列ごとの点灯行数（0〜8、下から）
uint8_t  colWarn[COLS];        // 列が WARNING 状態か (0/1)

// ====== ローパスフィルタ（EMA）======
const float ALPHA    = 0.3f;  // 平滑化係数（小さいほど滑らか・遅い）
float filteredDist   = 0.0f;

// ====== 警告点滅 ======
bool          warningVisible = true;
unsigned long lastBlink      = 0;
const int     BLINK_MS       = 300;

// ====== スキャン状態 ======
int currentAngle = SCAN_MIN;
int scanDir      = 1;  // +1=右へ進む, -1=左へ進む

Servo           myServo;
ArduinoLEDMatrix matrix;

// ============================================================
// 距離の有効判定
// ============================================================
bool isValidDistance(float dist) {
  return (dist > 0.0f && dist <= DIST_MAX);
}

// ============================================================
// 距離計測 + EMA ローパスフィルタ（タイムアウト 12ms ≈ 約 205 cm、DIST_MAX=100cm に対して余裕を持たせた設定）
// タイムアウト（0 返り）はフィルタに通さず前回値をそのまま返す
// ============================================================
float measureFiltered() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long dur = pulseIn(ECHO_PIN, HIGH, 12000UL);
  if (dur == 0) return filteredDist;  // タイムアウトは前回値を維持

  float raw = dur * 0.034f / 2.0f;
  filteredDist = ALPHA * raw + (1.0f - ALPHA) * filteredDist;
  return filteredDist;
}

// ============================================================
// 距離 → 点灯行数（下から何行光らせるか）
// warn は WARNING 状態のとき true になる
// ============================================================
int distToRows(float dist, bool &warn) {
  warn = false;
  if (dist <= 0.0f || dist > DIST_MAX) return 0;  // OUT
  if (dist < DIST_WARNING) { warn = true; return 8; }  // WARNING: 全行点灯
  if (dist < DIST_NEAR)    return 6;  // NEAR
  if (dist < DIST_MID)     return 3;  // MID
  return 1;                           // FAR
}

// ============================================================
// サーボ角度 → 列インデックス (0〜COLS-1)
// ============================================================
int angleToCol(int angle) {
  return (int)constrain(
    map(angle, SCAN_MIN, SCAN_MAX, 0, COLS - 1),
    0, COLS - 1
  );
}

// ============================================================
// 表示バッファを再構築して LED マトリクスに反映
// activCol : 現在スキャン中の列（縦ラインで強調）, -1 なら強調なし
// ============================================================
void renderDisplay(int activCol) {
  // 点滅タイミング更新
  unsigned long now = millis();
  if (now - lastBlink >= (unsigned long)BLINK_MS) {
    lastBlink    = now;
    warningVisible = !warningVisible;
  }

  memset(dispBuf, 0, sizeof(dispBuf));

  // ファンプロファイル描画（各列に距離バー）
  for (int c = 0; c < COLS; c++) {
    if (colWarn[c] && !warningVisible) continue;  // WARNING 点滅中は消灯

    int fill = colFill[c];
    for (int r = 0; r < fill; r++) {
      dispBuf[ROWS - 1 - r][c] = 1;  // 下から fill 行分点灯
    }
  }

  // 現在スキャン列をスキャンライン（縦ライン）で強調
  if (activCol >= 0 && activCol < COLS) {
    for (int r = 0; r < ROWS; r++) {
      dispBuf[r][activCol] = 1;
    }
  }

  matrix.renderBitmap(dispBuf, ROWS, COLS);
}

// ============================================================
// 距離 → 状態文字列
// ============================================================
const char* distToStatus(float dist) {
  if (!isValidDistance(dist))          return "OUT";
  if (dist < DIST_WARNING)             return "WARNING";
  if (dist < DIST_NEAR)                return "NEAR";
  if (dist < DIST_MID)                 return "MID";
  return "FAR";
}

// ============================================================
// シリアル出力（人間可読）
// ============================================================
void printHumanReadable(int angle, float dist) {
  Serial.print(angle);
  Serial.print("deg  ");
  if (isValidDistance(dist)) {
    Serial.print(dist, 1);
    Serial.print("cm");
  } else {
    Serial.print("---.-cm");
  }
  Serial.print("  ");
  Serial.println(distToStatus(dist));
}

// ============================================================
// シリアル出力（PC連携 CSV）
// ============================================================
void printCsvFrame(int angle, float dist) {
  Serial.print('$');
  Serial.print(angle);
  Serial.print(',');
  if (isValidDistance(dist)) {
    Serial.print(dist, 1);
  } else {
    Serial.print(-1);
  }
  Serial.print(',');
  Serial.println(millis());
}

// ============================================================
// setup
// ============================================================
void setup() {
  Serial.begin(9600);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  myServo.attach(SERVO_PIN);

  matrix.begin();
  memset(dispBuf, 0, sizeof(dispBuf));
  memset(colFill, 0, sizeof(colFill));
  memset(colWarn, 0, sizeof(colWarn));

  myServo.write(SCAN_MIN);
  delay(500);

  Serial.println("=== Radar Scanner Phase1 START ===");
  Serial.println("angle(deg)  distance(cm)  status");
  Serial.println("----------------------------------");
}

// ============================================================
// loop
// ============================================================
void loop() {
  // --- 距離計測 ---
  float dist = measureFiltered();

  // --- 列データ更新 ---
  bool warn = false;
  int rows = distToRows(dist, warn);
  int col  = angleToCol(currentAngle);

  colFill[col] = rows;
  colWarn[col] = warn ? 1 : 0;

  // --- LED 表示 ---
  renderDisplay(col);

  // --- シリアル出力（人間可読） ---
  printHumanReadable(currentAngle, dist);

  // --- CSV出力（PC連携用: $angle,distance_cm,millis） ---
  // distance が OUT/無効のときは -1 を出力
  printCsvFrame(currentAngle, dist);

  delay(STEP_DELAY);

  // --- 次の角度へ ---
  currentAngle += scanDir * SCAN_STEP;

  if (currentAngle >= SCAN_MAX) {
    currentAngle = SCAN_MAX;
    scanDir = -1;
    // 折り返し: スキャンライン非表示でプロファイルのみ表示
    renderDisplay(-1);
    delay(300);
  } else if (currentAngle <= SCAN_MIN) {
    currentAngle = SCAN_MIN;
    scanDir = 1;
    // 折り返し: スキャンライン非表示でプロファイルのみ表示
    renderDisplay(-1);
    delay(300);
    // 折り返し時にバッファをリセット（新スキャン開始）
    memset(colFill, 0, sizeof(colFill));
    memset(colWarn, 0, sizeof(colWarn));
  }

  myServo.write(currentAngle);
}
