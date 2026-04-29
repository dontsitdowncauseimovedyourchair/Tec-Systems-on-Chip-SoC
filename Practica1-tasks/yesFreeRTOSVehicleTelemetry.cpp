#include <Arduino_FreeRTOS.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <math.h>

//  OLED 
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

//  DHT 
#define DHTPIN 11
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

//  MPU 
Adafruit_MPU6050 mpu;

//  Pines 
const int pinLuz = A0;
const int led = 9;
const int pinAgua = A1;
const int motorFI = 5;
const int motorBI = 6;
const int pinBoton = 2; // PIN 2 FOR HARDWARE INTERRUPT (INT0)

//  Globals for Task Sharing 
volatile int g_porcentajeLuz = 0;
volatile bool g_focoON = false;
volatile float g_temp = 0.0;
volatile float g_hum = 0.0;
volatile int g_velocidad = 0;
volatile int g_pwm = 0;
volatile int g_nivelAgua = 0;
volatile bool g_frenando = false;

//  FreeRTOS Task Handles 
TaskHandle_t TaskMotor_Handle;
TaskHandle_t TaskMPU_Handle;

//  Prototypes 
void TaskLuz(void *pvParameters);
void TaskDHT(void *pvParameters);
void TaskMPU(void *pvParameters);
void TaskAgua(void *pvParameters);
void TaskMotor(void *pvParameters);
void TaskDisplay(void *pvParameters);
void TaskSafety(void *pvParameters);
void isrBoton(); // Interrupt Service Routine

void setup() {
  Serial.begin(9600);

  pinMode(led, OUTPUT);
  pinMode(motorFI, OUTPUT);
  pinMode(motorBI, OUTPUT);
  pinMode(pinBoton, INPUT); 

  // Attach External Interrupt on Pin 2
  attachInterrupt(digitalPinToInterrupt(pinBoton), isrBoton, CHANGE);

  // Initialize Sensors
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("Error OLED");
    while (true);
  }
  display.clearDisplay();
  display.setTextColor(WHITE);
  
  dht.begin();

  if (!mpu.begin(0x68)) {
    Serial.println("No se encontro MPU6050");
    while (true);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // Create Tasks 
  xTaskCreate(TaskLuz, "Luz", 128, NULL, 1, NULL);
  xTaskCreate(TaskDHT, "DHT", 128, NULL, 1, NULL);
  xTaskCreate(TaskAgua, "Agua", 128, NULL, 1, NULL);
  
  // Store handle for MPU so that the Display can suspend it
  xTaskCreate(TaskMPU, "MPU", 192, NULL, 1, &TaskMPU_Handle);
  xTaskCreate(TaskDisplay, "OLED", 256, NULL, 2, NULL); 
  
  // Store handle for Motor so that Safety can suspend it
  xTaskCreate(TaskMotor, "Motor", 128, NULL, 2, &TaskMotor_Handle); 
  xTaskCreate(TaskSafety, "Safety", 128, NULL, 3, NULL); 
}

void loop() {
  // FreeRTOS loop is idle task
}

//  INTERRUPT ROUTINE 
void isrBoton() {
  // Instantly updates the braking variable when the button is pressed/released
  g_frenando = digitalRead(pinBoton);
}

//  TASK DEFINITIONS 

void TaskLuz(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    int luz = analogRead(pinLuz);
    g_porcentajeLuz = map(luz, 0, 1023, 0, 100);
    g_porcentajeLuz = constrain(g_porcentajeLuz, 0, 100);
    g_focoON = g_porcentajeLuz < 50;
    
    digitalWrite(led, g_focoON ? HIGH : LOW);
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void TaskDHT(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    g_hum = dht.readHumidity();
    g_temp = dht.readTemperature();
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void TaskMPU(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    float magnitud = sqrt(
      a.acceleration.x * a.acceleration.x +
      a.acceleration.y * a.acceleration.y +
      a.acceleration.z * a.acceleration.z
    );

    float movimiento = abs(magnitud - 9.8);
    int vel = map(movimiento * 10, 0, 50, 0, 120);
    vel = constrain(vel, 0, 120);

    if (movimiento < 0.2) vel = 0;
    g_velocidad = vel;

    int pwm = map(vel, 0, 120, 0, 255);
    pwm = constrain(pwm, 0, 255);
    if (pwm < 60) pwm = 0;
    g_pwm = pwm;

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void TaskAgua(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    int agua = analogRead(pinAgua);
    g_nivelAgua = map(agua, 5, 330, 0, 100);
    g_nivelAgua = constrain(g_nivelAgua, 0, 100);
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void TaskDisplay(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    // prevent I2C bus collision
    vTaskSuspend(TaskMPU_Handle);

    display.clearDisplay();
    display.setTextSize(1);
    
    display.setCursor(0,0);
    display.print("Luz: "); display.print(g_porcentajeLuz); display.println("%");

    display.setCursor(0,10);
    display.print("Foco: "); display.println(g_focoON ? "ON" : "OFF");

    display.setCursor(0,20);
    display.print("Temp: "); display.print(g_temp); display.println(" C");

    display.setCursor(0,30);
    display.print("Hum: "); display.print(g_hum); display.println(" %");

    display.setCursor(0,40);
    display.print("Vel: "); display.print(g_velocidad); display.println(" km/h");

    display.setCursor(0,50);
    display.print("Agua: ");
    display.print(g_nivelAgua);
    display.print("% ");

    if (g_nivelAgua < 10) display.println("Empty");
    else if (g_nivelAgua < 60) display.println("LOW");
    else if (g_nivelAgua < 85) display.println("Medium");
    else display.println("HIGH");

    if (g_frenando) {
      display.setCursor(80,40);
      display.print("[ABS!]");
    }

    display.display();

    // Resume the MPU task now that we know I2C is free
    vTaskResume(TaskMPU_Handle);

    vTaskDelay(pdMS_TO_TICKS(200)); 
  }
}

void TaskMotor(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    if (g_frenando) {
      // ABS sequence triggered by hardware interrupt
      for(int i = 0; i < 5; i++) {
        analogWrite(motorFI, 0);
        analogWrite(motorBI, 0);
        vTaskDelay(pdMS_TO_TICKS(40));

        analogWrite(motorFI, g_pwm / 3);
        analogWrite(motorBI, 0);
        vTaskDelay(pdMS_TO_TICKS(40));
      }
    } else {
      // Normal operation
      analogWrite(motorFI, g_pwm);
      analogWrite(motorBI, 0);
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}

void TaskSafety(void *pvParameters) {
  (void) pvParameters;
  bool isSuspended = false;

  for (;;) {
    // Check safety conditions
    bool danger = (g_temp > 40.0) || (g_nivelAgua > 85);

    if (danger && !isSuspended) {
      // Suspend motor task fully
      vTaskSuspend(TaskMotor_Handle);
      analogWrite(motorFI, 0);
      analogWrite(motorBI, 0);
      isSuspended = true;
      Serial.println("ALARM: Motor SUSPENDED.");
    } 
    else if (!danger && isSuspended) {
      // Resume motor task
      vTaskResume(TaskMotor_Handle);
      isSuspended = false;
      Serial.println("SAFE: Motor RESUMED.");
    }
    
    vTaskDelay(pdMS_TO_TICKS(100)); 
  }
}
