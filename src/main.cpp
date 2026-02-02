#include <Arduino.h>
#include "rgb_lcd.h"

// --- Définition des broches
#define PIN_POT     33
#define PIN_BTN1    2   // BP1 (Probablement le Bleu ou Jaune)
#define PIN_BTN2    12  // BP2
// Note: Le schéma indique souvent que les boutons ferment vers la masse (GND).
// On utilise donc INPUT_PULLUP pour avoir un "1" au repos et "0" quand appuyé.

rgb_lcd lcd;

void setup() {
  // 1. Initialisation Série
  Serial.begin(115200);

  // 2. Initialisation LCD
  Wire1.setPins(15, 5);
  lcd.begin(16, 2, LCD_5x8DOTS, Wire1);
  lcd.setRGB(0, 0, 200); // Fond bleu
  lcd.print("Test IHM...");

  // 3. Configuration des broches
  // Si le bouton n'est pas appuyé, on lit 1 (HIGH).
  // Si on appuie, on lit 0 (LOW).
  pinMode(PIN_BTN1, INPUT_PULLUP);
  pinMode(PIN_BTN2, INPUT_PULLUP);
  
}

void loop() {
  // --- Lecture des capteurs ---
  int valBtn1 = digitalRead(PIN_BTN1);
  int valBtn2 = digitalRead(PIN_BTN2);
  int valPot  = analogRead(PIN_POT); // Valeur entre 0 et 4095

  // --- Affichage Terminal (pour débugger) ---
  Serial.print("BP1: ");
  Serial.print(valBtn1);
  Serial.print(" | BP2: ");
  Serial.print(valBtn2);
  Serial.print(" | Pot: ");
  Serial.println(valPot);

  // --- Affichage LCD ---
  lcd.setCursor(0, 0);
  lcd.print("B1:"); 
  lcd.print(valBtn1); // Affiche 0 ou 1
  lcd.print(" B2:"); 
  lcd.print(valBtn2); // Affiche 0 ou 1
  
  lcd.setCursor(0, 1);
  lcd.print("Pot: ");
  lcd.print(valPot);
  lcd.print("    "); // Espaces pour effacer les vieux chiffres

  delay(200); // pause pour ne pas saturer l'écran
}