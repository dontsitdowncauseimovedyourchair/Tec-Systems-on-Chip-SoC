#include <Wire.h>
#include <DHT.h> // Include the Adafruit DHT library

#define SLAVE_ADDR 0x08

// --- Pin Definitions ---
#define BUZZER_PIN 25
#define MOTOR_PIN 27  
#define BUTTON_PIN 14 
#define DHTPIN 4     // FIXED: Changed to pin 4
#define TRIG_PIN 18 
#define ECHO_PIN 19 

#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE); // Initialize the DHT sensor

// --- Global Variables ---
volatile bool playMartinillo = false;
volatile bool triggerErrorBuzzer = false;
volatile bool triggerMotorSequence = false;

int martinilloNotes[] = {262, 294, 330, 262, 262, 294, 330, 262, 330, 349, 392, 330, 349, 392};
int noteDurations[] = {400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 800, 400, 400, 800}; 

volatile byte currentTemp = 0;
volatile byte currentDist = 0;

volatile byte i2cData[2] = {0, 0}; 

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(MOTOR_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  dht.begin(); // Start the DHT sensor
  
  Wire.onReceive(receiveEvent); 
  Wire.onRequest(requestEvent); 
  Wire.begin(SLAVE_ADDR);       
  
  xTaskCreate(TaskBuzzer, "Buzzer Task", 2048, NULL, 1, NULL);
  xTaskCreate(TaskMotorControl, "Motor Task", 2048, NULL, 1, NULL);
  xTaskCreate(TaskTemperature, "Temp Task", 2048, NULL, 1, NULL);
  xTaskCreate(TaskProximity, "Prox Task", 2048, NULL, 1, NULL);
}

void loop() { }

// --- I2C Interrupts ---
void receiveEvent(int howMany) {
  while (Wire.available()) {
    int command = Wire.read();
    if (command == 1) playMartinillo = true;
    else if (command == 2) triggerMotorSequence = true;
    else if (command == 3) triggerErrorBuzzer = true;
  }
}

void requestEvent() {
  i2cData[0] = currentTemp;
  i2cData[1] = currentDist;
  Wire.write((uint8_t*)i2cData, 2); 
}

// --- Sensor Tasks ---

void TaskTemperature(void *pvParameters) {
  while (1) {
    // Read temperature as Celsius
    float t = dht.readTemperature();
    
    // Check if the read failed (common with DHT11s)
    if (!isnan(t)) {
      currentTemp = (byte)t; 
    } else {
      Serial.println("Failed to read from DHT sensor!");
    }
    
    // CRITICAL: The DHT11 physical hardware requires 2 seconds between reads.
    // Reading faster will cause it to lock up and output NaN.
    vTaskDelay(2000 / portTICK_PERIOD_MS); 
  }
}

void TaskProximity(void *pvParameters) {
  while (1) {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    
    long duration = pulseIn(ECHO_PIN, HIGH, 30000); 
    
    if (duration > 0) {
      long distanceCm = duration * 0.034 / 2;
      currentDist = (distanceCm > 255) ? 255 : (byte)distanceCm; 
    }
    
    vTaskDelay(30 / portTICK_PERIOD_MS); 
  }
}

// --- Motor and Buzzer Tasks ---

void TaskMotorControl(void *pvParameters) {
  while (1) {
    if (triggerMotorSequence) {
      triggerMotorSequence = false; 

      digitalWrite(MOTOR_PIN, HIGH);
      vTaskDelay(5000 / portTICK_PERIOD_MS);
      digitalWrite(MOTOR_PIN, LOW);

      bool buttonPressed = false;
      for(int i = 0; i < 100; i++) { 
         if (digitalRead(BUTTON_PIN) == HIGH) { 
            Serial.write("Botón presionado");
            buttonPressed = true;
            break; 
         }
         vTaskDelay(100 / portTICK_PERIOD_MS);
      }

      if (!buttonPressed) {
        while (digitalRead(BUTTON_PIN) == LOW) { 
          Serial.write("Botón no presionado");
           tone(BUZZER_PIN, 1000); 
           vTaskDelay(50 / portTICK_PERIOD_MS);
        }
        noTone(BUZZER_PIN); 
      }

      vTaskDelay(500 / portTICK_PERIOD_MS); 
      digitalWrite(MOTOR_PIN, HIGH);
      vTaskDelay(5000 / portTICK_PERIOD_MS);
      digitalWrite(MOTOR_PIN, LOW);
    }
    vTaskDelay(100 / portTICK_PERIOD_MS); 
  }
}

void TaskBuzzer(void *pvParameters) {
  while (1) {
    if (playMartinillo) {
      for (int i = 0; i < 14; i++) {
        tone(BUZZER_PIN, martinilloNotes[i]);
        vTaskDelay((noteDurations[i] * 0.9) / portTICK_PERIOD_MS); 
        noTone(BUZZER_PIN);
        vTaskDelay((noteDurations[i] * 0.1) / portTICK_PERIOD_MS); 
      }
      playMartinillo = false; 
    }
    
    if (triggerErrorBuzzer) {
      tone(BUZZER_PIN, 500); 
      vTaskDelay(5000 / portTICK_PERIOD_MS); 
      noTone(BUZZER_PIN);
      triggerErrorBuzzer = false;
    }
    
    vTaskDelay(100 / portTICK_PERIOD_MS); 
  }
}
