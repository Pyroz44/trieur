#include <Arduino.h>
#include "rgb_lcd.h"
#include <ESP32Encoder.h>

// --- Cablage (Selon le tableau excel et la maquette) ---
#define PIN_BTN_BLEU 12 
#define PIN_DIR      26  
#define PIN_ON       25  
#define PIN_PWM      27
#define PIN_ENC_A    19  
#define PIN_ENC_B    23  
#define PIN_IR0      36  // Capteur Noir (Servira pour tout : Zero et Simulation Couleur)

// --- Positions (Reglees manuellement sur la maquette) ---
// Comme on utilise le capteur IR0 pour tout, la mesure se fait en -100
const int POS_ORIGINE = 0;
const int POS_MESURE  = -100; // Position du capteur noir
const int POS_BLANC   = -100; // On reste sur le capteur pour detecter le retrait
const int POS_AUTRE   = 150;  // Une zone vide un peu plus loin

// Seuil experimental (Balle blanche = valeur haute, Rien = valeur basse)
const int SEUIL_DETECT = 2000; 

// Variables globales partagees entre la tache et le loop
volatile long vitesseReelle = 0; 
volatile int consigneVitesse = 0; // Vitesse demandée par la machine à états
volatile bool sensAvance = true;

// --- Machine a etats ---
enum Etat { 
  INIT,           // Retour 0
  ATTENTE_BTN,    // Pose balle + Btn Bleu
  ALLER_MESURE,   // Aller vers le capteur
  ANALYSE,        // Test si Blanc ou Noir
  ALLER_BLANC,    // Zone depot 1
  ATTENTE_RETRAIT,// Attendre qu'on enleve la balle
  ALLER_AUTRE,    // Zone depot 2
  ATTENTE_TEMPS   // Attente 5s
};
Etat etat = INIT;
long chrono = 0; // Pour compter les 5s

rgb_lcd lcd;
ESP32Encoder encoder;

// --- Tache FreeRTOS : Asservissement de Vitesse (100ms) ---
void vTaskAsserv(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t periode = pdMS_TO_TICKS(100); // 100ms pile
  
  long ancienCodeur = 0;
  float erreurSomme = 0.0; // Pour l'Integrale
  
  // Reglagles PI trouves par tests
  float Kp = 10.0; // Pas trop fort pour eviter les accoups
  float Ki = 15.0; // Pour donner la patate si on freine

  while (1) {
    vTaskDelayUntil(&xLastWakeTime, periode); // Attente stricte

    // 1. Calcul Vitesse
    long nouveauCodeur = encoder.getCount();
    vitesseReelle = abs(nouveauCodeur - ancienCodeur);
    ancienCodeur = nouveauCodeur;

    // Si on demande l'arret, on reset tout pour pas que ca reparte d'un coup
    if (consigneVitesse == 0) {
      ledcWrite(0, 0);
      erreurSomme = 0;
      continue;
    }

    // 2. Calcul Erreur et Commande
    float erreur = consigneVitesse - vitesseReelle;
    erreurSomme += erreur;
    
    // Limitation de la somme (Anti-Windup) pour pas saturer
    if (erreurSomme > 1000) erreurSomme = 1000;
    if (erreurSomme < -1000) erreurSomme = -1000;

    float commande = (erreur * Kp) + (erreurSomme * Ki);
    
    // Bornage PWM (0 - 2000 suffisent)
    if (commande < 0) commande = 0;
    if (commande > 2000) commande = 2000;

    // 3. Envoi au moteur
    if (sensAvance) digitalWrite(PIN_DIR, LOW); 
    else digitalWrite(PIN_DIR, HIGH);
    
    ledcWrite(0, (int)commande);
  }
}

// --- Petite fonction pour simplifier le code dans le loop ---
// Renvoie TRUE quand on est arrive
bool allerA(int cible) {
  long pos = encoder.getCount();
  int dist = cible - pos;
  
  // Si on est a moins de 15 points, on considere qu'on est bon
  if (abs(dist) < 15) {
    consigneVitesse = 0; // Stop moteur
    return true;
  }
  
  // Sinon on roule
  consigneVitesse = 22; // Vitesse validee en TP
  if (dist > 0) sensAvance = true;
  else sensAvance = false;
  
  return false;
}

