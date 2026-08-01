#define IN1 26
#define IN2 27
#define IN3 12
#define IN4 13

#define TRIG 18
#define ECHO 19

#define LED_PIN 23

volatile bool g_motorRunning = true;
volatile float g_distanceCm = 999;
volatile bool g_obstacle = false;

SemaphoreHandle_t stateMutex;

const float OBSTACLE_THRESHOLD_CM = 20.0;

void motorRun() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void motorStop() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void motorTask(void *pv) {
  for (;;) {
    bool obstacle, running;
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    obstacle = g_obstacle;
    running = g_motorRunning;
    xSemaphoreGive(stateMutex);

    if (obstacle || !running) {
      motorStop();
    } else {
      motorRun();
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

float readDistanceCm() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long duration = pulseIn(ECHO, HIGH, 50000);
  if (duration == 0) return 999;
  return duration * 0.0343 / 2.0;
}

void sensorTask(void *pv) {
  for (;;) {
    float d = readDistanceCm();
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    g_distanceCm = d;
    xSemaphoreGive(stateMutex);
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void safetyTask(void *pv) {
  for (;;) {
    float d;
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    d = g_distanceCm;
    xSemaphoreGive(stateMutex);

    bool obstacle = (d <= OBSTACLE_THRESHOLD_CM);

    xSemaphoreTake(stateMutex, portMAX_DELAY);
    g_obstacle = obstacle;
    xSemaphoreGive(stateMutex);

    digitalWrite(LED_PIN, obstacle ? HIGH : LOW);
    vTaskDelay(pdMS_TO_TICKS(30));
  }
}

void printStatus() {
  bool obstacle, running;
  float dist;
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  obstacle = g_obstacle;
  running = g_motorRunning;
  dist = g_distanceCm;
  xSemaphoreGive(stateMutex);

  Serial.print("[MOTOR] running=");
  Serial.print(running ? "true" : "false");
  Serial.print("  [SENSOR] distance=");
  Serial.print(dist);
  Serial.print("cm  [SAFETY] obstacle=");
  Serial.println(obstacle ? "true" : "false");
}

void handleCommand(String cmd) {
  cmd.trim();
  cmd.toUpperCase();

  if (cmd == "STOP") {
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    g_motorRunning = false;
    xSemaphoreGive(stateMutex);
    Serial.println("OK: motor stopped");
  } else if (cmd == "START") {
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    g_motorRunning = true;
    xSemaphoreGive(stateMutex);
    Serial.println("OK: motor started");
  } else if (cmd == "STATUS") {
    printStatus();
  } else {
    Serial.println("ERR: unknown command. Use START, STOP, STATUS");
  }
}

void serialTask(void *pv) {
  String buf = "";
  unsigned long lastPrint = 0;

  for (;;) {
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') {
        if (buf.length() > 0) {
          handleCommand(buf);
          buf = "";
        }
      } else {
        buf += c;
      }
    }

    if (millis() - lastPrint > 1000) {
      printStatus();
      lastPrint = millis();
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(LED_PIN, OUTPUT);

  stateMutex = xSemaphoreCreateMutex();

  Serial.println("mcu-io-firmware demo booting...");
  Serial.println("Commands: START  STOP  STATUS");

  xTaskCreate(motorTask,  "MotorTask",  2048, NULL, 2, NULL);
  xTaskCreate(sensorTask, "SensorTask", 2048, NULL, 2, NULL);
  xTaskCreate(safetyTask, "SafetyTask", 2048, NULL, 3, NULL);
  xTaskCreate(serialTask, "SerialTask", 4096, NULL, 1, NULL);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}