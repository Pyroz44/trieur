#include <Arduino.h>
#include "rgb_lcd.h"
#include <ESP32Encoder.h>  // 1. On inclut la bibliothèque demandée

// --- Broches (Moteur + Codeur) ---
#define PIN_POT     33
#define PIN_BTN1    2
#define PIN_BTN2    12
#define PIN_DIR     26  // Sens (Broches corrigées précédemment)
#define PIN_ON      25  // Activation
#define PIN_PWM     27
#define PIN_ENC_A   23  // Codeur A (Donnée Brochure)
#define PIN_ENC_B   19  // Codeur B (Donnée Brochure)

// --- Config PWM ---
const int pwmChannel = 0;
const int pwmResolution = 11; 
const int pwmMaxDuty = 2047;

rgb_lcd lcd;
ESP32Encoder encoder; // 2. On crée l'objet codeur

void setup() {
  Serial.begin(115200);
  Wire1.setPins(15, 5);
  lcd.begin(16, 2, LCD_5x8DOTS, Wire1);
  lcd.setRGB(255, 255, 255);
  lcd.print("Test Codeur");

  // IHM
  pinMode(PIN_BTN1, INPUT_PULLUP);
  pinMode(PIN_BTN2, INPUT_PULLUP);

  // Moteur
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_ON, OUTPUT);
  digitalWrite(PIN_ON, HIGH); // Moteur actif
  
  // PWM
  ledcSetup(pwmChannel, 25000, pwmResolution);
  ledcAttachPin(PIN_PWM, pwmChannel);
  ledcWrite(pwmChannel, 0);

  // --- 3. Configuration du Codeur (Comme demandé par le sujet) ---
  // Active les résistances internes pour éviter les parasites
  ESP32Encoder::useInternalWeakPullResistors = UP;
  // Attache les pins A et B au compteur matériel
  encoder.attachHalfQuad(PIN_ENC_A, PIN_ENC_B);
  // Remet le compteur à 0 au démarrage
  encoder.setCount(0);
}

void loop() {
  // --- Gestion Moteur ---
  int rawPot = analogRead(PIN_POT);
  int percent = map(rawPot, 0, 4095, 0, 100);
  int dutyCycle = map(rawPot, 0, 4095, 0, pwmMaxDuty);

  if (percent < 2) { dutyCycle = 0; }

  // Gestion Sens
  if (digitalRead(PIN_BTN1) == LOW) {
    digitalWrite(PIN_DIR, LOW);
  } else if (digitalRead(PIN_BTN2) == LOW) {
    digitalWrite(PIN_DIR, HIGH);
  }

  ledcWrite(pwmChannel, dutyCycle);

  // --- 4. Lecture et Affichage du Codeur ---
  long position = encoder.getCount(); // Lit la position actuelle

  // Affichage LCD
  lcd.setCursor(0, 0);
  lcd.print("Pos: ");
  lcd.print(position);
  lcd.print("      "); // Efface les vieux chiffres

  lcd.setCursor(0, 1);
  lcd.print("Vit: "); lcd.print(percent); lcd.print("%");

  // Affichage Terminal pour la mesure
  Serial.print("Position: ");
  Serial.println(position);

  delay(100);
}