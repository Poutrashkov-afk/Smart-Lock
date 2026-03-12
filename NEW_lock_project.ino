#include <SoftwareSerial.h>
SoftwareSerial RFID(2, 3);

#include "Adafruit_NeoPixel.h"
#ifdef __AVR__
  #include <avr/power.h>
#endif

#define PIN 6
#define NUMPIXELS 10

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// --- Timing ---
unsigned long previousMillis = 0;
unsigned long stateStartMillis = 0;
const unsigned long LED_INTERVAL = 50;
const unsigned long STATE_DURATION = 1500;

// --- State Machine ---
enum LedState { IDLE_STATE, ACCEPTED_STATE, REFUSED_STATE };
LedState currentState = IDLE_STATE;
int currentPixel = 0;

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

  pixels.setBrightness(25);
  pixels.begin();

  RFID.begin(9600);
  Serial.begin(9600);

  pinMode(yes, OUTPUT);
  pinMode(no, OUTPUT);

  setAllPixels(255, 128, 0);
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
void setAllPixels(uint8_t r, uint8_t g, uint8_t b)
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
    digitalWrite(yes, HIGH);
  }
  else
  {
    Serial.println(F("  >> RESULTAT: REJETE <<"));
    Serial.println(F("  Etat: IDLE -> REFUSE"));
    currentState = REFUSED_STATE;
    digitalWrite(no, HIGH);
  }

  Serial.println(F("--------------------------"));

  currentPixel = 0;
  stateStartMillis = millis();
  previousMillis = millis();
}

// ===================== LEDs =====================
void updateLEDs()
{
  unsigned long now = millis();

  switch (currentState)
  {
    case IDLE_STATE:
      setAllPixels(255, 128, 0);
      pixels.show();
      break;

    case ACCEPTED_STATE:
      if (currentPixel < NUMPIXELS && (now - previousMillis >= LED_INTERVAL))
      {
        pixels.setPixelColor(currentPixel, pixels.Color(0, 255, 0));
        pixels.show();
        Serial.print(F("  LED "));
        Serial.print(currentPixel);
        Serial.println(F(" -> GREEN"));
        currentPixel++;
        previousMillis = now;
      }
      if (now - stateStartMillis >= STATE_DURATION)
      {
        Serial.println(F("  Etat: ACCEPTE -> IDLE"));
        Serial.println(F("En attente de la carte RFID..."));
        Serial.println(F("--------------------------"));
        digitalWrite(yes, LOW);
        currentState = IDLE_STATE;
      }
      break;

    case REFUSED_STATE:
      if (currentPixel < NUMPIXELS && (now - previousMillis >= LED_INTERVAL))
      {
        pixels.setPixelColor(currentPixel, pixels.Color(255, 0, 0));
        pixels.show();
        Serial.print(F("  LED "));
        Serial.print(currentPixel);
        Serial.println(F(" -> RED"));
        currentPixel++;
        previousMillis = now;
      }
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

// ===================== LOOP =====================
void loop()
{
  readTags();
  updateLEDs();
}