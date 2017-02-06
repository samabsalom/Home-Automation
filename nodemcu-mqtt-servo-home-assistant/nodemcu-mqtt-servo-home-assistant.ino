// based on https://github.com/mertenats/Open-Home-Automation
//
// switch
//   - platform: mqtt
//     name: Blinds
//     state_topic: 'blinds/1/status'
//     command_topic: 'blinds/8/switch'
//     optimistic: false
// with 3v3 ground and d1 connected to a servo this should allow the servo to rotate through two values
//  i have used 0 and 90 and intend to use this to control some ventian blind sin my bedroom with home assistant



#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Servo.h>

#define MQTT_VERSION MQTT_VERSION_3_1_1

Servo servo;

const char* WIFI_SSID =  "changeme";   // cannot be longer than 32 characters!
const char* WIFI_PASSWORD =  "changeme";   //

const PROGMEM char* MQTT_CLIENT_ID = "servo";
const PROGMEM char* MQTT_SERVER_IP = "changeme";
const PROGMEM uint16_t MQTT_SERVER_PORT = 1883;
const PROGMEM char* MQTT_USER = "changeme";
const PROGMEM char* MQTT_PASSWORD = "changeme";

const char* MQTT_SERVO_STATE_TOPIC1 = "changeme/1/status";
const char* MQTT_SERVO_COMMAND_TOPIC1 = "changeme/1/switch";


#define BUFFER_SIZE 100

// bool LedState = false;
const char* SERVO_ON = "ON";
const char* SERVO_OFF = "OFF";
#define SERVO_ON_VALUE -10
#define SERVO_OFF_VALUE 90

const PROGMEM uint8_t SERVO_PIN1 = 5; //d1

boolean m_servo_state1 = false; // servo is turned off by default

WiFiClient wifiClient;
PubSubClient client(wifiClient);

// function called to publish the state of the light (on/off)
void publishServoState1() {
  if (m_servo_state1) {
    client.publish(MQTT_SERVO_STATE_TOPIC1, SERVO_ON, true);
  } else {
    client.publish(MQTT_SERVO_STATE_TOPIC1, SERVO_OFF, true);
  }
}

// function called to turn on/off the light
void setServoState1() {
  if (m_servo_state1) {
    servo.write(SERVO_OFF_VALUE);
    Serial.println("INFO: Turn servo on...");
  } else {
    servo.write(SERVO_ON_VALUE);
    Serial.println("INFO: Turn servo off...");
  }
}

// function called when a MQTT message arrived
void callback(char* p_topic, byte* p_payload, unsigned int p_length) {
  // concat the payload into a string
  String payload;
  for (uint8_t i = 0; i < p_length; i++) {
    payload.concat((char)p_payload[i]);
  }

  // handle message topic
  if (String(MQTT_SERVO_COMMAND_TOPIC1).equals(p_topic)) {
    // test if the payload is equal to "ON" or "OFF"
    if (payload.equals(String(SERVO_ON))) {
      if (m_servo_state1 != true) {
        m_servo_state1 = true;
        setServoState1();
        publishServoState1();
      }
    } else if (payload.equals(String(SERVO_OFF))) {
      if (m_servo_state1 != false) {
        m_servo_state1 = false;
        setServoState1();
        publishServoState1();
      }
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("INFO: Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("INFO: connected");
      // Once connected, publish an announcement...
      publishServoState1();

      // ... and resubscribe
      client.subscribe(MQTT_SERVO_COMMAND_TOPIC1);

    } else {
      Serial.print("ERROR: failed, rc=");
      Serial.print(client.state());
      Serial.println("DEBUG: try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


void setup() {
  // init the serial
  Serial.begin(115200);

  // init the servo
  servo.attach(SERVO_PIN1);
  setServoState1();
//  servo.write(-10);

  // init the WiFi connection
  Serial.println();
  Serial.println();
  Serial.print("INFO: Connecting to ");
  WiFi.mode(WIFI_STA);
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("INFO: WiFi connected");
  Serial.print("INFO: IP address: ");
  Serial.println(WiFi.localIP());

  // init the MQTT connection
  client.setServer(MQTT_SERVER_IP, MQTT_SERVER_PORT);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
