
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <PubSubClient.h>




const char* mqtt_server = "iot.eclipse.org";

const int PIN_LED = 2; //boot
const int SignalPin = D0; //fire



//MQTT

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;








ESP8266WebServer server(80);
void gasSersor();
void gasSersorNotFound();



// callback

//void callback(char* topic, byte* payload, unsigned int length) {
//  Serial.print("Message arrived [");
//  Serial.print(topic);
//  Serial.print("] ");
//  for (int i = 0; i < length; i++) {
//    Serial.print((char)payload[i]);
//  }
//  Serial.println();
//
//  // Switch on the LED if an 1 was received as first character
//  if ((char)payload[0] == '1') {
//    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
//    // but actually the LED is on; this is because
//    // it is acive low on the ESP-01)
//  } else {
//    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
//  }
//
//}



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
      delay(5000);
    }
  }
}












void setup() {

  pinMode(PIN_LED, OUTPUT);
  pinMode(SignalPin, OUTPUT);
  
  Serial.begin(115200);

  //MQTT
  //  setup_wifi();
  client.setServer(mqtt_server, 1883);
//  client.setCallback(callback);



  Serial.println("\n Starting");
  unsigned long startedAt = millis();
  WiFi.printDiag(Serial); // WiFi password printed



  Serial.println("config portal");

  digitalWrite(PIN_LED, LOW); // LED on in configuration mode.




  WiFiManager wifiManager;

  if (WiFi.SSID() != "") wifiManager.setConfigPortalTimeout(60); //If no access point name has been previously entered disable timeout.

  // starts access point
  //and goes into a blocking loop awaiting configuration



  if (!wifiManager.startConfigPortal()) {//Delete these two parameters if you do not want a WiFi password on your configuration access point
    Serial.println("Not connected to WiFi but continuing anyway.");
  } else {
    Serial.println("WI-FI connected Successfull:)");
  }
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
  Serial.print(waited / 1000);
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

  // MQTT

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
