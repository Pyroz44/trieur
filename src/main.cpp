#include <Arduino.h>
#include "rgb_lcd.h"

// Mapping des E/S
#define PIN_POT     33
#define PIN_BTN1    2
#define PIN_BTN2    12
#define PIN_DIR     25
#define PIN_ON      26
#define PIN_PWM     27

// Paramètres PWM
const int pwmFreq = 25000;
const int pwmChannel = 0;
const int pwmResolution = 12;
// Calcul résolution : 80MHz / 25kHz = 3200 pas max

rgb_lcd lcd;

void setup() {
  Serial.begin(115200);

  // Init LCD
  Wire1.setPins(15, 5);
  lcd.begin(16, 2, LCD_5x8DOTS, Wire1);
  lcd.setRGB(255, 255, 255);
  lcd.print("Test Moteur");

  // Configuration IHM
  pinMode(PIN_BTN1, INPUT_PULLUP);
  pinMode(PIN_BTN2, INPUT_PULLUP);

  // Configuration Moteur
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_ON, OUTPUT);

  // Configuration LEDC (PWM)
  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(PIN_PWM, pwmChannel);

  // Conditions initiales
  digitalWrite(PIN_ON, HIGH); // Driver actif
  digitalWrite(PIN_DIR, LOW);
  ledcWrite(pwmChannel, 0);   // Vitesse nulle
}

void loop() {
  // Lecture consigne vitesse
  int rawPot = analogRead(PIN_POT);
  // Mise à l'échelle (0-4095 -> 0-3200)
  int dutyCycle = map(rawPot, 0, 4095, 0, 3200);

  // Gestion sens de rotation
  if (digitalRead(PIN_BTN1) == LOW) {
    digitalWrite(PIN_DIR, LOW);
    lcd.setCursor(0, 1);
    lcd.print("Sens: Horaire   ");
  } 
  else if (digitalRead(PIN_BTN2) == LOW) {
    digitalWrite(PIN_DIR, HIGH);
    lcd.setCursor(0, 1);
    lcd.print("Sens: Anti-Hor  ");
  }

  // Application de la commande
  ledcWrite(pwmChannel, dutyCycle);
  
  delay(50);
}