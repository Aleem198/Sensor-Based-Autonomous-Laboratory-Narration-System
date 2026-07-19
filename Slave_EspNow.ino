/*
=========================================================
     SMART AUDIO INFORMATION DISPLAY SYSTEM
     ESP1 - AUDIO STATION 1
=========================================================

ESP1 Hardware:
--------------

HC-SR04:
TRIG -> GPIO13
ECHO -> GPIO12

DFPlayer:
TX   -> GPIO16 (ESP RX2)
RX   -> GPIO17 (ESP TX2)
BUSY -> GPIO4

4-Channel Relay:
Relay1 -> GPIO5
Relay2 -> GPIO18
Relay3 -> GPIO19
Relay4 -> GPIO20

ESP-NOW:
ESP1 sends play request to ESP4 Master.

FUNCTION:
---------
• Detect visitor within 10 cm for 1 second
• Request permission from ESP4
• If permission granted -> Relays ON + Audio
• Only one ESP can play at a time
• Detect again for 2 sec during audio to reset
• Relays remain ON during audio
• Relays remain ON for 5 sec after finish/reset
• Send AUDIO_DONE to ESP4 after cooldown
• Ready for next visitor

=========================================================
*/

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>


// =====================================================
// ESP CONFIGURATION
// =====================================================

#define MY_ESP_ID 3

#define ESPNOW_CHANNEL 1


// =====================================================
// ESP4 MASTER MAC ADDRESS
// =====================================================

uint8_t MASTER_MAC[] = {
  0x68, 0x09, 0x47, 0x28, 0xF8, 0x04
};


// =====================================================
// MESSAGE TYPES
// MUST MATCH ESP4 MASTER
// =====================================================

enum MessageType {

  REQUEST_PLAY = 1,

  GRANT_PLAY = 2,

  DENY_PLAY = 3,

  AUDIO_DONE = 4

};


// =====================================================
// MESSAGE STRUCTURE
// MUST MATCH ESP4 MASTER
// =====================================================

typedef struct {

  uint8_t type;

  uint8_t espID;

} Message;


Message outgoingMessage;


// =====================================================
// ESP-NOW FLAGS
// =====================================================

volatile bool permissionGranted = false;

volatile bool permissionDenied = false;


// =====================================================
// PIN DEFINITIONS
// =====================================================

// Ultrasonic

#define TRIG_PIN 13

#define ECHO_PIN 12


// DFPlayer BUSY

#define BUSY_PIN 4


// 4-Channel Relay

#define RELAY1 5

#define RELAY2 18

#define RELAY3 19

#define RELAY4 20


// =====================================================
// DFPLAYER
// =====================================================

HardwareSerial mp3(2);

DFRobotDFPlayerMini player;


// =====================================================
// SYSTEM VARIABLES
// =====================================================

bool playing = false;

bool cooldown = false;

bool waitingPermission = false;


// =====================================================
// TIMERS
// =====================================================

unsigned long startDetect = 0;

unsigned long resetDetect = 0;

unsigned long cooldownStart = 0;

unsigned long permissionStart = 0;


// Master response timeout

const unsigned long PERMISSION_TIMEOUT = 5000;


// =====================================================
// ULTRASONIC
// SAME LOGIC AS YOUR WORKING ESP4
// =====================================================

float getDistance()
{

  digitalWrite(
    TRIG_PIN,
    LOW
  );

  delayMicroseconds(2);


  digitalWrite(
    TRIG_PIN,
    HIGH
  );

  delayMicroseconds(10);


  digitalWrite(
    TRIG_PIN,
    LOW
  );


  long duration = pulseIn(
    ECHO_PIN,
    HIGH,
    30000
  );


  if (duration == 0)
  {

    return 999;

  }


  return duration * 0.0343 / 2.0;

}


// =====================================================
// ALL RELAYS ON
// Active LOW
// =====================================================

