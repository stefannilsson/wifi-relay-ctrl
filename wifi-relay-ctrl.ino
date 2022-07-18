#define RELAY_CTRL_VERSION "v0.9.0"

// Pin Definitions
#define BLUE_LED D4 // Blue LED
#define R1 D0       // Relay1
#define R2 D5       // Relay2
#define R3 D6       // Relay3
#define R4 D7       // Relay4
#define R5 D8       // Relay5
#define R6 D3       // Relay6
#define R7 D2       // Relay7
#define R8 D1       // Relay8

// Wifi Settings (included via WifiConfig.h)
// const char *ssid = "...";
// const char *password = "...";

#define MAX_PARALLEL_RELAYS 3
#define NTP_SYNC_INTERVAL 300*1000

// relay statuses
bool relays[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// -1 if not running, otherwise epoch time since started.
int relay_updated[20] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

#include "WifiConfig.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 0, NTP_SYNC_INTERVAL);


#include <ArduinoJson.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
ESP8266WebServer server(80);

void setup() {
  Serial.begin(115200);
  Serial.println(RELAY_CTRL_VERSION);

  initalizeRelayPins();

  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  Serial.println();
  Serial.println("Configuring Wifi...");
  // You can remove the password parameter if you want the AP to be open.
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\n");

  IPAddress myIP = WiFi.localIP();
  Serial.print("My IP address: ");
  Serial.println(myIP);

  Serial.println("Setting up NTP client...");
  timeClient.begin();

  const char * headerkeys[] = {"Accept"} ;
  size_t headerkeyssize = sizeof(headerkeys) / sizeof(char*);
  //ask server to track these headers
  server.collectHeaders(headerkeys, headerkeyssize );

  server.on("/", handleMainHTML);
  server.on("/relays", handleRelayArg);
  server.enableCORS(true);
  
  server.begin(80);

  Serial.println("Server started");
}

void loop() {
  server.handleClient();
}

void initalizeRelayPins() {
  pinMode(R1, OUTPUT);
  pinMode(R2, OUTPUT);
  pinMode(R3, OUTPUT);
  pinMode(R4, OUTPUT);
  pinMode(R5, OUTPUT);
  pinMode(R6, OUTPUT);
  pinMode(R7, OUTPUT);
  pinMode(R8, OUTPUT);

  allRelaysOff();
}

void handleMainHTML() {
  Serial.println("Client connected.");
  digitalWrite(BLUE_LED, HIGH);
  timeClient.update(); // NTP client only polls from server if NTP_SYNC_INTERVAL is due.

  int currentEpoch = timeClient.getEpochTime();
  Serial.printf("Current time : %d\n", currentEpoch);

  server.send(200, "text/html", MainHTML());
  digitalWrite(BLUE_LED, LOW);
}
void handleRelayArg() {
  Serial.println("Client connected.");
  digitalWrite(BLUE_LED, HIGH);
  timeClient.update(); // NTP client only polls from server if NTP_SYNC_INTERVAL is due.

  int currentEpoch = timeClient.getEpochTime();
  Serial.printf("Current time : %d\n", currentEpoch);

  String message = "";

  if (server.args() == 0) {
    // REST Request requesting application/json response.
    char json_string[512];
    DynamicJsonDocument doc(1024);

    doc["R1"]["state"] = relays[R1] == true;
    doc["R1"]["lastUpdated"] = relay_updated[R1];

    doc["R2"]["state"] = relays[R2] == true;
    doc["R2"]["lastUpdated"] = relay_updated[R2];

    doc["R3"]["state"] = relays[R3] == true;
    doc["R3"]["lastUpdated"] = relay_updated[R3]; 

    doc["R4"]["state"] = relays[R4] == true;
    doc["R4"]["lastUpdated"] = relay_updated[R4];

    doc["R5"]["state"] = relays[R5] == true;
    doc["R5"]["lastUpdated"] = relay_updated[R5];

    doc["R6"]["state"] = relays[R6] == true;
    doc["R6"]["lastUpdated"] = relay_updated[R6];

    doc["R7"]["state"] = relays[R7] == true;
    doc["R7"]["lastUpdated"] = relay_updated[R7];

    doc["R8"]["state"] = relays[R8] == true;
    doc["R8"]["lastUpdated"] = relay_updated[R8];

    doc["currentTime"] = currentEpoch;

    serializeJson(doc, json_string);
    server.send(200, "application/json", json_string);
    digitalWrite(BLUE_LED, LOW);
    return;
  }

  if (server.arg("id") == "") {
    server.send(400, "text/plain", "'id' query parameter missing.");
    digitalWrite(BLUE_LED, LOW);
    return;
  }
  if (server.arg("state") == "") {
    server.send(400, "text/plain", "'state' query parameter missing (\"on\" or \"off\".");
    digitalWrite(BLUE_LED, LOW);
    return;
  }

  int relay;
  int state;

  switch (server.arg("id").toInt()) {
    case 1:
      relay = R1;
      break;
    case 2:
      relay = R2;
      break;
    case 3:
      relay = R3;
      break;
    case 4:
      relay = R4;
      break;
    case 5:
      relay = R5;
      break;
    case 6:
      relay = R6;
      break;
    case 7:
      relay = R7;
      break;
    case 8:
      relay = R8;
      break;
    default:
      server.send(400, "text/plain", "Invalid 'id' (relay number) query parameter provided.");
      digitalWrite(BLUE_LED, LOW);
      return;
  }

  relay_updated[relay] = currentEpoch;

  if (server.arg("state") == "on") {
    state = HIGH;
    relays[relay] = true;
  }
  else if (server.arg("state") == "off") {
    state = LOW;
    relays[relay] = false;
  }
  else {
    server.send(400, "text/plain", "Invalid 'state' query parameter provided (\"on\" or \"off\"");
  }

  int on_relays = 0;
  for (int i = 0; i < sizeof(relays); i++) {
    if (relays[i] == true)
      on_relays++;
  }

  // make sure we're not running more than MAX_PARALLEL_RELAYS relays in parallel.
  if (on_relays > MAX_PARALLEL_RELAYS) {
    Serial.println("Too many relays in parallel.");
    allRelaysOff();

    relays[relay] = true;
    on_relays++;

    delay(500);
  }

  digitalWrite(relay, state);
  Serial.printf("Setting relay '%d' to '%d'\n", server.arg("id").toInt(), state);

  if (server.arg("fromMainHTML") == "true") {
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "OK");
    digitalWrite(BLUE_LED, LOW);
    return;
  }

  server.send(200, "text/plain", "");
}

