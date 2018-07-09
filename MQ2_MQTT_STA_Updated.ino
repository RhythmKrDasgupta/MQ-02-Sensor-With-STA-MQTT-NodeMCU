#include <FS.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>





const int PIN_LED = 2; //boot
const int SignalPin = D0; //fire

ESP8266WebServer server(80);
void gasSersor();
void gasSersorNotFound();






//MQTT

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;



//define your default values here, if there are different values in config.json, they are overwritten.

char mqtt_server[40] = "iot.eclipse.org";
char mqtt_port[6] = "1883";
char username[34] = "";
char password[34] = "";




bool shouldSaveConfig = false;

void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}




// callback

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is acive low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}



//reconnect

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("mavencube/mygassensor/out", msg);
      // ... and resubscribe
      client.subscribe("mavencube/mygassensor/in");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(1000);
    }
  }
}












void setup() {


  Serial.begin(115200);

  pinMode(PIN_LED, OUTPUT);
  pinMode(SignalPin, OUTPUT);


  //read configuration from FS json

  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(username, json["username"]);
          strcpy(password, json["password"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read




  Serial.println("\n Starting");
  unsigned long startedAt = millis();
  WiFi.printDiag(Serial); // WiFi password printed



  Serial.println("config portal");
  digitalWrite(PIN_LED, LOW); // LED on in configuration mode.




  //username

  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 5);
  WiFiManagerParameter custom_username("username", "username", username, 32);
  WiFiManagerParameter custom_password("password", "password", password, 32);






  WiFiManager wifiManager;

  if (WiFi.SSID() != "") wifiManager.setConfigPortalTimeout(30); //If no access point name has been previously entered disable timeout.

  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_username);
  wifiManager.addParameter(&custom_password);




  // starts access point
  //and goes into a blocking loop awaiting configuration



  if (!wifiManager.startConfigPortal("MQ_2_ESP8266")) {//Delete these two parameters if you do not want a WiFi password on your configuration access point
    Serial.println("Not connected to WiFi but continuing anyway.");
  } else {
    Serial.println("WI-FI connected Successfull:)");
  }



  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(username, custom_username.getValue());
  strcpy(password, custom_password.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["username"] = username;
    json["password"] = password;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());

  // mqtt
  client.setServer(mqtt_server, atoi(mqtt_port)); // parseInt to the port
  client.setCallback(callback);









  digitalWrite(PIN_LED, HIGH); // led off ...not in configuration mode.
  pinMode(SignalPin, OUTPUT);
  server.on("/", gasSersor);
  server.onNotFound(gasSersorNotFound);

  server.begin();
  Serial.println("HTTP server started");



  // For some unknown reason webserver can only be started once per boot up
  // so webserver can not be used again in the sketch.

  Serial.print("After waiting ");
  int connRes = WiFi.waitForConnectResult();
  float waited = (millis() - startedAt);
  Serial.print(waited / 5000);
  Serial.print(" secs in setup() connection result is ");
  Serial.println(connRes);
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("failed to connect, finishing setup anyway");
  } else {
    Serial.print("local ip: ");
    Serial.println(WiFi.localIP());
  }


}





void gasSersor() {
  server.send(200, "text/plain", msg);
}



void gasSersorNotFound() {
  server.send(404, "text/plain", "404: Gas Sensor Not found");
}







void loop() {

  server.handleClient();
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;
    value = analogRead(A0);
    snprintf (msg, 75, "sensor value: %ld", value);
    Serial.println(msg);
    client.publish("mavencube/mygassensor/out", msg);
    int sensorThres = 200;

    if ( value > sensorThres) {
      digitalWrite(SignalPin, HIGH);
      Serial.print("lPG Gas found");
      client.publish("mavencube/mygassensor/out", "LPG Gas found");
    }
    else {
      digitalWrite(SignalPin, LOW);
      client.publish("mavencube/mygassensor/out", "LPG Not found");
    }

    int smoke = 100;
    if ( value > smoke) {
      digitalWrite(SignalPin, HIGH);
      Serial.print("Smoke found");
      client.publish("mavencube/mygassensor/out", "Smoke Is found");
    }
    else {
      digitalWrite(SignalPin, LOW);
      //client.publish("mavencube/mygassensor/out", "Smoke is Not found");
    }

  }
}