void relaysON()
{

  digitalWrite(
    RELAY1,
    LOW
  );


  digitalWrite(
    RELAY2,
    LOW
  );


  digitalWrite(
    RELAY3,
    LOW
  );


  digitalWrite(
    RELAY4,
    LOW
  );

}


// =====================================================
// ALL RELAYS OFF
// Active LOW
// =====================================================

void relaysOFF()
{

  digitalWrite(
    RELAY1,
    HIGH
  );


  digitalWrite(
    RELAY2,
    HIGH
  );


  digitalWrite(
    RELAY3,
    HIGH
  );


  digitalWrite(
    RELAY4,
    HIGH
  );

}


// =====================================================
// SEND MESSAGE TO ESP4 MASTER
// =====================================================

void sendToMaster(
  uint8_t messageType
)
{

  outgoingMessage.type =
    messageType;


  outgoingMessage.espID =
    MY_ESP_ID;


  esp_err_t result =
    esp_now_send(

      MASTER_MAC,

      (uint8_t *)&outgoingMessage,

      sizeof(outgoingMessage)

    );


  if (result == ESP_OK)
  {

    Serial.println(
      "Message Queued To ESP4 Master"
    );

  }

  else
  {

    Serial.print(
      "ESP-NOW Send Error: "
    );

    Serial.println(
      result
    );

  }

}


// =====================================================
// RECEIVE MESSAGE FROM ESP4
// =====================================================

void onDataRecv(
  const esp_now_recv_info_t *info,
  const uint8_t *data,
  int len
)
{

  if (len != sizeof(Message))
  {

    return;

  }


  Message received;


  memcpy(
    &received,
    data,
    sizeof(received)
  );


  // Ignore messages
  // not meant for ESP1

  if (
    received.espID
    != MY_ESP_ID
  )
  {

    return;

  }


  // ===================================================
  // PERMISSION GRANTED
  // ===================================================

  if (
    received.type
    == GRANT_PLAY
  )
  {

    permissionGranted =
      true;


    permissionDenied =
      false;


    Serial.println();

    Serial.println(
      "MASTER GRANTED PERMISSION"
    );

  }


  // ===================================================
  // PERMISSION DENIED
  // ===================================================

  else if (
    received.type
    == DENY_PLAY
  )
  {

    permissionDenied =
      true;


    permissionGranted =
      false;


    Serial.println();

    Serial.println(
      "MASTER DENIED PERMISSION"
    );

  }

}


// =====================================================
// ADD ESP4 MASTER AS PEER
// =====================================================

void addMasterPeer()
{

  esp_now_peer_info_t peerInfo = {};


  memcpy(
    peerInfo.peer_addr,
    MASTER_MAC,
    6
  );


  peerInfo.channel =
    ESPNOW_CHANNEL;


  peerInfo.encrypt =
    false;


  esp_err_t result =
    esp_now_add_peer(
      &peerInfo
    );


  if (
    result == ESP_OK
    ||
    result == ESP_ERR_ESPNOW_EXIST
  )
  {

    Serial.println(
      "ESP4 Master Peer Ready"
    );

  }

  else
  {

    Serial.print(
      "Master Peer Error: "
    );

    Serial.println(
      result
    );

  }

}


// =====================================================
// SETUP
// =====================================================

