// ============================================================================
// AUTONOMOUS EMERGENCY BRAKING (AEB) 2WD ROBOT CAR FIRMWARE
// Hardware: Arduino Uno R3 + L298N + HC-SR04 + Direct Green/Red Status LEDs
// Features: 500 Hz Phase-Interleaved PWM (50% Battery Load Reduction),
//           Left/Right Gearbox Friction Balancing (130/100),
//           Proportional Deceleration (25 cm) & Passive H-Bridge Brake (10 cm)
// ============================================================================

const int ENA_PIN = 5;   // Left Motor PWM
const int ENB_PIN = 6;   // Right Motor PWM
const int IN1_PIN = 8;   // Left Motor Forward (HIGH)
const int IN2_PIN = 7;   // Left Motor Reverse (LOW)
const int IN3_PIN = 9;   // Right Motor Forward (HIGH)
const int IN4_PIN = 10;  // Right Motor Reverse (LOW)

// --- DIRECT-MOUNTED STATUS LED PINS (NO BREADBOARD REQUIRED) ---
const int GREEN_LED_PIN = 13; // Green LED Anode (+) [Cathode (-) on adjacent GND]
const int RED_LED_PIN   = 3;  // Red LED Anode (+)   [PWM current-limited]
const int RED_GND_PIN   = 2;  // Red LED Cathode (-) [Configured as digital GND]

// --- DISTANCE THRESHOLDS (CM) ---
const float SLOW_DIST_CM = 25.0; // Start proportional deceleration at <= 25.0 cm
const float STOP_DIST_CM = 10.0; // Full emergency stop at <= 10.0 cm

// --- CALIBRATED WHEEL SPEED LIMITS ---
const int MAX_SPEED_TRAI = 130;  // Left wheel cruising speed
const int MAX_SPEED_PHAI = 100;  // Right wheel cruising speed
const int MIN_SPEED_TRAI = 90;   // Left wheel minimum crawling speed
const int MIN_SPEED_PHAI = 70;   // Right wheel minimum crawling speed

float currentDist = 100.0;
float speedRatio = 1.0f;
bool isStopped = false;

int activeTrig = -1;
int activeEcho = -1;

void setup() {
  Serial.begin(9600);

  pinMode(ENA_PIN, OUTPUT);
  pinMode(ENB_PIN, OUTPUT);
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  pinMode(IN3_PIN, OUTPUT);
  pinMode(IN4_PIN, OUTPUT);

  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(RED_GND_PIN, OUTPUT);
  digitalWrite(RED_GND_PIN, LOW);

  stopMotorsCompletely();
}

void setStatusLEDs(bool greenOn, bool redOn) {
  digitalWrite(GREEN_LED_PIN, greenOn ? HIGH : LOW);
  analogWrite(RED_LED_PIN, redOn ? 90 : 0);
}

void loop() {
  // 1. Read ultrasonic distance
  float rawDist = readSmartUltrasonicCm();

  if (rawDist < currentDist) {
    currentDist = rawDist;
  } else {
    currentDist = 0.5f * currentDist + 0.5f * rawDist;
  }

  Serial.print("Khoang cach: ");
  Serial.print(currentDist, 1);
  Serial.print(" cm | ");

  // 2. ZONE 1 (<= 10 CM): FULL STOP -> GREEN LED OFF, RED LED ON
  if (currentDist <= STOP_DIST_CM) {
    speedRatio = 0.0f;
    setStatusLEDs(false, true);
    Serial.println("[DUNG HAN - LED DO]");

    if (!isStopped) {
      passiveShortBrake();
      isStopped = true;
    } else {
      stopMotorsCompletely();
    }
    delay(15);
  }
  // 3. ZONE 2 (25 CM -> 10 CM): PROPORTIONAL DECELERATION -> GREEN LED OFF, RED LED ON
  else if (currentDist < SLOW_DIST_CM) {
    isStopped = false;
    setStatusLEDs(false, true);

    float ratio = (currentDist - STOP_DIST_CM) / (SLOW_DIST_CM - STOP_DIST_CM);
    speedRatio = constrain(ratio, 0.0f, 1.0f);

    int pwmTrai = MIN_SPEED_TRAI + (int)(speedRatio * (MAX_SPEED_TRAI - MIN_SPEED_TRAI));
    int pwmPhai = MIN_SPEED_PHAI + (int)(speedRatio * (MAX_SPEED_PHAI - MIN_SPEED_PHAI));

    Serial.print("Trai: ");
    Serial.print(pwmTrai);
    Serial.print(" - Phai: ");
    Serial.print(pwmPhai);
    Serial.println(" [GIAM TOC - LED DO]");

    driveBalancedInterleavedPWM(pwmTrai, pwmPhai, 26);
  }
  // 4. ZONE 3 (> 25 CM): NORMAL CRUISING -> GREEN LED ON, RED LED OFF
  else {
    isStopped = false;
    setStatusLEDs(true, false);

    if (speedRatio < 1.0f) {
      speedRatio += 0.15f;
      if (speedRatio > 1.0f) speedRatio = 1.0f;
    }

    int pwmTrai = MIN_SPEED_TRAI + (int)(speedRatio * (MAX_SPEED_TRAI - MIN_SPEED_TRAI));
    int pwmPhai = MIN_SPEED_PHAI + (int)(speedRatio * (MAX_SPEED_PHAI - MIN_SPEED_PHAI));

    Serial.print("Trai: ");
    Serial.print(pwmTrai);
    Serial.print(" - Phai: ");
    Serial.print(pwmPhai);
    Serial.println(" [CHAY DEU - LED XANH]");

    driveBalancedInterleavedPWM(pwmTrai, pwmPhai, 26);
  }
}

