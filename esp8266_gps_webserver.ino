// Wiring:
// NEO-6M GPS TX -> NodeMCU D6 (GPIO12 / SoftwareSerial RX)
// NEO-6M GPS RX -> NodeMCU D5 (GPIO14 / SoftwareSerial TX)
// Connect GPS VCC to 3.3V and GND to GND.
// Ensure GPS module shares common ground with the ESP8266.

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>

// WiFi Access Point credentials
const char *AP_SSID = "PetTracker";
const char *AP_PASS = "12345678";

// GPS settings
static const uint32_t GPS_BAUD = 9600;

// SoftwareSerial pins: RX, TX (ESP perspective)
SoftwareSerial gpsSerial(D6, D5);
TinyGPSPlus gps;

ESP8266WebServer server(80);

double lastLat = 0.0;
double lastLon = 0.0;
bool hasFix = false;
bool hasPosition = false;

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Pet Tracker</title>
  <style>
    body { font-family: Arial, sans-serif; background: #f5f7fa; margin: 0; padding: 0; }
    .container { max-width: 480px; margin: 30px auto; background: #fff; padding: 20px; box-shadow: 0 2px 8px rgba(0,0,0,0.1); border-radius: 10px; }
    h1 { margin-top: 0; font-size: 1.6em; text-align: center; color: #2c3e50; }
    .status { font-size: 1.2em; margin: 10px 0; }
    .value { font-weight: bold; }
    button { width: 100%; padding: 12px; font-size: 1em; background: #3498db; color: white; border: none; border-radius: 8px; cursor: pointer; }
    button:active { background: #2980b9; }
  </style>
</head>
<body>
  <div class="container">
    <h1>Pet Tracker</h1>
    <div class="status">GPS Fix: <span id="fix" class="value">--</span></div>
    <div class="status">Latitude: <span id="lat" class="value">--</span></div>
    <div class="status">Longitude: <span id="lon" class="value">--</span></div>
    <button id="mapBtn" type="button" disabled>Waiting for GPS...</button>
  </div>
<script>
const fixEl = document.getElementById('fix');
const latEl = document.getElementById('lat');
const lonEl = document.getElementById('lon');
const mapBtn = document.getElementById('mapBtn');

function updateMapLink(lat, lon) {
  mapBtn.onclick = () => {
    window.open(`https://www.google.com/maps?q=${lat},${lon}`, '_blank');
  };
}

async function fetchGPS() {
  try {
    const res = await fetch('/gps');
    const data = await res.json();
    fixEl.textContent = data.fix ? 'FIX' : 'NO FIX';
    if (data.lat === null || data.lon === null) {
      latEl.textContent = '--';
      lonEl.textContent = '--';
      mapBtn.disabled = true;
      mapBtn.textContent = 'Waiting for GPS...';
    } else {
      latEl.textContent = data.lat.toFixed(6);
      lonEl.textContent = data.lon.toFixed(6);
      updateMapLink(data.lat, data.lon);
      mapBtn.disabled = false;
      mapBtn.textContent = 'Open in Google Maps';
    }
  } catch (e) {
    fixEl.textContent = 'ERROR';
  }
}

fetchGPS();
setInterval(fetchGPS, 2000);
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleGps() {
  String json = "{\"fix\":";
  json += hasFix ? "true" : "false";
  json += ",\"lat\":";
  json += hasPosition ? String(lastLat, 6) : "null";
  json += ",\"lon\":";
  json += hasPosition ? String(lastLon, 6) : "null";
  json += "}";
  server.send(200, "application/json", json);
}

void setupWiFi() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress ip = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(ip);
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("Starting Pet Tracker...");

  gpsSerial.begin(GPS_BAUD);
  setupWiFi();

  server.on("/", handleRoot);
  server.on("/gps", handleGps);
  server.begin();
  Serial.println("Web server started on port 80");
}

void loop() {
  while (gpsSerial.available() > 0) {
    if (gps.encode(gpsSerial.read())) {
      if (gps.location.isValid() && gps.location.isUpdated()) {
        lastLat = gps.location.lat();
        lastLon = gps.location.lng();
        hasFix = true;
        hasPosition = true;
        Serial.print("GPS update - Fix: YES Lat: ");
        Serial.print(lastLat, 6);
        Serial.print(" Lon: ");
        Serial.println(lastLon, 6);
      } else if (!gps.location.isValid()) {
        hasFix = false;
        Serial.println("GPS data invalid - no fix");
      } else {
        hasFix = false;
      }
    }
  }

  server.handleClient();
}