void setup()
{

  Serial.begin(
    115200
  );


  // ===================================================
  // ULTRASONIC
  // ===================================================

  pinMode(
    TRIG_PIN,
    OUTPUT
  );


  pinMode(
    ECHO_PIN,
    INPUT
  );


  // ===================================================
  // RELAYS
  // ===================================================

  pinMode(
    RELAY1,
    OUTPUT
  );


  pinMode(
    RELAY2,
    OUTPUT
  );


  pinMode(
    RELAY3,
    OUTPUT
  );


  pinMode(
    RELAY4,
    OUTPUT
  );


  // Relay OFF initially

  relaysOFF();


  // ===================================================
  // DFPLAYER BUSY
  // ===================================================

  pinMode(
    BUSY_PIN,
    INPUT_PULLUP
  );


  // ===================================================
  // DFPLAYER
  // ===================================================

  mp3.begin(
    9600,
    SERIAL_8N1,
    16,
    17
  );


  delay(
    2000
  );


  if (
    !player.begin(
      mp3
    )
  )
  {

    Serial.println(
      "DFPlayer NOT FOUND"
    );


    while (1);

  }


  Serial.println(
    "DFPlayer Ready"
  );


  player.volume(
    30
  );


  // ===================================================
  // WIFI
  // ===================================================

  WiFi.mode(
    WIFI_STA
  );


  WiFi.disconnect();


  delay(
    300
  );


  // Force same WiFi channel
  // as ESP4 Master

  esp_wifi_set_channel(
    ESPNOW_CHANNEL,
    WIFI_SECOND_CHAN_NONE
  );


  Serial.println();

  Serial.println(
    "================================"
  );


  Serial.println(
    "ESP1 AUDIO STATION"
  );


  Serial.print(
    "ESP1 MAC: "
  );


  Serial.println(
    WiFi.macAddress()
  );


  Serial.print(
    "ESP-NOW CHANNEL: "
  );


  Serial.println(
    ESPNOW_CHANNEL
  );


  Serial.println(
    "================================"
  );


  // ===================================================
  // ESP-NOW INITIALIZATION
  // ===================================================

  if (
    esp_now_init()
    != ESP_OK
  )
  {

    Serial.println(
      "ESP-NOW INIT FAILED"
    );


    return;

  }


  // Register receive callback

  esp_now_register_recv_cb(
    onDataRecv
  );


  // Add ESP4 Master

  addMasterPeer();


  Serial.println();

  Serial.println(
    "ESP1 READY"
  );


  Serial.println(
    "Waiting For Visitor..."
  );

}


// =====================================================
// MAIN LOOP
// =====================================================