// ==================== 500 HZ PHASE-INTERLEAVED PWM ====================
void driveBalancedInterleavedPWM(int pwmTrai, int pwmPhai, int durationMs) {
  if (pwmTrai <= 0 && pwmPhai <= 0) {
    stopMotorsCompletely();
    delay(durationMs);
    return;
  }

  digitalWrite(IN1_PIN, HIGH);
  digitalWrite(IN2_PIN, LOW);
  digitalWrite(IN3_PIN, HIGH);
  digitalWrite(IN4_PIN, LOW);

  int onUsTrai = map(constrain(pwmTrai, 0, 120), 0, 120, 0, 950);
  int onUsPhai = map(constrain(pwmPhai, 0, 120), 0, 120, 0, 950);
  int restUs = 2000 - (onUsTrai + onUsPhai);
  int cycles = (durationMs * 1000L) / 2000L;

  for (int i = 0; i < cycles; i++) {
    // Phase 1: Left Wheel Active (Right Wheel Resting)
    digitalWrite(ENA_PIN, HIGH);
    digitalWrite(ENB_PIN, LOW);
    if (onUsTrai > 0) delayMicroseconds(onUsTrai);

    // Phase 2: Right Wheel Active (Left Wheel Resting)
    digitalWrite(ENA_PIN, LOW);
    digitalWrite(ENB_PIN, HIGH);
    if (onUsPhai > 0) delayMicroseconds(onUsPhai);

    // Phase 3: Remainder of 2000us cycle
    digitalWrite(ENA_PIN, LOW);
    digitalWrite(ENB_PIN, LOW);
    if (restUs > 0) {
      delayMicroseconds(restUs);
    }
  }
}

void passiveShortBrake() {
  digitalWrite(IN1_PIN, LOW);
  digitalWrite(IN2_PIN, LOW);
  digitalWrite(IN3_PIN, LOW);
  digitalWrite(IN4_PIN, LOW);
  digitalWrite(ENA_PIN, HIGH);
  digitalWrite(ENB_PIN, HIGH);
  delay(45);
  stopMotorsCompletely();
}

void stopMotorsCompletely() {
  digitalWrite(ENA_PIN, LOW);
  digitalWrite(ENB_PIN, LOW);
  digitalWrite(IN1_PIN, LOW);
  digitalWrite(IN2_PIN, LOW);
  digitalWrite(IN3_PIN, LOW);
  digitalWrite(IN4_PIN, LOW);
}

// ==================== ULTRASONIC RANGING (PINS 11 & 12) ====================
float tryPing(int trigPin, int echoPin) {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  unsigned long duration = pulseIn(echoPin, HIGH, 10000UL);
  if (duration == 0) return 150.0;
  float cm = (duration * 0.0348f) / 2.0f;
  if (cm < 1.5f || cm > 145.0f) return 150.0;
  return cm;
}

float readSmartUltrasonicCm() {
  if (activeTrig != -1) {
    float d = tryPing(activeTrig, activeEcho);
    if (d < 145.0) return d;
  }

  const int pairs[2][2] = {{11, 12}, {12, 11}};
  for (int i = 0; i < 2; i++) {
    float d = tryPing(pairs[i][0], pairs[i][1]);
    if (d < 145.0) {
      activeTrig = pairs[i][0];
      activeEcho = pairs[i][1];
      return d;
    }
  }
  return 150.0;
}