void setup() {
  Wire1.setPins(15, 5);
  lcd.begin(16, 2, LCD_5x8DOTS, Wire1);
  
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_ON, OUTPUT);
  pinMode(PIN_BTN_BLEU, INPUT_PULLUP);
  pinMode(PIN_IR0, INPUT);
  
  // Config PWM
  ledcSetup(0, 25000, 11);
  ledcAttachPin(PIN_PWM, 0);
  digitalWrite(PIN_ON, HIGH); // Allumage carte puissance

  // Codeur
  ESP32Encoder::useInternalWeakPullResistors = UP;
  encoder.attachHalfQuad(PIN_ENC_A, PIN_ENC_B);
  encoder.setCount(0); // On suppose qu'on est a 0 au demarrage

  // Lancement de la tache de fond pour le moteur
  xTaskCreate(vTaskAsserv, "RegulVitesse", 10000, NULL, 1, NULL);
}

void loop() {
  int capteur = analogRead(PIN_IR0); // On utilise le capteur Noir pour tout

  switch (etat) {
    // 1. Init
    case INIT:
      lcd.setRGB(255, 0, 0); 
      lcd.setCursor(0,0); lcd.print("1.Retour 0      ");
      if (allerA(POS_ORIGINE)) {
        etat = ATTENTE_BTN;
      }
      break;

    // 2. Attente Depart
    case ATTENTE_BTN:
      lcd.setRGB(0, 0, 255);
      lcd.setCursor(0,0); lcd.print("2.Attente Bleu  ");
      // Si on appuie sur le bouton bleu
      if (digitalRead(PIN_BTN_BLEU) == LOW) {
        etat = ALLER_MESURE;
        delay(500); // Anti-rebond simple
      }
      break;

    // 3. Deplacement vers le capteur
    case ALLER_MESURE:
      lcd.setRGB(255, 255, 0);
      lcd.setCursor(0,0); lcd.print("3.Go Mesure...  ");
      if (allerA(POS_MESURE)) {
        delay(1000); // On stabilise avant de lire
        etat = ANALYSE;
      }
      break;

    // 4. Decision (C'est ici qu'on triche un peu avec le capteur noir)
    case ANALYSE:
      lcd.setCursor(0,0); lcd.print("4.Analyse...    ");
      // Si valeur forte = Blanc, sinon = Autre
      if (capteur > SEUIL_DETECT) {
        etat = ALLER_BLANC;
      } else {
        etat = ALLER_AUTRE;
      }
      break;

    // 5. Cas Balle BLANCHE
    case ALLER_BLANC:
      lcd.setRGB(255, 255, 255);
      lcd.setCursor(0,0); lcd.print("5.Zone BLANC    ");
      if (allerA(POS_BLANC)) {
        etat = ATTENTE_RETRAIT;
      }
      break;

    // 6. Attente qu'on enleve la balle
    case ATTENTE_RETRAIT:
      lcd.setCursor(0,0); lcd.print("6.Enlevez Balle!");
      // Tant que le capteur voit quelque chose, on attend
      if (capteur < 1000) { 
        delay(1000);
        etat = INIT; // Retour au debut
      }
      break;

    // 7. Cas Balle AUTRE
    case ALLER_AUTRE:
      lcd.setRGB(200, 0, 200);
      lcd.setCursor(0,0); lcd.print("7.Zone AUTRE    ");
      if (allerA(POS_AUTRE)) {
        chrono = millis(); // On lance le chrono
        etat = ATTENTE_TEMPS;
      }
      break;

    // 8. Temporisation 5s
    case ATTENTE_TEMPS:
      lcd.setCursor(0,0); lcd.print("8.Attente 5s... ");
      if (millis() - chrono > 5000) {
        etat = INIT;
      }
      break;
  }
  delay(10); // Petit delay pour laisser respirer le loop
}