String onOffLinks(int relayNo, bool currentState) {
  String msg = "<b>Relay#" + String(relayNo) + "</b> ";
  if (!currentState) {
    msg += "[ <a href=\"/relays?id=" + String(relayNo) + "&state=on&fromMainHTML=true\">ON</a> / OFF ]";
  } else {
    msg += "[ ON / <a href=\"/relays?id=" + String(relayNo) + "&state=off&fromMainHTML=true\">OFF</a> ]";
  }

  msg += "<br />\n";
  return msg;
}


String MainHTML() {
  String html = "<html><title>Relay Control</title><body>";

  html += "<p>Relay Control ("+String(RELAY_CTRL_VERSION)+")</p>";

  html += "<p>\n";
  html += onOffLinks(1, relays[R1]);
  html += onOffLinks(2, relays[R2]);
  html += onOffLinks(3, relays[R3]);
  html += onOffLinks(4, relays[R4]);
  html += onOffLinks(5, relays[R5]);
  html += onOffLinks(6, relays[R6]);
  html += onOffLinks(7, relays[R7]);
  html += onOffLinks(8, relays[R8]);
  html += "</p>\n";

  html += "</body></html>";
  return html;
}


void allRelaysOff() {
  digitalWrite(R1, LOW);
  digitalWrite(R2, LOW);
  digitalWrite(R3, LOW);
  digitalWrite(R4, LOW);
  digitalWrite(R5, LOW);
  digitalWrite(R6, LOW);
  digitalWrite(R7, LOW);
  digitalWrite(R8, LOW);

  relays[0] = false;
  relays[1] = false;
  relays[2] = false;
  relays[3] = false;
  relays[4] = false;
  relays[5] = false;
  relays[6] = false;
  relays[7] = false;
  relays[8] = false;
  relays[9] = false;
  relays[10] = false;
  relays[11] = false;
  relays[12] = false;
  relays[13] = false;
  relays[14] = false;
  relays[15] = false;
  relays[16] = false;
  relays[17] = false;
  relays[18] = false;
  relays[19] = false;

}
