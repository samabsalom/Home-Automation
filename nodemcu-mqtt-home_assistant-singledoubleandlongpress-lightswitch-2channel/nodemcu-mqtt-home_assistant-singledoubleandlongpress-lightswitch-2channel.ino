/*
   Based again on samuel mertenats original code
   This has been adapted to allow a 2 channel relay to be controlled by both home assistant and two buttons on the wall.

   The idea here is to recreate a light switch on the wall that can also be controlled by Home-Assistant.
   but a double press will control another topic by mqtt on home assistant and long presses will
   send an all on or off command that an automation will intepret

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


   Configuration (HA) :
switch:
  platform: mqtt
  name: 'Office Switch'
  state_topic: 'changeme/1/status'
  command_topic: 'changeme/1/set'
  retain: true
  optimistic: false

sensor:
  - platform: mqtt
    state_topic: 'changeme/allonoff/switch'
    name: allonoff

script:
  loungeon:
    alias: Lounge on
    sequence:
      - service: homeassistant.turn_on
        data:
          entity_id: scene.loungeon
      - service: mqtt.publish
        data:
          payload: waiting
          topic: loungelight/allonoff/switch
          qos: 0
          retain: true
  loungeoff:
    alias: Lounge off
    sequence:
      - service: homeassistant.turn_on
        data:
          entity_id: scene.loungeoff
      - service: mqtt.publish
        data:
          payload: waiting
          topic: loungelight/allonoff/switch
          qos: 0
          retain: true

automation
  - alias: all on
    trigger:
      platform: state
      entity_id: sensor.allonoff
      to: "ON"
    action:
      service: homeassistant.turn_on
      entity_id: script.loungeon
  - alias: all off
    trigger:
      platform: state
      entity_id: sensor.allonoff
      to: "OFF"
    action:
      service: homeassistant.turn_on
      entity_id: script.loungeoff


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
const PROGMEM char* MQTT_PASSWORD = "raspberry";

#define OTA_UPDATE //Uncomment if using OTA update
#define OTApassword "lounge" // change this to whatever password you want to use when you upload OTA
#define OTAname "lounge-lightswitch" //MQTT device for broker and OTA name
int OTAport = 8266;

// MQTT: topics
const PROGMEM char* MQTT_SWITCH_STATUS_TOPIC1 = "changeme/1/status"; //these are the topics for the relays attached to the board
const PROGMEM char* MQTT_SWITCH_COMMAND_TOPIC1 = "changeme/1/switch"; //
const PROGMEM char* MQTT_SWITCH_STATUS_TOPIC2 = "changeme/2/status"; //
const PROGMEM char* MQTT_SWITCH_COMMAND_TOPIC2 = "changeme/2/switch"; //
const PROGMEM char* MQTT_ALL_ONOFF_TOPIC = "changeme/allonoff/switch"; //this is the topic that the automation will interpret to everything on and off
const PROGMEM char* MQTT_LAMP_TOPIC = "changeme/3/switch";  //these are the topics that you want to control that run elesewhere
const PROGMEM char* MQTT_TANK_TOPIC = "changeme/4/switch"; //         they are named lamp and tank as that is what i use them for. fish tank and a lamp

// default payload
const PROGMEM char* SWITCH_ON = "ON";
const PROGMEM char* SWITCH_OFF = "OFF";

// store the state of the switch
boolean m_switch_state1 = false;
boolean m_switch_state2 = false;
boolean m_tank_state = false;
boolean m_lamp_state = false;
// D1/GPIO5
const PROGMEM uint8_t BUTTON_PIN1 = 0;
const PROGMEM uint8_t BUTTON_PIN2 = 2;
const PROGMEM uint8_t RELAY_PIN1= 5;
const PROGMEM uint8_t RELAY_PIN2= 4;

#ifdef OTA_UPDATE
  #include <ESP8266mDNS.h>
  #include <WiFiUdp.h>
  #include <ArduinoOTA.h>
#endif


WiFiClient wifiClient;
PubSubClient client(wifiClient);
OneButton button1(BUTTON_PIN1, true); // false : active HIGH
OneButton button2(BUTTON_PIN2, true); // true : active LOW


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

void doubleclick1() {
  if (m_tank_state) {
    m_tank_state = false;
  } else {
    m_tank_state = true;
  }
  publishTankState();
  Serial.println("Button 1 doubleclick.");
}

void doubleclick2() {
  if (m_lamp_state) {
    m_lamp_state = false;
  } else {
    m_lamp_state = true;
  }
  publishLampState();
  Serial.println("Button 2 doubleclick.");
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

void publishTankState() {
  if (m_tank_state) {
    client.publish(MQTT_TANK_TOPIC, SWITCH_ON, true);
  } else {
    client.publish(MQTT_TANK_TOPIC, SWITCH_OFF, true);
  }
}

void publishLampState() {
  if (m_lamp_state) {
    client.publish(MQTT_LAMP_TOPIC, SWITCH_ON, true);
  } else {
    client.publish(MQTT_LAMP_TOPIC, SWITCH_OFF, true);
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
  // handle message topic
  if (String(MQTT_TANK_TOPIC).equals(p_topic)) {
    // test if the payload is equal to "ON" or "OFF"
    if (payload.equals(String(SWITCH_ON))) {
      if (m_tank_state != true) {
        m_tank_state = true;
        publishTankState();
      }
    } else if (payload.equals(String(SWITCH_OFF))) {
      if (m_tank_state != false) {
        m_tank_state = false;
        publishTankState();
      }
    }
  }

  // handle message topic
  if (String(MQTT_LAMP_TOPIC).equals(p_topic)) {
    // test if the payload is equal to "ON" or "OFF"
    if (payload.equals(String(SWITCH_ON))) {
      if (m_lamp_state != true) {
        m_lamp_state = true;
        publishLampState();
      }
    } else if (payload.equals(String(SWITCH_OFF))) {
      if (m_lamp_state != false) {
        m_lamp_state = false;
        publishLampState();
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
      client.subscribe(MQTT_TANK_TOPIC);
      client.subscribe(MQTT_LAMP_TOPIC);
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
  button1.attachDoubleClick(doubleclick1);
  button2.attachDoubleClick(doubleclick2);

  // init the led
  pinMode(RELAY_PIN1, OUTPUT);
//  analogWriteRange(255);
  setSwitchState1();
  // init the led
  pinMode(RELAY_PIN2, OUTPUT);
//  analogWriteRange(255);
  setSwitchState2();


  setupWifi();

  #ifdef OTA_UPDATE
    setupOTA();
  #endif
}

void setupWifi();{
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
    Serial.println("INFO: IP address: ");
    Serial.println(WiFi.localIP());

    // init the MQTT connection
    client.setServer(MQTT_SERVER_IP, MQTT_SERVER_PORT);
    client.setCallback(callback);
}
void setupOTA (){
    ArduinoOTA.setPort(OTAport);
    ArduinoOTA.setHostname(OTAname);
    ArduinoOTA.setPassword((const char *)OTApassword);

    ArduinoOTA.onStart([]() {
      Serial.println("Start");
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("\nEnd");
      ESP.restart();
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.begin();
    Serial.print("OTA running");
}


void loop() {
  #ifdef OTA_UPDATE
  ArduinoOTA.handle();
  #endif
  // keep watching the push button:
  button1.tick();
  button2.tick();
  delay(10);

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
