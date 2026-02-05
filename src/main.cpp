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

// Variable partagée pour lire la vitesse
volatile long vitesseMesuree = 0; 

// --- TÂCHE FREERTOS : Le Métronome ---
// Cette tâche s'exécute indépendamment du reste
void vTaskPeriodic(void *pvParameters) {
  TickType_t xLastWakeTime;
  // Période de 100ms (10 fois par seconde)
  const TickType_t xFrequency = pdMS_TO_TICKS(100);

  // Initialisation du temps
  xLastWakeTime = xTaskGetTickCount();
  
  long anciennePosition = 0;

  while (1) {
    // 1. Attendre exactement 100ms depuis le dernier réveil
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

    // 2. Lire la position actuelle
    long nouvellePosition = encoder.getCount();

    // 3. Calculer la différence (Vitesse = Distance / Temps)
    // Comme le temps est fixe (100ms), la différence EST la vitesse
    vitesseMesuree = nouvellePosition - anciennePosition;

    // 4. Mémoriser pour le prochain tour
    anciennePosition = nouvellePosition;
  }
}

void setup() {
  Serial.begin(115200);
  Wire1.setPins(15, 5);
  lcd.begin(16, 2, LCD_5x8DOTS, Wire1);
  lcd.setRGB(0, 200, 255); 

  // Init Moteur
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_ON, OUTPUT);
  pinMode(PIN_POT, INPUT);
  
  ledcSetup(0, 25000, 11);
  ledcAttachPin(PIN_PWM, 0);
  digitalWrite(PIN_ON, HIGH);

  // Init Codeur
  ESP32Encoder::useInternalWeakPullResistors = UP;
  encoder.attachHalfQuad(PIN_ENC_A, PIN_ENC_B);
  
  // --- CRÉATION DE LA TÂCHE FREERTOS ---
  xTaskCreate(
    vTaskPeriodic,    // Fonction
    "MesureVitesse",  // Nom
    10000,            // Mémoire pile
    NULL,             // Paramètres
    1,                // Priorité
    NULL              // Handle
  );
}

void loop() {
  // 1. Commande Manuelle (Potentiomètre)
  int valPot = analogRead(PIN_POT);
  int pwm = 0;
  
  // Gestion simple avant/arrière avec zone morte
  if (valPot > 2200) {
    digitalWrite(PIN_DIR, LOW);
    pwm = map(valPot, 2200, 4095, 0, 2047); 
  } 
  else if (valPot < 1900) {
    digitalWrite(PIN_DIR, HIGH);
    pwm = map(valPot, 1900, 0, 0, 2047);
  }

  ledcWrite(0, pwm);

  // 2. Affichage
  static long lastDisplay = 0;
  if (millis() - lastDisplay > 200) {
    lastDisplay = millis();
    
    lcd.setCursor(0, 0);
    lcd.print("PWM: "); lcd.print(pwm); lcd.print("   ");
    
    lcd.setCursor(0, 1);
    // On affiche la valeur calculée par la tâche FreeRTOS
    lcd.print("Vit: "); lcd.print(vitesseMesuree); 
    lcd.print(" pts   ");
  }
}