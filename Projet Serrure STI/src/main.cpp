// --- Libraries ---

#include <SoftwareSerial.h>
#include <wire.h>
#include "rgb_lcd.h"
#include <Arduino.h>
#include "Adafruit_NeoPixel.h"
#include "Grove_I2C_Motor_Driver.h"
#ifdef __AVR__
  #include <avr/power.h>
#endif

// --- Pins ---

#define I2C_ADDRESS 0x0f
#define PIN 6
#define NUMPIXELS 10

// --- Functions ---

void ouverture();
void fermeture();
void updateLEDs();
void readTags();
void setALLpixels(uint8_t r, uint8_t g, uint8_t b);

// --- Objects ---

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
rgb_lcd lcd;
const int Red = 255;
const int Green = 255;
const int Blue = 255;
SoftwareSerial RFID(2, 3);

// --- Timing ---

unsigned long stateStartMillis = 0;
const unsigned long STATE_DURATION = 5000;

// --- State Machine ---

enum LedState { IDLE_STATE, ACCEPTED_STATE, REFUSED_STATE };
LedState currentState = IDLE_STATE;
bool stateJustChanged = false;  // Flag pour savoir si on vient de changer d'état

// --- RFID ---

int yes = 13;
int no = 12;

int tag[14] = {254,254,254,254,254,254,254,254,254,254,254,254,254,254};
int newtag[14] = {0};

// --- Debug ---

unsigned long scanCount = 0;

void setup()
{
#if defined(__AVR_ATtiny85__)
  if (F_CPU == 16000000) clock_prescale_set(clock_div_1);
#endif

  Motor.begin(I2C_ADDRESS);

  lcd.begin(16, 2);
  lcd.setRGB(255, 128, 0);
  lcd.setCursor(1, 0);
  lcd.display();
  lcd.print("Serrure fermee");

  pixels.setBrightness(25);
  pixels.begin();

  RFID.begin(9600);
  Serial.begin(9600);

  pinMode(yes, OUTPUT);
  pinMode(no, OUTPUT);

  setALLpixels(255, 128, 0);
  pixels.show();

  Serial.println(F("=========================="));
  Serial.println(F("  Initialisation du lecteur RFID"));
  Serial.println(F("=========================="));
  Serial.println(F("Etat: IDLE"));
  Serial.println(F("En attente de la carte RFID..."));
  Serial.println(F("--------------------------"));

  // Print the stored tag for reference
  Serial.print(F("Stored tag: ["));
  for (int i = 0; i < 14; i++)
  {
    Serial.print(tag[i]);
    if (i < 13) Serial.print(F(", "));
  }
  Serial.println(F("]"));
  Serial.println(F("--------------------------"));
}

// ===================== HELPERS =====================

void setALLpixels(uint8_t r, uint8_t g, uint8_t b)
{
  for (int i = 0; i < NUMPIXELS; i++)
    pixels.setPixelColor(i, pixels.Color(r, g, b));
}

boolean comparetag(int aa[], int bb[])
{
  int matchCount = 0;
  for (int i = 0; i < 14; i++)
  {
    if (aa[i] == bb[i]) matchCount++;
  }

  Serial.print(F("  Match score: "));
  Serial.print(matchCount);
  Serial.println(F("/14"));

  return (matchCount == 14);
}

// ===================== RFID =====================

