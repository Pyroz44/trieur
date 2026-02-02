#include <Arduino.h>
#include "rgb_lcd.h"

// --- CORRECTION DES BROCHES (Inversion 25/26) ---
// D'après le test, 25 contrôle l'arrêt (ON) et 26 le sens (DIR)
#define PIN_POT     33
#define PIN_BTN1    2
#define PIN_BTN2    12
#define PIN_DIR     26  // C'était 25 avant
#define PIN_ON      25  // C'était 26 avant
#define PIN_PWM     27

// --- Configuration PWM ---
const int pwmFreq = 25000;
const int pwmChannel = 0;
const int pwmResolution = 11;   // 11 bits (Max 2047)
const int pwmMaxDuty = 2047;

rgb_lcd lcd;

void setup() {
  Serial.begin(115200);
  Wire1.setPins(15, 5);
  lcd.begin(16, 2, LCD_5x8DOTS, Wire1);
  lcd.setRGB(50, 50, 50);
  lcd.print("Correction Pins");

  pinMode(PIN_BTN1, INPUT_PULLUP);
  pinMode(PIN_BTN2, INPUT_PULLUP);
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_ON, OUTPUT);

  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(PIN_PWM, pwmChannel);

  // Initialisation
  digitalWrite(PIN_ON, HIGH); // Activer le driver
  digitalWrite(PIN_DIR, LOW);
  ledcWrite(pwmChannel, 0);
}

void loop() {
  // Lecture Potentiomètre
  int rawPot = analogRead(PIN_POT);
  int percent = map(rawPot, 0, 4095, 0, 100);
  int dutyCycle = map(rawPot, 0, 4095, 0, pwmMaxDuty);

  // Zone morte
  if (percent < 2) {
    dutyCycle = 0;
    percent = 0;
  }

  // Gestion du Sens avec les boutons
  if (digitalRead(PIN_BTN1) == LOW) {
    digitalWrite(PIN_DIR, LOW);
    lcd.setRGB(0, 255, 0); // Vert
    lcd.setCursor(0, 0); lcd.print("Sens: A (LOW)   ");
  } 
  else if (digitalRead(PIN_BTN2) == LOW) {
    digitalWrite(PIN_DIR, HIGH);
    lcd.setRGB(0, 0, 255); // Bleu
    lcd.setCursor(0, 0); lcd.print("Sens: B (HIGH)  ");
  }

  // Commande Moteur
  ledcWrite(pwmChannel, dutyCycle);

  // Affichage
  lcd.setCursor(0, 1);
  lcd.print("Vitesse: ");
  lcd.print(percent);
  lcd.print("%   ");

  delay(50);
}