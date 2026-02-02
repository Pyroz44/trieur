#include <Arduino.h>
#include "rgb_lcd.h"
#include <ESP32Encoder.h>

// --- Config ---
#define PIN_POT     33
#define PIN_BTN1    2   
#define PIN_BTN2    12  
#define PIN_DIR     26  
#define PIN_ON      25  
#define PIN_PWM     27
#define PIN_ENC_A   23
#define PIN_ENC_B   19

const int ENC_RES = 413;     
const int POS_BAC_1 = 0;     
const int POS_BAC_2 = 200;   

// --- Nouveaux Réglages ---
float Kp = 7.0;             // Avant: 15.0 -> Beaucoup plus doux
float Ki = 0.05;            // Avant: 0.8 -> Accumulation très lente
int maxSpeed = 1500;        // Vitesse max réduite

// Variables
int targetPos = 0; 
float errorSum = 0;

rgb_lcd lcd;
ESP32Encoder encoder;

void setup() {
  Serial.begin(115200);
  Wire1.setPins(15, 5);
  lcd.begin(16, 2, LCD_5x8DOTS, Wire1);
  lcd.setRGB(0, 50, 0); // Vert sombre
  lcd.print("Mode Stable");

  pinMode(PIN_BTN1, INPUT_PULLUP);
  pinMode(PIN_BTN2, INPUT_PULLUP);
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_ON, OUTPUT);
  
  ledcSetup(0, 25000, 11);
  ledcAttachPin(PIN_PWM, 0);
  digitalWrite(PIN_ON, HIGH);

  ESP32Encoder::useInternalWeakPullResistors = UP;
  encoder.attachHalfQuad(PIN_ENC_A, PIN_ENC_B);
  encoder.setCount(0); 
}

void loop() {
  // --- 1. Sélecteur ---
  int oldTarget = targetPos;
  
  if (digitalRead(PIN_BTN1) == LOW) targetPos = POS_BAC_1; 
  else if (digitalRead(PIN_BTN2) == LOW) targetPos = POS_BAC_2; 

  if (oldTarget != targetPos) {
    errorSum = 0; // Reset mémoire si changement
  }

  // --- 2. Asservissement PI ---
  long currentPos = encoder.getCount();
  int error = targetPos - currentPos;
  
  // --- Anti-Tremblement (Nouveau) ---
  // Si on est très proche de la cible (à 2 points près), on arrête tout.
  if (abs(error) <= 2) {
    errorSum = 0;
    error = 0; // On force l'erreur à 0 pour couper le moteur
  }
  else {
    errorSum += error;
  }
  
  // Anti-tremblement
  if (errorSum > 2000) errorSum = 2000;
  if (errorSum < -2000) errorSum = -2000;

  // Calcul commande
  float controlSignal = (error * Kp) + (errorSum * Ki);

  // Gestion Sens
  if (controlSignal > 0) {
    digitalWrite(PIN_DIR, LOW);
  } else {
    digitalWrite(PIN_DIR, HIGH);
    controlSignal = -controlSignal;
  }

  if (controlSignal > maxSpeed) controlSignal = maxSpeed;
  
  // Envoi
  ledcWrite(0, (int)controlSignal);

  // --- 3. Affichage ---
  static long lastTime = 0;
  if (millis() - lastTime > 150) {
    lastTime = millis();
    lcd.setCursor(0, 1);
    lcd.print("T:"); lcd.print(targetPos);
    lcd.print(" C:"); lcd.print(currentPos);
    lcd.print("   ");
  }
}