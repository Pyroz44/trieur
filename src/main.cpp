#include <Arduino.h>
#include "rgb_lcd.h"
#include <ESP32Encoder.h>

// --- Config Matérielle ---
#define PIN_POT     33
#define PIN_DIR     26  
#define PIN_ON      25  
#define PIN_PWM     27
#define PIN_ENC_A   19  
#define PIN_ENC_B   23  

rgb_lcd lcd;
ESP32Encoder encoder;

// Variables partagées
volatile long vitesseMesuree = 0; 
volatile int commandeEnvoyee = 0;
int consigneVitesse = 22; 

// --- TÂCHE FREERTOS : Régulateur PI ---
void vTaskRegulateur(void *pvParameters) {
  TickType_t xLastWakeTime;
  const TickType_t xFrequency = pdMS_TO_TICKS(100); 
  xLastWakeTime = xTaskGetTickCount();
  
  long anciennePosition = 0;
  
  // --- REGLAGES PI ---
  // Kp baissé (pour arrêter les accoups)
  float Kp = 10.0; 
  // Ki ajouté (pour donner la force de base)
  float Ki = 15.0; 

  // Mémoire de l'intégrale
  float erreurSomme = 0.0;

  while (1) {
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

    long nouvellePosition = encoder.getCount();
    vitesseMesuree = abs(nouvellePosition - anciennePosition); 
    anciennePosition = nouvellePosition;

    float erreur = consigneVitesse - vitesseMesuree;

    // --- CALCUL DE L'INTEGRALE ---
    erreurSomme += erreur;
    
    // Anti-Windup (Important : on borne la mémoire pour pas qu'elle explose)
    if (erreurSomme > 1000) erreurSomme = 1000;
    if (erreurSomme < -1000) erreurSomme = -1000;

    // --- CALCUL COMMANDE (P + I) ---
    float commande = (erreur * Kp) + (erreurSomme * Ki);

    // Sécurités
    if (commande < 0) commande = 0;
    if (commande > 2000) commande = 2000;
    
    commandeEnvoyee = (int)commande; 

    digitalWrite(PIN_DIR, LOW); 
    ledcWrite(0, commandeEnvoyee);
  }
}

void setup() {
  Serial.begin(115200);
  Wire1.setPins(15, 5);
  lcd.begin(16, 2, LCD_5x8DOTS, Wire1);
  lcd.setRGB(0, 255, 0); // Vert = Etape PI validée

  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_ON, OUTPUT);
  
  ledcSetup(0, 25000, 11);
  ledcAttachPin(PIN_PWM, 0);
  digitalWrite(PIN_ON, HIGH);

  ESP32Encoder::useInternalWeakPullResistors = UP;
  encoder.attachHalfQuad(PIN_ENC_A, PIN_ENC_B);
  
  xTaskCreate(vTaskRegulateur, "RegulateurPI", 10000, NULL, 1, NULL);
}

void loop() {
  static long lastDisplay = 0;
  if (millis() - lastDisplay > 200) {
    lastDisplay = millis();
    lcd.setCursor(0, 0); 
    lcd.print("Cib:"); lcd.print(consigneVitesse);
    lcd.print(" PWM:"); lcd.print(commandeEnvoyee); lcd.print("  ");
    
    lcd.setCursor(0, 1); 
    lcd.print("Reelle:"); lcd.print(vitesseMesuree); lcd.print("   ");
  }
}