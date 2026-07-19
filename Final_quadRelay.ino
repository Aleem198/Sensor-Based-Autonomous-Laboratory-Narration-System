/*
=========================================================
      SMART AUDIO INFORMATION DISPLAY SYSTEM
=========================================================

ESP32 + HC-SR04 + DFPlayer Mini + BUSY + 4 Relay Module

TRIG  -> GPIO13
ECHO  -> GPIO12

DFPlayer TX -> GPIO16 (ESP RX2)
DFPlayer RX -> GPIO17 (ESP TX2)
BUSY -> GPIO4

Relay1 -> GPIO5
Relay2 -> GPIO18
Relay3 -> GPIO19
Relay4 -> GPIO21

Features
---------
• Detect visitor within 10cm for 1 second
• LEDs ON
• Play Audio
• LEDs stay ON while audio is playing
• Detect again for 2 seconds to stop & reset audio
• LEDs stay ON for 5 sec after finish/reset
• Ready for next visitor
=========================================================
*/

#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// ---------------- PIN DEFINITIONS ----------------

#define TRIG_PIN 13
#define ECHO_PIN 12

#define RELAY1 5
#define RELAY2 18
#define RELAY3 19
#define RELAY4 21

#define BUSY_PIN 4

// ---------------- DFPLAYER ----------------

HardwareSerial mp3(2);
DFRobotDFPlayerMini player;

// ---------------- VARIABLES ----------------

bool playing = false;
bool cooldown = false;

unsigned long startDetect = 0;
unsigned long resetDetect = 0;
unsigned long cooldownStart = 0;

// ---------------- ULTRASONIC ----------------

float getDistance()
{
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0)
    return 999;

  return duration * 0.0343 / 2.0;
}

// ---------------- SETUP ----------------

void setup()
{
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  pinMode(RELAY4, OUTPUT);

  pinMode(BUSY_PIN, INPUT_PULLUP);

  // Relay OFF (Active LOW)

  digitalWrite(RELAY1, HIGH);
  digitalWrite(RELAY2, HIGH);
  digitalWrite(RELAY3, HIGH);
  digitalWrite(RELAY4, HIGH);

  // DFPlayer

  mp3.begin(9600, SERIAL_8N1, 16, 17);

  delay(2000);

  if (!player.begin(mp3))
  {
    Serial.println("DFPlayer NOT FOUND");

    while (1);
  }

  Serial.println("System Ready");

  player.volume(30);

  Serial.println("Waiting For Visitor...");
}

// ---------------- LOOP ----------------

void loop()
{
  float distance = getDistance();

  Serial.print("Distance : ");
  Serial.println(distance);

  // ---------------- COOLDOWN ----------------

  if (cooldown)
  {
    // Keep LEDs ON during cooldown
    digitalWrite(RELAY1, LOW);
    digitalWrite(RELAY2, LOW);
    digitalWrite(RELAY3, LOW);
    digitalWrite(RELAY4, LOW);

    if (millis() - cooldownStart >= 5000)
    {
      Serial.println("Cooldown Finished");

      digitalWrite(RELAY1, HIGH);
      digitalWrite(RELAY2, HIGH);
      digitalWrite(RELAY3, HIGH);
      digitalWrite(RELAY4, HIGH);

      cooldown = false;

      Serial.println("Waiting For Visitor...");
    }

    delay(20);
    return;
  }

  // ---------------- WAIT FOR FIRST DETECTION ----------------

  if (!playing)
  {
    if (distance <= 10)
    {
      if (startDetect == 0)
        startDetect = millis();

      // Visitor must stay for 1 second
      if (millis() - startDetect >= 1000)
      {
        Serial.println("Visitor Confirmed");

        // LEDs ON
        digitalWrite(RELAY1, LOW);
        digitalWrite(RELAY2, LOW);
        digitalWrite(RELAY3, LOW);
        digitalWrite(RELAY4, LOW);

        delay(200);

        // Play Audio
        player.playMp3Folder(1);

        // Give DFPlayer time to start
        delay(500);

        // Wait until BUSY goes LOW
        while (digitalRead(BUSY_PIN) == HIGH)
        {
          delay(10);
        }

        Serial.println("Audio Started");

        playing = true;

        startDetect = 0;
        resetDetect = 0;
      }
    }
    else
    {
      startDetect = 0;
    }
  }

  // ---------------- AUDIO PLAYING ----------------

  if (playing)
  {
    // Keep LEDs ON continuously
    digitalWrite(RELAY1, LOW);
    digitalWrite(RELAY2, LOW);
    digitalWrite(RELAY3, LOW);
    digitalWrite(RELAY4, LOW);

    // ---------- RESET AUDIO ----------

    if (distance <= 10)
    {
      if (resetDetect == 0)
        resetDetect = millis();

      // Visitor detected for 2 seconds while audio is playing
      if (millis() - resetDetect >= 2000)
      {
        Serial.println("Resetting Audio");

        // Stop audio
        player.stop();

        delay(300);

        playing = false;

        // Keep LEDs ON
        digitalWrite(RELAY1, LOW);
        digitalWrite(RELAY2, LOW);
        digitalWrite(RELAY3, LOW);
        digitalWrite(RELAY4, LOW);

        // Start cooldown
        cooldown = true;
        cooldownStart = millis();

        resetDetect = 0;

        Serial.println("Cooldown Started");
      }
    }
    else
    {
      resetDetect = 0;
    }

    // ---------- AUDIO FINISHED ----------

    if (playing)
    {
      // BUSY = LOW while playing
      // BUSY = HIGH after audio finishes

      if (digitalRead(BUSY_PIN) == HIGH)
      {
        delay(200);

        if (digitalRead(BUSY_PIN) == HIGH)
        {
          Serial.println("Audio Finished");

          playing = false;

          // Keep LEDs ON
          digitalWrite(RELAY1, LOW);
          digitalWrite(RELAY2, LOW);
          digitalWrite(RELAY3, LOW);
          digitalWrite(RELAY4, LOW);

          cooldown = true;
          cooldownStart = millis();

          Serial.println("Cooldown Started");
        }
      }
    }
  }

  delay(20);

}   // End loop