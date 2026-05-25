#include <Wire.h>
#include <Keypad.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <string.h>

#define SLAVE_ADDR 0x08

// --- OLED Setup ---
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
#define OLED_RESET    -1 
#define SCREEN_ADDRESS 0x3C 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Keypad Setup ---
const byte ROWS = 4; 
const byte COLS = 4; 
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {13, 12, 14, 27}; 
byte colPins[COLS] = {26, 25, 33, 32}; 
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// --- Password Variables ---
const char CORRECT_PASSWORD[] = "123456"; 
char enteredPassword[7] = "";             
byte digitCount = 0;

volatile byte failedAttempts = 0;
volatile bool passwordMode = false;
volatile unsigned long lockoutStartTime = 0;
const unsigned long LOCKOUT_DURATION = 30000; 

void setup() {
  Serial.begin(115200);
  Wire.begin(); 
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); 
  }
  
  display.clearDisplay();
  display.setTextSize(1);      
  display.setTextColor(SSD1306_WHITE); 
  display.setCursor(0, 0);     
  display.println(F("System Ready"));
  display.display();
  
  // FIXED: Increased stack sizes significantly to prevent overflow
  xTaskCreate(TaskKeypad, "Keypad Task", 8192, NULL, 1, NULL);
  xTaskCreate(TaskSensorPoll, "Sensor Poll", 4096, NULL, 1, NULL); 
}

void loop() { }

// --- Tasks ---

void TaskSensorPoll(void *pvParameters) {
  while (1) {
    if (!passwordMode && failedAttempts < 3) { 
      Wire.requestFrom(SLAVE_ADDR, 2); 
      
      if (Wire.available() == 2) {
        byte temp = Wire.read();
        byte dist = Wire.read();
        
        display.clearDisplay();
        display.setTextSize(2); 
        display.setTextColor(SSD1306_WHITE);
        
        display.setCursor(0, 0);
        display.print("T: "); 
        display.print(temp); 
        display.print(" C"); 
        
        display.setCursor(0, 32);
        display.print("D: "); 
        if (dist == 255) {
           display.print("MAX");
        } else {
           display.print(dist); 
           display.print("cm");
        }
        
        display.display(); 
      }
    }
    
    vTaskDelay(300 / portTICK_PERIOD_MS); 
  }
}

void TaskKeypad(void *pvParameters) {
  while (1) {
    char key = keypad.getKey();
    
    if (key) { 
      if (failedAttempts >= 3) {
        if (millis() - lockoutStartTime < LOCKOUT_DURATION) {
          display.clearDisplay();
          display.setTextSize(1);
          display.setCursor(0, 20);
          display.println("SYSTEM LOCKED");
          display.print("Wait 30s...");
          display.display();
          
          vTaskDelay(200 / portTICK_PERIOD_MS);
          continue; 
        } else {
          failedAttempts = 0;
          display.clearDisplay(); 
          display.display();
        }
      }

      if (!passwordMode) {
        if (key == '1') {
          Wire.beginTransmission(SLAVE_ADDR);
          Wire.write(1); 
          Wire.endTransmission();
          
          display.clearDisplay();
          display.setTextSize(1);
          display.setCursor(0, 20);
          display.println("Playing Melody...");
          display.display();
        }
        else if (key == '2') {
          passwordMode = true;
          digitCount = 0;
          memset(enteredPassword, 0, sizeof(enteredPassword)); 
          
          display.clearDisplay();
          display.setTextSize(1);
          display.setCursor(0, 0);
          display.println("Enter Password:");
          display.display();
        }
      } 
      else { 
        if (key >= '0' && key <= '9') { 
          enteredPassword[digitCount] = key;
          digitCount++;
          
          display.clearDisplay();
          display.setTextSize(1);
          display.setCursor(0, 0);
          display.println("Enter Password:");
          display.setCursor(0, 20);
          display.setTextSize(2);
          for(int i=0; i<digitCount; i++) {
            display.print("*");
          }
          display.display();
          
          if (digitCount == 6) {
            if (strcmp(enteredPassword, CORRECT_PASSWORD) == 0) {
              failedAttempts = 0;
              passwordMode = false;
              
              display.clearDisplay();
              display.setTextSize(1);
              display.setCursor(0, 20);
              display.println("ACCESS GRANTED");
              display.display();
              
              Wire.beginTransmission(SLAVE_ADDR);
              Wire.write(2); 
              Wire.endTransmission();
              
              vTaskDelay(2000 / portTICK_PERIOD_MS); 
            } else {
              failedAttempts++;
              passwordMode = false; 
              
              display.clearDisplay();
              display.setTextSize(1);
              display.setCursor(0, 20);
              display.println("ACCESS DENIED");
              display.display();
              
              if (failedAttempts >= 3) {
                lockoutStartTime = millis();
                Wire.beginTransmission(SLAVE_ADDR);
                Wire.write(3); 
                Wire.endTransmission();
              }
              
              vTaskDelay(2000 / portTICK_PERIOD_MS); 
            }
          }
        } 
        else if (key == 'C') { 
           passwordMode = false;
        }
      }
    }
    vTaskDelay(50 / portTICK_PERIOD_MS); 
  }
}
