#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Streaming.h>
#include <Adafruit_NeoPixel.h>
#include <SimpleTimer.h>
#include <WS2812FX.h>

#define ONE_WIRE_BUS D4
#define NEOPIXEL_PIN 14
#define NUM_PIXELS 16

const char* ssid = "Scott";
const char* password = "106shadow";
const char* mqtt_server = "192.168.0.24";
const char* s_stripcolor_mqtt_topic = "/Home/ChrisOffice/setStripColor";
const char* s_pixelcolor_mqtt_topic = "/Home/ChrisOffice/setPixelColor";
const char* s_stripeffect_mqtt_topic = "/Home/ChrisOffice/setStripEffect";
const char* security_arm = "/Home/ChrisOffice/securityarm";
const char* temp_mqtt_topic = "/Home/ChrisOffice/Temperature";
const char* motion_mqtt_topic = "officeMotion";
int arm = 0;

WiFiClient espClient;
PubSubClient client(espClient);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
SimpleTimer timer;
// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
WS2812FX ws2812fx = WS2812FX(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

const byte ledPin = 2;
const byte motionPin = 5;
char temperatureString[6];

void setStripColor(int red, int green, int blue) {
    ws2812fx.setColor(red, green, blue);
}

void MQTTCallBack(char* topic, byte* payload, unsigned int length) {
 //Serial.print("Message arrived [");
 //Serial.print(topic);
 //Serial.print("] ");
 char *m = (char *) payload;
 //Serial.println(m);
 if ((strcmp(topic, s_stripeffect_mqtt_topic) == 0)) {
      if (ws2812fx.isRunning() == false) {
        ws2812fx.start();
      }
      Serial.println(atoi(m));
      ws2812fx.setMode(atoi(m));
 }

 if ((strcmp(topic, s_stripcolor_mqtt_topic) == 0) && (length == 9)) {
   Serial.println(m);
   int red = String(m).substring(1,3).toInt();
   int green = String(m).substring(4,6).toInt();
   int blue = String(m).substring(7,9).toInt();
   //Serial.println("Setting Strip Color to: "+red+green+blue);
   setStripColor(red, green, blue);
   //Serial.println();
 }
//handles up to 999 pixels
 if ((strcmp(topic, s_pixelcolor_mqtt_topic) == 0) && (length == 12)) {
   if(ws2812fx.isRunning() == true) {
     ws2812fx.stop();
     strip.show();
   }
   Serial.println(m);
   int pixel = String(m).substring(1,3).toInt();
   int red = String(m).substring(4,6).toInt();
   int green = String(m).substring(7,9).toInt();
   int blue = String(m).substring(10,12).toInt();
   Serial.println(pixel);
   strip.setPixelColor(pixel, red, green, blue);
   strip.show();
 }

}

void reconnect() {
 // Loop until we're reconnected
 while (!client.connected()) {
 Serial.print("Attempting MQTT connection...");
 // Attempt to connect
 if (client.connect("ESP8266 Client")) {
  Serial.println("connected");
  // ... and subscribe to topic
  client.subscribe(s_stripcolor_mqtt_topic);
  client.subscribe(s_pixelcolor_mqtt_topic);
  client.subscribe(s_stripeffect_mqtt_topic);
  client.subscribe(motion_mqtt_topic);
  client.subscribe(security_arm);
 } else {
  Serial.print("failed, rc=");
  Serial.print(client.state());
  Serial.println(" try again in 5 seconds");
  // Wait 5 seconds before retrying
  delay(5000);
  }
 }
}

void getTemp() {
  //Serial << "Requesting DS18B20 Temperature....." << endl;
  float temp;
  do {
    DS18B20.requestTemperatures();
    temp = DS18B20.getTempFByIndex(0);
  } while (temp == 185 || temp == (-196.6));
    dtostrf(temp, 2, 2, temperatureString);
    //Serial << temperatureString << endl;
}

void publishTemps() {
  getTemp();
  Serial << "Sending Temperature: " << temperatureString << endl;
  client.publish(temp_mqtt_topic, temperatureString);
}
void publishMotion() {
  String motion_status = String(digitalRead(motionPin));
  Serial << "Motion Status: " << motion_status << endl;
  client.publish(motion_mqtt_topic, motion_status.c_str());
}

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
   ArduinoOTA.setHostname("esp8266-1");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
   ArduinoOTA.setPasswordHash("ad2b36d6234a66120dbf04016384d64a");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
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
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  client.setServer(mqtt_server, 1883);
  client.setCallback(MQTTCallBack);
  pinMode(ledPin, OUTPUT);
  pinMode(motionPin, INPUT);
  DS18B20.begin();
  strip.begin();
  strip.show();
  timer.setInterval(10000, publishTemps);
  timer.setInterval(250, publishMotion);
  ws2812fx.init();
  ws2812fx.setBrightness(255);
  ws2812fx.setSpeed(200);
  ws2812fx.setMode(FX_MODE_RAINBOW_CYCLE);
  ws2812fx.start();
}

void loop() {
  ArduinoOTA.handle();
  if(!client.connected()) {
    reconnect();
  }
  client.loop();
  timer.run();
  ws2812fx.service();
}
