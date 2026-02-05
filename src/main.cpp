#include <Arduino.h>
#include "rgb_lcd.h"
#include <ESP32Encoder.h>

// --- Définition des broches (Pins) ---
#define PIN_POT     33
#define PIN_BTN1    2   // Bouton Jaune
#define PIN_BTN2    12  // Bouton Bleu/Vert
#define PIN_DIR     26  // Direction du moteur
#define PIN_ON      25  // Activation du moteur
#define PIN_PWM     27  // Vitesse du moteur
#define PIN_ENC_A   19  // Codeur A (inversé avec B pour le bon sens)
#define PIN_ENC_B   23  // Codeur B
#define PIN_IR0     36  // Capteur Optique (CNY70)

// --- Constantes du TP ---
const int ENC_RES = 408;        // Nombre de points pour 1 tour (mesuré en TP)
const int SEUIL_CAPTEUR = 3000; // Seuil pour détecter le bras devant le capteur

// --- Réglages du PID (Asservissement) ---
float Kp = 25.0;       // Gain proportionnel (Puissance)
float Ki = 0.2;        // Gain intégral (Correction erreur statique)
int maxSpeed = 1500;   // Vitesse max autorisée

// Force minimale pour faire bouger le moteur (lutte contre les frottements)
int forceMinimale = 700; 

// Vitesse spécifique pour le retour à la maison (un peu plus rapide pour pas bloquer)
int returnSpeed = 900; 

// --- Machine à États ---
// Liste des étapes possibles du système
enum Etat { ATTENTE, CYCLE_HORAIRE, CYCLE_ANTI_HORAIRE, RETOUR_LENT, CALIBRAGE_FINAL };
Etat etatCourant = ATTENTE;

// Variables globales
int targetPos = 0;       // La position qu'on veut atteindre (consigne)
float errorSum = 0;      // Somme des erreurs (pour le Ki)
int stepIndex = 0;       // Numéro de l'étape en cours (0 à 8)
long lastStepTime = 0;   // Chronomètre pour les pauses d'1 seconde

rgb_lcd lcd;
ESP32Encoder encoder;

// --- Fonction POM (Prise d'Origine Machine) ---
// Sert à trouver le zéro physique grâce au capteur optique
void chercherCapteur() {
  lcd.setRGB(255, 100, 0); // Ecran Orange
  lcd.clear(); lcd.print("Calibrage...");
  
  // Si le capteur voit DEJA le bras, on est bon, on ne bouge pas
  if (analogRead(PIN_IR0) > SEUIL_CAPTEUR) {
      ledcWrite(0, 0);
      encoder.setCount(0); // On remet le compteur à 0 ici
      targetPos = 0;
      errorSum = 0;
      lcd.setRGB(0, 255, 0);
      lcd.clear(); lcd.print("Pret (Deja OK)");
      delay(500);
      return; // On quitte la fonction
  }

  // Sinon, on cherche le capteur
  ledcWrite(0, 0); 
  delay(200);

  // On regarde dans quel sens tourner pour revenir vers 0
  bool doitReculer = (encoder.getCount() > 0);
  if (doitReculer) digitalWrite(PIN_DIR, HIGH);
  else digitalWrite(PIN_DIR, LOW);

  // On lance le moteur avec la force minimale
  ledcWrite(0, forceMinimale); 

  // On attend tant que le capteur ne voit rien
  long timeout = millis();
  while(analogRead(PIN_IR0) < SEUIL_CAPTEUR) {
    // Sécurité : si ça dure plus de 4s, on arrête pour pas tout casser
    if (millis() - timeout > 4000) break; 
  }
  
  // On a trouvé le capteur : Arrêt complet
  ledcWrite(0, 0);
  encoder.setCount(0); // C'est notre nouveau zéro
  targetPos = 0;
  errorSum = 0;
  
  lcd.setRGB(0, 255, 0); // Vert
  lcd.clear(); lcd.print("Pret");
}

// --- Fonction d'Asservissement (PID) ---
// Appellée en boucle pour corriger la position du moteur
void asservissement() {
  long currentPos = encoder.getCount();
  int error = targetPos - currentPos; // Erreur = Consigne - Mesure

  // Zone morte : si on est très proche, on considère que l'erreur est nulle
  // Ça évite que le moteur grésille à l'arrêt
  if (abs(error) <= 2) { 
    errorSum = 0; 
    error = 0; 
  } else { 
    errorSum += error; // Accumulation pour l'intégrale
  }
  
  // Anti-Windup : On borne l'intégrale pour pas qu'elle devienne énorme
  if (errorSum > 1000) errorSum = 1000;
  if (errorSum < -1000) errorSum = -1000;

  // Calcul de la commande
  float controlSignal = (error * Kp) + (errorSum * Ki);

  // Gestion du sens de rotation et conversion en positif
  if (controlSignal > 0) {
    digitalWrite(PIN_DIR, LOW);
  } else {
    digitalWrite(PIN_DIR, HIGH);
    controlSignal = -controlSignal;
    controlSignal += 150; // Petit boost car le moteur force plus en marche arrière
  }

  // --- Astuce pour les frottements ---
  // On définit la force minimale pour que le moteur décolle
  int forceActive = forceMinimale; 

  // Si on est en train de faire le retour maison, on force plus fort (950)
  // pour être sûr de ne pas faire de pause
  if (etatCourant == RETOUR_LENT) {
      forceActive = 950; 
  }

  // Si on doit bouger, on applique au moins la force minimale (Kickstart)
  if (error != 0 && controlSignal < forceActive) {
      controlSignal = forceActive;
  }

  // Limitation de la vitesse max (Sécurité)
  int limite = maxSpeed;
  if (etatCourant == RETOUR_LENT) limite = returnSpeed; // Vitesse différente pour le retour
  
  if (controlSignal > limite) controlSignal = limite;
  
  // Si l'erreur est nulle, on coupe vraiment le moteur
  if (error == 0) controlSignal = 0;

  // Envoi de la commande au moteur
  ledcWrite(0, (int)controlSignal);
}