void loop()
{

  float distance =
    getDistance();


  Serial.print(
    "Distance : "
  );


  Serial.println(
    distance
  );


  // ===================================================
  // COOLDOWN
  // ===================================================

  if (
    cooldown
  )
  {

    // Keep all relays ON

    relaysON();


    // Wait 5 seconds

    if (
      millis()
      - cooldownStart
      >= 5000
    )
    {

      Serial.println();

      Serial.println(
        "ESP1 Cooldown Finished"
      );


      // Turn relays OFF

      relaysOFF();


      cooldown =
        false;


      // =================================================
      // TELL ESP4 MASTER WE ARE DONE
      // =================================================

      sendToMaster(
        AUDIO_DONE
      );


      Serial.println(
        "AUDIO_DONE Sent To ESP4"
      );


      Serial.println(
        "Waiting For Visitor..."
      );

    }


    delay(
      20
    );


    return;

  }


  // ===================================================
  // WAITING FOR MASTER PERMISSION
  // ===================================================

  if (
    waitingPermission
  )
  {


    // =================================================
    // PERMISSION GRANTED
    // =================================================

    if (
      permissionGranted
    )
    {

      permissionGranted =
        false;


      waitingPermission =
        false;


      Serial.println();

      Serial.println(
        "STARTING ESP1 AUDIO"
      );


      // ===============================================
      // RELAYS ON
      // ===============================================

      relaysON();


      delay(
        200
      );


      // ===============================================
      // PLAY AUDIO
      // ===============================================

      player.playMp3Folder(
        1
      );


      delay(
        500
      );


      // ===============================================
      // WAIT FOR BUSY LOW
      // Same logic as your working ESP4
      // ===============================================

      unsigned long busyWaitStart =
        millis();


      while (
        digitalRead(
          BUSY_PIN
        )
        == HIGH
      )
      {

        delay(
          10
        );


        // Safety timeout

        if (
          millis()
          - busyWaitStart
          > 5000
        )
        {

          Serial.println(
            "DFPlayer BUSY Timeout"
          );


          break;

        }

      }


      Serial.println(
        "ESP1 Audio Started"
      );


      playing =
        true;


      resetDetect =
        0;

    }


    // =================================================
    // PERMISSION DENIED
    // =================================================

    else if (
      permissionDenied
    )
    {

      permissionDenied =
        false;


      waitingPermission =
        false;


      startDetect =
        0;


      Serial.println();

      Serial.println(
        "ESP4 MASTER BUSY"
      );


      Serial.println(
        "ESP1 WILL NOT PLAY"
      );


      Serial.println(
        "Waiting For Visitor..."
      );

    }


    // =================================================
    // MASTER RESPONSE TIMEOUT
    // =================================================

    else if (
      millis()
      - permissionStart
      >= PERMISSION_TIMEOUT
    )
    {

      waitingPermission =
        false;


      startDetect =
        0;


      Serial.println();

      Serial.println(
        "ESP4 MASTER RESPONSE TIMEOUT"
      );


      Serial.println(
        "Waiting For Visitor..."
      );

    }


    delay(
      20
    );


    return;

  }


  // ===================================================
  // WAIT FOR FIRST DETECTION
  // ===================================================

  if (
    !playing
  )
  {

    // Visitor within 10cm

    if (
      distance <= 10
    )
    {

      if (
        startDetect == 0
      )
      {

        startDetect =
          millis();

      }


      // Visitor must stay for 1 second

      if (
        millis()
        - startDetect
        >= 1000
      )
      {

        Serial.println();

        Serial.println(
          "ESP1 Visitor Confirmed"
        );


        // Reset permission flags

        permissionGranted =
          false;


        permissionDenied =
          false;


        // ===============================================
        // ASK ESP4 MASTER FOR PERMISSION
        // ===============================================

        sendToMaster(
          REQUEST_PLAY
        );


        waitingPermission =
          true;


        permissionStart =
          millis();


        startDetect =
          0;


        Serial.println(
          "Waiting For ESP4 Permission..."
        );

      }

    }

    else
    {

      startDetect =
        0;

    }

  }


  // ===================================================
  // AUDIO PLAYING
  // ===================================================

  if (
    playing
  )
  {

    // Keep relays ON

    relaysON();


    // =================================================
    // RESET AUDIO
    // =================================================

    if (
      distance <= 10
    )
    {

      if (
        resetDetect == 0
      )
      {

        resetDetect =
          millis();

      }


      // Visitor detected continuously
      // for 2 seconds during playback

      if (
        millis()
        - resetDetect
        >= 2000
      )
      {

        Serial.println();

        Serial.println(
          "Resetting ESP1 Audio"
        );


        // Stop audio

        player.stop();


        delay(
          300
        );


        playing =
          false;


        // Keep relays ON

        relaysON();


        // Start 5-second cooldown

        cooldown =
          true;


        cooldownStart =
          millis();


        resetDetect =
          0;


        Serial.println(
          "ESP1 Cooldown Started"
        );

      }

    }

    else
    {

      resetDetect =
        0;

    }


    // =================================================
    // AUDIO FINISHED NORMALLY
    // =================================================

    if (
      playing
    )
    {

      // BUSY LOW = Playing
      // BUSY HIGH = Finished

      if (
        digitalRead(
          BUSY_PIN
        )
        == HIGH
      )
      {

        delay(
          200
        );


        if (
          digitalRead(
            BUSY_PIN
          )
          == HIGH
        )
        {

          Serial.println();

          Serial.println(
            "ESP1 Audio Finished"
          );


          playing =
            false;


          // Keep relays ON

          relaysON();


          // Start cooldown

          cooldown =
            true;


          cooldownStart =
            millis();


          Serial.println(
            "ESP1 Cooldown Started"
          );

        }

      }

    }

  }


  delay(
    20
  );

}