void readTags()
{
  if (RFID.available() < 14) return;

  scanCount++;

  Serial.println(F("=========================="));
  Serial.print(F("  Scan n°"));
  Serial.println(scanCount);
  Serial.println(F("=========================="));
  Serial.println(F("Carte détectée! Lecture..."));

  for (int z = 0; z < 14; z++)
    newtag[z] = RFID.read();

  // Print raw tag data
  Serial.print(F("  Raw tag: ["));
  for (int i = 0; i < 14; i++)
  {
    Serial.print(newtag[i]);
    if (i < 13) Serial.print(F(", "));
  }
  Serial.println(F("]"));

  // Print tag as HEX
  Serial.print(F("  Hex tag: ["));
  for (int i = 0; i < 14; i++)
  {
    if (newtag[i] < 16) Serial.print(F("0"));
    Serial.print(newtag[i], HEX);
    if (i < 13) Serial.print(F(" "));
  }
  Serial.println(F("]"));

  // Print tag as ASCII (printable chars only)
  Serial.print(F("  ASCII:   ["));
  for (int i = 0; i < 14; i++)
  {
    if (newtag[i] >= 32 && newtag[i] <= 126)
      Serial.print((char)newtag[i]);
    else
      Serial.print(F("."));
  }
  Serial.println(F("]"));

  // Drain leftover bytes
  int flushed = 0;
  while (RFID.available())
  {
    RFID.read();
    flushed++;
  }
  if (flushed > 0)
  {
    Serial.print(F("  Flush des octets en trop: "));
    Serial.println(flushed);
  }

  // Compare
  Serial.println(F("  Comparaison avec la carte acceptée..."));

  digitalWrite(yes, LOW);
  digitalWrite(no, LOW);

  if (comparetag(newtag, tag))
  {
    Serial.println(F("  >> RESULTAT: ACCEPTE <<"));
    Serial.println(F("  Etat: IDLE -> ACCEPTE"));
    currentState = ACCEPTED_STATE;
    stateJustChanged = true;  // Marquer le changement d'état
    digitalWrite(yes, HIGH);
  }
  else
  {
    Serial.println(F("  >> RESULTAT: REJETE <<"));
    Serial.println(F("  Etat: IDLE -> REFUSE"));
    currentState = REFUSED_STATE;
    stateJustChanged = true;  // Marquer le changement d'état
    digitalWrite(no, HIGH);
  }

  Serial.println(F("--------------------------"));

  stateStartMillis = millis();
}

// ===================== LEDs =====================

void updateLEDs()
{
  unsigned long now = millis();

  switch (currentState)
  {
    case IDLE_STATE:
      lcd.begin(16, 2);
      lcd.setRGB(255, 128, 0);
      lcd.setCursor(1, 0);
      lcd.display();
      lcd.print("Serrure Fermee");
      setALLpixels(255, 128, 0);
      pixels.show();
      break;

    case ACCEPTED_STATE:
      // Exécuter une seule fois au changement d'état
      if (stateJustChanged)
      {
        lcd.clear();
        lcd.begin(16, 2);
        lcd.setRGB(0, 255, 0);
        lcd.setCursor(1, 0);
        lcd.display();
        lcd.print("Serrure Ouverte");
        
        // Toutes les LEDs en vert immédiatement
        setALLpixels(0, 255, 0);
        pixels.show();
        
        Serial.println(F("  Toutes les LEDs -> GREEN"));
        
        ouverture();
        stateJustChanged = false;  // Reset du flag
      }
      
      // Vérifier si le temps est écoulé
      if (now - stateStartMillis >= STATE_DURATION)
      {
        Serial.println(F("  Etat: ACCEPTE -> IDLE"));
        Serial.println(F("En attente de la carte RFID..."));
        Serial.println(F("--------------------------"));
        digitalWrite(yes, LOW);
        fermeture();
        currentState = IDLE_STATE;
      }
      break;

    case REFUSED_STATE:
      // Exécuter une seule fois au changement d'état
      if (stateJustChanged)
      {
        lcd.clear();
        lcd.begin(16, 2);
        lcd.setRGB(255, 0, 0);
        lcd.setCursor(1, 0);
        lcd.display();
        lcd.print("Acces Refuse");
        
        // Toutes les LEDs en rouge immédiatement
        setALLpixels(255, 0, 0);
        pixels.show();
        
        Serial.println(F("  Toutes les LEDs -> RED"));
        
        Motor.speed(MOTOR1, 0);
        stateJustChanged = false;  // Reset du flag
      }
      
      // Vérifier si le temps est écoulé
      if (now - stateStartMillis >= STATE_DURATION)
      {
        Serial.println(F("  Etat: REFUSE -> IDLE"));
        Serial.println(F("En attente de la carte RFID..."));
        Serial.println(F("--------------------------"));
        digitalWrite(no, LOW);
        currentState = IDLE_STATE;
      }
      break;
  }
}

// ===================== MOTOR =====================

void ouverture()
{
  Motor.speed(MOTOR1, 255);
  delay(2000);
  Motor.speed(MOTOR1, 0);
}

void fermeture()
{
  Motor.speed(MOTOR1, -255);
  delay(2000);
  Motor.speed(MOTOR1, 0);
}

// ===================== LOOP =====================
void loop()
{
  readTags();
  updateLEDs();
}