void setup() {
  Serial.begin(115200);
  // Init LCD
  Wire1.setPins(15, 5);
  lcd.begin(16, 2, LCD_5x8DOTS, Wire1);
  
  // Init Boutons et Capteurs
  pinMode(PIN_BTN1, INPUT_PULLUP);
  pinMode(PIN_BTN2, INPUT_PULLUP);
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_ON, OUTPUT);
  pinMode(PIN_IR0, INPUT);
  
  // Init PWM Moteur
  ledcSetup(0, 25000, 11);
  ledcAttachPin(PIN_PWM, 0);
  digitalWrite(PIN_ON, HIGH); // On allume le driver moteur

  // Init Codeur
  ESP32Encoder::useInternalWeakPullResistors = UP;
  encoder.attachHalfQuad(PIN_ENC_A, PIN_ENC_B);
  
  // On fait une première calibration au démarrage
  chercherCapteur();
}

void loop() {
  long currentPos = encoder.getCount();
  // On regarde si on est arrivé (erreur inférieure à 25 points)
  bool positionAtteinte = (abs(targetPos - currentPos) < 25); 

  // Machine à états pour gérer les cycles
  switch (etatCourant) {
    case ATTENTE:
      lcd.setRGB(50, 50, 50);
      lcd.setCursor(0, 0); lcd.print("Pret. BP1/BP2 ? ");
      targetPos = 0; // On maintient la position 0
      
      // Choix du cycle avec les boutons
      if (digitalRead(PIN_BTN1) == LOW) {
        etatCourant = CYCLE_HORAIRE;
        stepIndex = 0;
        targetPos = 0;
        lastStepTime = millis();
        lcd.clear();
      }
      else if (digitalRead(PIN_BTN2) == LOW) {
        etatCourant = CYCLE_ANTI_HORAIRE;
        stepIndex = 0;
        targetPos = 0;
        lastStepTime = millis();
        lcd.clear();
      }
      break;

    case CYCLE_HORAIRE:
      lcd.setRGB(255, 200, 0);
      lcd.setCursor(0, 0); lcd.print("Cycle H >>>     ");
      
      // On change d'étape si : 1s passée ET position atteinte
      // OU si 2s sont passées (sécurité pour pas rester bloqué)
      if ((millis() - lastStepTime > 1000 && positionAtteinte) || (millis() - lastStepTime > 2000)) {
        lastStepTime = millis();
        stepIndex++;
        
        if (stepIndex <= 8) {
           // Calcul de la prochaine position (1/8ème de tour)
           targetPos = (ENC_RES * stepIndex) / 8;
        } else {
           // Fin du cycle -> On rentre
           etatCourant = RETOUR_LENT;
           targetPos = 0;
        }
      }
      break;

    case CYCLE_ANTI_HORAIRE:
      lcd.setRGB(0, 100, 255);
      lcd.setCursor(0, 0); lcd.print("Cycle AH <<<    ");

      // Même logique pour le sens anti-horaire
      if ((millis() - lastStepTime > 1000 && positionAtteinte) || (millis() - lastStepTime > 2000)) {
        lastStepTime = millis();
        stepIndex++;
        
        if (stepIndex <= 8) {
            // Position négative pour tourner dans l'autre sens
            targetPos = - (ENC_RES * stepIndex) / 8;
        } else {
            etatCourant = RETOUR_LENT;
            targetPos = 0;
        }
      }
      break;
      
    case RETOUR_LENT:
      lcd.setRGB(100, 0, 100);
      lcd.setCursor(0, 0); lcd.print("Retour Maison...");
      
      // Dès qu'on est proche du capteur (moins de 150 points),
      // on coupe le PID et on laisse la fonction de calibration finir le travail
      // pour éviter les accoups.
      if (abs(currentPos) < 150) { 
        etatCourant = CALIBRAGE_FINAL;
      }
      break;

    case CALIBRAGE_FINAL:
      // Finition précise avec le capteur optique
      chercherCapteur();
      etatCourant = ATTENTE;
      break;
  }

  // On lance l'asservissement en permanence sauf pendant la calibration manuelle
  if (etatCourant != CALIBRAGE_FINAL) {
      asservissement();
  }
}