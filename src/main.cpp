#include <Arduino.h>
#include "rgb_lcd.h"

// Mapping des E/S
#define PIN_POT     33
#define PIN_BTN1    2
#define PIN_BTN2    12
#define PIN_DIR     25
#define PIN_ON      26
#define PIN_PWM     27

// Paramètres PWM corrigés
const int pwmFreq = 25000;
const int pwmChannel = 0;
// 11 bits (Max 2047)
const int pwmResolution = 11; 

rgb_lcd lcd;

void setup() {
  Serial.begin(115200);

  Wire1.setPins(15, 5);
  lcd.begin(16, 2, LCD_5x8DOTS, Wire1);
  lcd.setRGB(0, 255, 0);
  lcd.print("Moteur OK ?");

  pinMode(PIN_BTN1, INPUT_PULLUP);
  pinMode(PIN_BTN2, INPUT_PULLUP);
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_ON, OUTPUT);

  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(PIN_PWM, pwmChannel);

  digitalWrite(PIN_ON, HIGH); 
  digitalWrite(PIN_DIR, LOW);
  ledcWrite(pwmChannel, 0);   
}

void loop() {
  int rawPot = analogRead(PIN_POT);
  
  // On mappe vers 2047 (la valeur max de 11 bits)
  int dutyCycle = map(rawPot, 0, 4095, 0, 2047);

  if (digitalRead(PIN_BTN1) == LOW) {
    digitalWrite(PIN_DIR, LOW);
    lcd.setCursor(0, 1); lcd.print("Sens: H   ");
  } 
  else if (digitalRead(PIN_BTN2) == LOW) {
    digitalWrite(PIN_DIR, HIGH);
    lcd.setCursor(0, 1); lcd.print("Sens: AH  ");
  } else {
     lcd.setCursor(0, 1); lcd.print("Attente...");
  }

  ledcWrite(pwmChannel, dutyCycle);
  
  Serial.print("PWM: "); Serial.println(dutyCycle);
  
  delay(50);
}