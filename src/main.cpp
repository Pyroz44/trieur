#include <Arduino.h>
#include "rgb_lcd.h"
#include <ESP32Encoder.h>

// --- Configuration Matérielle ---
// Basé sur tes tests validés (DIR=26, ON=25)
#define PIN_POT     33
#define PIN_BTN1    2
#define PIN_BTN2    12
#define PIN_DIR     26  
#define PIN_ON      25  
#define PIN_PWM     27
#define PIN_ENC_A   23
#define PIN_ENC_B   19

// --- Paramètres Système ---
const int ENC_RES = 413;        // Résolution mesurée (points par tour)
const int PWM_CHAN = 0;
const int PWM_RES = 11;         // 11 bits
const int PWM_MAX = 2047;       // Valeur max PWM

// --- Paramètres Asservissement ---
// Kp augmenté pour vaincre les frottements (Fluidité)
float Kp = 25.0; 
// Seuil minimal pour envoyer du jus au moteur (Anti-grésillement)
int minPWM = 80; 

rgb_lcd lcd;
ESP32Encoder encoder;

void setup() {
  Serial.begin(115200);
  
  // LCD
  Wire1.setPins(15, 5);
  lcd.begin(16, 2, LCD_5x8DOTS, Wire1);
  lcd.setRGB(50, 50, 200);
  lcd.print("Mode Servo P+");

  // I/O
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_ON, OUTPUT);
  
  // PWM
  ledcSetup(PWM_CHAN, 25000, PWM_RES);
  ledcAttachPin(PIN_PWM, PWM_CHAN);

  // Moteur ON
  digitalWrite(PIN_ON, HIGH);

  // Codeur
  ESP32Encoder::useInternalWeakPullResistors = UP;
  encoder.attachHalfQuad(PIN_ENC_A, PIN_ENC_B);
  encoder.setCount(0); // Position 0 au démarrage
}

void loop() {
  // 1. Consigne (Potentiomètre -> Angle en points codeur)
  int valPot = analogRead(PIN_POT);
  // On borne la consigne entre 0 et 413 (1 tour)
  int targetPos = map(valPot, 0, 4095, 0, ENC_RES);

  // 2. Mesure (Position réelle)
  long currentPos = encoder.getCount();

  // 3. Erreur
  int error = targetPos - currentPos;

  // 4. Commande (P)
  int controlSignal = error * Kp;

  // 5. Pilotage Moteur
  // Gestion du sens
  if (controlSignal > 0) {
    digitalWrite(PIN_DIR, LOW);
  } else {
    digitalWrite(PIN_DIR, HIGH);
    controlSignal = -controlSignal; // Valeur absolue pour le PWM
  }

  // Saturation (Max PWM)
  if (controlSignal > PWM_MAX) controlSignal = PWM_MAX;
  
  // Zone Morte (Pour éviter la surchauffe à l'arrêt, mais assez bas pour être précis)
  if (controlSignal < minPWM) controlSignal = 0;

  // Envoi
  ledcWrite(PWM_CHAN, controlSignal);

  // --- Affichage (Rafraîchissement limité) ---
  static long lastTime = 0;
  if (millis() - lastTime > 150) {
    lastTime = millis();
    
    lcd.setCursor(0, 0);
    lcd.print("C:"); lcd.print(targetPos);
    lcd.print(" M:"); lcd.print(currentPos); 
    lcd.print("   "); // Effacement fin de ligne

    lcd.setCursor(0, 1);
    // Affiche l'erreur pour voir si l'asservissement est précis
    lcd.print("Err:"); lcd.print(error);
    lcd.print(" PWM:"); lcd.print(controlSignal);
    lcd.print("  ");
  }
}