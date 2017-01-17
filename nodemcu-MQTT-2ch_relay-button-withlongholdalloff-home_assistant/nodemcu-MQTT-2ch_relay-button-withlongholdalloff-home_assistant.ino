/*
   Based again on samuel mertenats original code
   This has been adapted to allow a 2 channel relay to be controlled by both home assistant and two buttons on the wall.

   The idea here is to recreate a light switch on the wall that can also be controlled by Home-Assistant

   https://home-assistant.io/components/switch.mqtt/

   Libraries :
    - ESP8266 core for Arduino : https://github.com/esp8266/Arduino
    - PubSubClient : https://github.com/knolleary/pubsubclient
    - OneButton : https://github.com/mathertel/OneButton

   Sources :
    - File > Examples > ES8266WiFi > WiFiClient
    - File > Examples > PubSubClient > mqtt_auth
    - File > Examples > PubSubClient > mqtt_esp8266
    - OneButton : http://www.mathertel.de/Arduino/OneButtonLibrary.aspx

   Schematic :
    - https://github.com/mertenats/open-home-automation/blob/master/ha_mqtt_switch/Schematic.png

   Configuration (HA) :
    switch:
      platform: mqtt
      name: 'Office Switch'
      state_topic: 'changeme/1/status'
      command_topic: 'changeme/1/set'
      retain: true
      optimistic: false

automation:
- alias: new on topic from switch
  trigger:
    platform: state
    entity_id: sensor.allonoff
    from: "OFF"
    to: "ON"
  action:
    service: scene.turn_on
    entity_id: scene.loungeon

- alias: new off topic from switch
  trigger:
    platform: state
    entity_id: sensor.allonoff
    from: "ON"
    to: "OFF"
  action:
    service: scene.turn_on
    entity_id: scene.loungeoff

sensor:
    - platform: mqtt
      state_topic: 'changeme/allonoff/switch'
      name: allonoff


   Samuel M. - v1.1 - 08.2016
   If you like this example, please add a star! Thank you!
   https://github.com/mertenats/open-home-automation
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <OneButton.h>

#define MQTT_VERSION MQTT_VERSION_3_1_1

// Wifi: SSID and password
const char* WIFI_SSID = "changeme";
const char* WIFI_PASSWORD = "changeme";

// MQTT: ID, server IP, port, username and password
const PROGMEM char* MQTT_CLIENT_ID = "changeme";
const PROGMEM char* MQTT_SERVER_IP = "changeme";
const PROGMEM uint16_t MQTT_SERVER_PORT = 1883;
const PROGMEM char* MQTT_USER = "changeme";
const PROGMEM char* MQTT_PASSWORD = "changeme";

// MQTT: topics
const PROGMEM char* MQTT_SWITCH_STATUS_TOPIC1 = "changeme/1/status";
const PROGMEM char* MQTT_SWITCH_COMMAND_TOPIC1 = "changeme/1/switch";
const PROGMEM char* MQTT_SWITCH_STATUS_TOPIC2 = "changeme/2/status";
const PROGMEM char* MQTT_SWITCH_COMMAND_TOPIC2 = "changeme/2/switch";
const PROGMEM char* MQTT_ALL_ONOFF_TOPIC = "changeme/allonoff/switch";
// default payload
const PROGMEM char* SWITCH_ON = "ON";
const PROGMEM char* SWITCH_OFF = "OFF";

// store the state of the switch
boolean m_switch_state1 = false;
boolean m_switch_state2 = false;
// D1/GPIO5
const PROGMEM uint8_t BUTTON_PIN1 = 13;
const PROGMEM uint8_t BUTTON_PIN2 = 15;
const PROGMEM uint8_t RELAY_PIN1= 4;
const PROGMEM uint8_t RELAY_PIN2= 5;


WiFiClient wifiClient;
PubSubClient client(wifiClient);
OneButton button1(BUTTON_PIN1, false); // false : active HIGH
OneButton button2(BUTTON_PIN2, false);


// function called on button1 press
// toggle the state of the switch
void click1() {
  if (m_switch_state1) {
    m_switch_state1 = false;
  } else {
    m_switch_state1 = true;
  }
  publishSwitchState1();
  setSwitchState1();
  Serial.println("Button 1 press");
}
// function called on button press
// toggle the state of the switch
void click2() {
  if (m_switch_state2) {
    m_switch_state2 = false;
  } else {
    m_switch_state2 = true;
  }
  publishSwitchState2();
  setSwitchState2();
  Serial.println("Button 2 press");
}

// This function will be called once, when the button1 is released after beeing pressed for a long time.
void longPressStop1() {
  client.publish(MQTT_ALL_ONOFF_TOPIC, SWITCH_ON, true);
  Serial.println("Button 1 long press");

} // longPressStop1

// This function will be called once, when the button1 is released after beeing pressed for a long time.
void longPressStop2() {
  client.publish(MQTT_ALL_ONOFF_TOPIC, SWITCH_OFF, true);
  Serial.println("Button 2 long press");
} // longPressStop2


// function called to publish the state of the switch (on/off)
void publishSwitchState1() {
  if (m_switch_state1) {
    client.publish(MQTT_SWITCH_STATUS_TOPIC1, SWITCH_ON, true);
  } else {
    client.publish(MQTT_SWITCH_STATUS_TOPIC1, SWITCH_OFF, true);
  }
}

// function called to publish the state of the switch (on/off)
void publishSwitchState2() {
  if (m_switch_state2) {
    client.publish(MQTT_SWITCH_STATUS_TOPIC2, SWITCH_ON, true);
  } else {
    client.publish(MQTT_SWITCH_STATUS_TOPIC2, SWITCH_OFF, true);
  }
}

// function called to turn on/off the relay
void setSwitchState1() {
  if (m_switch_state1) {
    digitalWrite(RELAY_PIN1, HIGH);
    Serial.println("INFO: Turn relay1 on...");
  } else {
    digitalWrite(RELAY_PIN1, LOW);
    Serial.println("INFO: Turn relay1 off...");
  }
}

// function called to turn on/off the relay
void setSwitchState2() {
  if (m_switch_state2) {
    digitalWrite(RELAY_PIN2, HIGH);
    Serial.println("INFO: Turn relay2 on...");
  } else {
    digitalWrite(RELAY_PIN2, LOW);
    Serial.println("INFO: Turn relay2 off...");
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
  if (String(MQTT_SWITCH_COMMAND_TOPIC1).equals(p_topic)) {
    // test if the payload is equal to "ON" or "OFF"
    if (payload.equals(String(SWITCH_ON))) {
      if (m_switch_state1 != true) {
        m_switch_state1 = true;
        publishSwitchState1();
        setSwitchState1();
      }
    } else if (payload.equals(String(SWITCH_OFF))) {
      if (m_switch_state1 != false) {
        m_switch_state1 = false;
        publishSwitchState1();
        setSwitchState1();

      }
    }
  }
  // handle message topic
  if (String(MQTT_SWITCH_COMMAND_TOPIC2).equals(p_topic)) {
    // test if the payload is equal to "ON" or "OFF"
    if (payload.equals(String(SWITCH_ON))) {
      if (m_switch_state2 != true) {
        m_switch_state2 = true;
        publishSwitchState2();
        setSwitchState2();
      }
    } else if (payload.equals(String(SWITCH_OFF))) {
      if (m_switch_state2 != false) {
        m_switch_state2 = false;
        publishSwitchState2();
        setSwitchState2();

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
      // publish the initial values
      publishSwitchState1();
      publishSwitchState2();
      // ... and resubscribe
      client.subscribe(MQTT_SWITCH_COMMAND_TOPIC1);
      client.subscribe(MQTT_SWITCH_COMMAND_TOPIC2);

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

  // link the click function to be called on a single click event.
  button1.attachClick(click1);
  button2.attachClick(click2);
  button1.attachLongPressStop(longPressStop1);
  button2.attachLongPressStop(longPressStop2);

  // init the led
  pinMode(RELAY_PIN1, OUTPUT);
  analogWriteRange(255);
  setSwitchState1();
  // init the led
  pinMode(RELAY_PIN2, OUTPUT);
  analogWriteRange(255);
  setSwitchState2();



  // init the WiFi connection
  Serial.println();
  Serial.println();
  Serial.print("INFO: Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("INFO: WiFi connected");
  Serial.println("INFO: IP address: ");
  Serial.println(WiFi.localIP());

  // init the MQTT connection
  client.setServer(MQTT_SERVER_IP, MQTT_SERVER_PORT);
  client.setCallback(callback);
}

void loop() {
  // keep watching the push button:
  button1.tick();
  button2.tick();
  delay(10);

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
