#include <WiFi.h>

void connect() {
  WiFi.disconnect();
  WiFi.setAutoReconnect(); 
  while (WiFi.status() != WL_CONNECTED) {
    int n = WiFi.scanNetworks();
    
    if (n == 0) {
      Serial.println("No network was found.");
      delay(3000);
      continue;
    }

    if (password == "0" || password.length() == 0) WiFi.begin(ssid.c_str());
    else WiFi.begin(ssid.c_str(), password.c_str());

    int t = 0;
    while (WiFi.status() != WL_CONNECTED && t < 20) {
      Serial.print(".");
      delay(500);
      t++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected");
      Serial.print("Local IP: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("\nCan't connect to network.");
      WiFi.disconnect();
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(9600);
  connect();
}

void loop() {
}
