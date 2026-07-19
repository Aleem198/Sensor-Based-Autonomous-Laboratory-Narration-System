#include <WiFi.h>
#include <esp_now.h>
#include "DFRobotDFPlayerMini.h"


// =====================================================
// ESP IDs
// =====================================================

#define ESP1_ID 1
#define ESP2_ID 2
#define ESP3_ID 3
#define ESP4_ID 4


// =====================================================
// MAC ADDRESSES
// =====================================================

// ESP1
uint8_t ESP1_MAC[] = {
  0x70, 0x4B, 0xCA, 0x8E, 0xF3, 0x5C
};

// ESP2
uint8_t ESP2_MAC[] = {
  0x68, 0x09, 0x47, 0x57, 0xF9, 0x8C
};

// ESP3
uint8_t ESP3_MAC[] = {
  0x1C, 0xC3, 0xAB, 0x3C, 0xA0, 0x5C
};


// =====================================================
// MESSAGE TYPES
// =====================================================

#define REQUEST_PLAY 1
#define GRANT_PLAY   2
#define DENY_PLAY    3
#define AUDIO_DONE   4


typedef struct {
  int type;
  int espID;
} Message;


Message incomingMessage;
Message outgoingMessage;


// =====================================================
// MASTER VARIABLE
// =====================================================

// 0 = Nobody playing
// 1 = ESP1 playing
// 2 = ESP2 playing
// 3 = ESP3 playing
// 4 = ESP4 playing

volatile int activeESP = 0;


// =====================================================
// ESP4 PINS
// =====================================================

const int sensorPin = 13;

const int ledPin = 14;

const int busyPin = 4;


// =====================================================
// DFPLAYER
// =====================================================

HardwareSerial mp3Serial(2);

DFRobotDFPlayerMini player;


// =====================================================
// ESP4 VARIABLES
// =====================================================

bool detectionStarted = false;

unsigned long detectionStartTime = 0;

unsigned long audioStartTime = 0;

unsigned long cooldownStartTime = 0;


// =====================================================
// SETTINGS
// =====================================================

const unsigned long detectionTime =
  1000;

const unsigned long cooldownTime =
  5000;

const unsigned long busyIgnoreTime =
  1000;


// =====================================================
// ESP4 PLAYBACK STATE
// =====================================================

enum ESP4State {

  ESP4_IDLE,

  ESP4_PLAYING,

  ESP4_COOLDOWN

};


ESP4State esp4State =
  ESP4_IDLE;


// =====================================================
// SEND MESSAGE
// =====================================================

void sendMessage(
  uint8_t *mac,
  int messageType,
  int targetID
) {

  outgoingMessage.type =
    messageType;

  outgoingMessage.espID =
    targetID;


  esp_now_send(
    mac,
    (uint8_t *)&outgoingMessage,
    sizeof(outgoingMessage)
  );

}


// =====================================================
// SEND MESSAGE TO CORRECT ESP
// =====================================================

void sendToESP(
  int espID,
  int messageType
) {


  if (espID == ESP1_ID) {

    sendMessage(
      ESP1_MAC,
      messageType,
      ESP1_ID
    );

  }


  else if (espID == ESP2_ID) {

    sendMessage(
      ESP2_MAC,
      messageType,
      ESP2_ID
    );

  }


  else if (espID == ESP3_ID) {

    sendMessage(
      ESP3_MAC,
      messageType,
      ESP3_ID
    );

  }

}


// =====================================================
// RECEIVE MESSAGE FROM ESP1 / ESP2 / ESP3
// =====================================================

void onDataRecv(
  const esp_now_recv_info_t *info,
  const uint8_t *data,
  int len
) {


  if (len != sizeof(Message)) {

    return;

  }


  memcpy(
    &incomingMessage,
    data,
    sizeof(incomingMessage)
  );


  int senderID =
    incomingMessage.espID;


  // =================================================
  // PLAY REQUEST
  // =================================================

  if (
    incomingMessage.type
    == REQUEST_PLAY
  ) {


    Serial.println();

    Serial.print(
      "Play request from ESP"
    );

    Serial.println(
      senderID
    );


    // ===============================================
    // SYSTEM FREE
    // ===============================================

    if (activeESP == 0) {


      // Lock system immediately

      activeESP =
        senderID;


      Serial.print(
        "System locked by ESP"
      );

      Serial.println(
        senderID
      );


      // Give permission

      sendToESP(
        senderID,
        GRANT_PLAY
      );


      Serial.print(
        "Permission granted to ESP"
      );

      Serial.println(
        senderID
      );

    }


    // ===============================================
    // SYSTEM BUSY
    // ===============================================

    else {


      Serial.print(
        "Request denied. ESP"
      );

      Serial.print(
        activeESP
      );

      Serial.println(
        " is active."
      );


      sendToESP(
        senderID,
        DENY_PLAY
      );

    }

  }


  // =================================================
  // AUDIO FINISHED + COOLDOWN FINISHED
  // =================================================

  else if (
    incomingMessage.type
    == AUDIO_DONE
  ) {


    // Only active ESP
    // can unlock system

    if (
      senderID
      == activeESP
    ) {


      Serial.print(
        "ESP"
      );

      Serial.print(
        senderID
      );

      Serial.println(
        " completed audio and cooldown."
      );


      // Unlock system

      activeESP =
        0;


      Serial.println(
        "SYSTEM IS NOW FREE"
      );

    }

  }

}


// =====================================================
// ADD CLIENT PEER
// =====================================================

