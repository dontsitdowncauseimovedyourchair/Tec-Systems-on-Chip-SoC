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
int pinLuz = A0;
int led = 9;
int pinAgua = A1;

//  MOTOR
const int motorFI = 5;
const int motorBI = 6;

int pinBoton = 7;

void setup() {
  Serial.begin(9600);

  pinMode(led, OUTPUT);
  pinMode(motorFI, OUTPUT);
  pinMode(motorBI, OUTPUT);
  pinMode(pinBoton, INPUT);

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("Error OLED");
    while (true);
  }

  display.clearDisplay();
  display.setTextColor(WHITE);

  // DHT
  dht.begin();

  // MPU6050
  if (!mpu.begin(0x68)) {  
    Serial.println("No se encontro MPU6050");
    while (1);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
}

void loop() {

  // LUZ 
  int luz = analogRead(pinLuz);
  int porcentaje = map(luz, 0, 1023, 0, 100);
  porcentaje = constrain(porcentaje, 0, 100);

  bool focoON = porcentaje < 50;
  digitalWrite(led, focoON ? HIGH : LOW);

  // DHT 
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // MPU 
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float magnitud = sqrt(
    a.acceleration.x * a.acceleration.x +
    a.acceleration.y * a.acceleration.y +
    a.acceleration.z * a.acceleration.z
  );

  float movimiento = abs(magnitud - 9.8);

  int velocidad = map(movimiento * 10, 0, 50, 0, 120);
  velocidad = constrain(velocidad, 0, 120);

  if (movimiento < 0.2) {
    velocidad = 0;
  }

  // PWM
  int pwm = map(velocidad, 0, 120, 0, 255);
  pwm = constrain(pwm, 0, 255);

  if (pwm < 60) pwm = 0;

  // BOTÓN 
  bool frenando = digitalRead(pinBoton);

  // AGUA (CALIBRADO)
  int agua = analogRead(pinAgua);

  int nivelAgua = map(agua, 5, 330, 0, 100);
  nivelAgua = constrain(nivelAgua, 0, 100);

  Serial.print("Agua raw: ");
  Serial.print(agua);
  Serial.print(" | %: ");
  Serial.println(nivelAgua);

  // DISPLAY
  display.clearDisplay();
  display.setTextSize(1);

  display.setCursor(0,0);
  display.print("Luz: "); display.print(porcentaje); display.println("%");

  display.setCursor(0,10);
  display.print("Foco: "); display.println(focoON ? "ON" : "OFF");

  display.setCursor(0,20);
  display.print("Temp: "); display.print(t); display.println(" C");

  display.setCursor(0,30);
  display.print("Hum: "); display.print(h); display.println(" %");

  display.setCursor(0,40);
  display.print("Vel: "); display.print(velocidad); display.println(" km/h");

  // AGUA
  display.setCursor(0,50);
  display.print("Agua: ");
  display.print(nivelAgua);
  display.print("% ");

  if (nivelAgua < 10) {
    display.println("Empty");
  } else if (nivelAgua < 60) {
    display.println("LOW");
  } else if (nivelAgua < 85) {
    display.println("Medium");
  } else {
    display.println("HIGH");
  }

  // ABS indicator
  display.setCursor(80,40);
  if (frenando) {
    display.print("[ABS!]");
  }

  display.display();

  // MOTOR Y SEGURIDAD AGUA
  if (nivelAgua > 85) {
    analogWrite(motorFI, 0);
    analogWrite(motorBI, 0);
  }
  else if (frenando) {

    for(int i = 0; i < 5; i++) {
      analogWrite(motorFI, 0);
      analogWrite(motorBI, 0);
      delay(40);

      analogWrite(motorFI, pwm / 3);
      analogWrite(motorBI, 0);
      delay(40);
    }

  } else {

    analogWrite(motorFI, pwm);
    analogWrite(motorBI, 0);
    delay(100);
  }
}