void addPeer(
  uint8_t *mac
) {


  esp_now_peer_info_t peerInfo = {};


  memcpy(
    peerInfo.peer_addr,
    mac,
    6
  );


  peerInfo.channel =
    0;


  peerInfo.encrypt =
    false;


  if (
    esp_now_add_peer(
      &peerInfo
    )
    == ESP_OK
  ) {


    Serial.println(
      "Peer added"
    );

  }


  else {


    Serial.println(
      "Failed to add peer"
    );

  }

}


// =====================================================
// SETUP
// =====================================================

void setup() {


  Serial.begin(
    115200
  );


  delay(
    1000
  );


  // =================================================
  // ESP4 SENSOR
  // =================================================

  pinMode(
    sensorPin,
    INPUT
  );


  // =================================================
  // ESP4 LED / RELAY
  // =================================================

  pinMode(
    ledPin,
    OUTPUT
  );


  digitalWrite(
    ledPin,
    LOW
  );


  // =================================================
  // DFPLAYER BUSY
  // =================================================

  pinMode(
    busyPin,
    INPUT
  );


  // =================================================
  // DFPLAYER
  // =================================================

  mp3Serial.begin(
    9600,
    SERIAL_8N1,
    16,
    17
  );


  if (
    !player.begin(
      mp3Serial
    )
  ) {


    Serial.println(
      "DFPlayer connection failed"
    );

  }


  else {


    Serial.println(
      "DFPlayer connected"
    );


    player.volume(
      25
    );

  }


  // =================================================
  // WIFI
  // =================================================

  WiFi.mode(
    WIFI_STA
  );


  delay(
    500
  );


  Serial.println();

  Serial.println(
    "============================"
  );


  Serial.println(
    "ESP4 MASTER + AUDIO UNIT"
  );


  Serial.print(
    "ESP4 MAC: "
  );


  Serial.println(
    WiFi.macAddress()
  );


  Serial.println(
    "============================"
  );


  // =================================================
  // ESP-NOW
  // =================================================

  if (
    esp_now_init()
    != ESP_OK
  ) {


    Serial.println(
      "ESP-NOW initialization failed"
    );


    return;

  }


  esp_now_register_recv_cb(
    onDataRecv
  );


  // Add ESP1

  addPeer(
    ESP1_MAC
  );


  // Add ESP2

  addPeer(
    ESP2_MAC
  );


  // Add ESP3

  addPeer(
    ESP3_MAC
  );


  Serial.println();

  Serial.println(
    "MASTER READY"
  );


  Serial.println(
    "System FREE"
  );

}


// =====================================================
// LOOP
// =====================================================

void loop() {


  // =================================================
  // ESP4 IDLE
  // =================================================

  if (
    esp4State
    == ESP4_IDLE
  ) {


    // IMPORTANT:
    // ESP4 only checks its sensor
    // when nobody else is playing


    if (
      activeESP
      == 0
    ) {


      int detected =
        digitalRead(
          sensorPin
        );


      // =============================================
      // PERSON DETECTED
      // =============================================

      if (
        detected
        == HIGH
      ) {


        if (
          !detectionStarted
        ) {


          detectionStarted =
            true;


          detectionStartTime =
            millis();

        }


        // Detection continuously
        // for 1 second

        if (
          millis()
          - detectionStartTime
          >= detectionTime
        ) {


          Serial.println();

          Serial.println(
            "ESP4 PERSON DETECTED"
          );


          // Lock system

          activeESP =
            ESP4_ID;


          Serial.println(
            "ESP4 LOCKED SYSTEM"
          );


          // Turn LED / Relay ON

          digitalWrite(
            ledPin,
            HIGH
          );


          // Play ESP4 audio

          player.play(
            1
          );


          audioStartTime =
            millis();


          esp4State =
            ESP4_PLAYING;


          detectionStarted =
            false;


          Serial.println(
            "ESP4 AUDIO STARTED"
          );

        }

      }


      else {


        detectionStarted =
          false;

      }

    }


    // Someone else is playing

    else {


      // Ignore ESP4 sensor

      detectionStarted =
        false;

    }

  }


  // =================================================
  // ESP4 AUDIO PLAYING
  // =================================================

  else if (
    esp4State
    == ESP4_PLAYING
  ) {


    // Ignore BUSY pin initially

    if (
      millis()
      - audioStartTime
      > busyIgnoreTime
    ) {


      // LOW = Playing
      // HIGH = Finished

      if (
        digitalRead(
          busyPin
        )
        == HIGH
      ) {


        Serial.println();

        Serial.println(
          "ESP4 AUDIO FINISHED"
        );


        // Start cooldown

        cooldownStartTime =
          millis();


        esp4State =
          ESP4_COOLDOWN;


        Serial.println(
          "ESP4 5 SECOND COOLDOWN"
        );

      }

    }

  }


  // =================================================
  // ESP4 COOLDOWN
  // =================================================

  else if (
    esp4State
    == ESP4_COOLDOWN
  ) {


    if (
      millis()
      - cooldownStartTime
      >= cooldownTime
    ) {


      // Turn LED / Relay OFF

      digitalWrite(
        ledPin,
        LOW
      );


      // Unlock entire system

      activeESP =
        0;


      esp4State =
        ESP4_IDLE;


      detectionStarted =
        false;


      Serial.println();

      Serial.println(
        "ESP4 COOLDOWN FINISHED"
      );


      Serial.println(
        "SYSTEM IS NOW FREE"
      );

    }

  }

}