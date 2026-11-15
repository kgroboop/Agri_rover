/*
 * Agri-Rover Motor Driver Board (ESP-NOW Receiver & Web Server Dashboard)
 */

#include <esp_now.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// ---------------- PIN DEFINITIONS (RECEIVER / DRIVER BOARD) ----------------
#define ENA_DRIVE   1  // GPIO 1 / D0
#define IN1_DRIVE   2  // GPIO 2 / D1
#define IN2_DRIVE   3  // GPIO 3 / D2
#define IN3_DRIVE   4  // GPIO 4 / D3
#define IN4_DRIVE   5  // GPIO 5 / D4
#define ENB_DRIVE   6  // GPIO 6 / D5
#define SERVO_PIN   7  // GPIO 7 / D6 - Pump Servo

const int PUMP_OFF_ANGLE = 0;    
const int PUMP_ON_ANGLE  = 90;   
Servo pumpServo;

// ---------------- WIFI ACCESS POINT CONFIG ----------------
const char* ap_ssid = "AgriRover_Dashboard";
const char* ap_pass = "12345678";
WebServer server(80);

// ---------------- ESP-NOW DATA STRUCTURE ----------------
typedef struct struct_message {
  uint8_t driveCommand; // 0=STOP, 1=FWD
  uint8_t motorSpeed;   
  bool pumpState;       
  float temperature;
  float humidity;
  int soilMoisture;
} struct_message;

struct_message latestTelemetry;
unsigned long logEntryCounter = 1;

// ---------------- HTML WEB DASHBOARD (Safe DOM Rendering) ----------------
const char MAIN_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Agri-Rover AI Dashboard</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #121212; color: #e0e0e0; margin: 0; padding: 15px; text-align: center; }
        .container { max-width: 480px; margin: auto; background: #1e1e1e; padding: 20px; border-radius: 14px; box-shadow: 0 8px 24px rgba(0,0,0,0.4); border: 1px solid #2d2d2d; }
        h1 { color: #00e676; font-size: 1.5em; margin-bottom: 2px; }
        .subtitle { color: #888; font-size: 0.85em; margin-bottom: 15px; }
        
        .card { background: #262626; border-radius: 12px; padding: 15px; margin-bottom: 15px; border: 1px solid #333; text-align: center; }
        .card-title { font-size: 0.75em; text-transform: uppercase; letter-spacing: 1px; color: #a0a0a0; margin-bottom: 10px; }
        
        .snapshot-container { width: 100%; max-height: 320px; background: #000; border-radius: 10px; overflow: hidden; border: 1px solid #444; display: flex; justify-content: center; align-items: center; }
        .snapshot-img { width: 100%; height: auto; object-fit: contain; display: block; }
        
        .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
        .metric-box { background: #1e1e1e; padding: 12px; border-radius: 8px; text-align: center; border: 1px solid #333; }
        .metric-val { font-size: 1.2em; font-weight: bold; margin-top: 4px; color: #ffffff; }
        
        .badge-success { background: #00e676; color: #121212; padding: 3px 8px; border-radius: 4px; font-weight: bold; font-size: 0.8em; }
        .badge-danger { background: #ff5252; color: #ffffff; padding: 3px 8px; border-radius: 4px; font-weight: bold; font-size: 0.8em; }
        
        .btn { background: #00e676; color: #121212; border: none; padding: 10px 20px; border-radius: 8px; cursor: pointer; font-size: 0.9em; font-weight: bold; margin-top: 5px; width: 100%; }
        .btn:active { opacity: 0.8; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🌱 Agri-Rover AI</h1>
        <div class="subtitle" id="currentDateDisplay">Live Telemetry & Feed</div>

        <div class="card">
            <div class="card-title">Live Plant Snapshot View</div>
            <div class="snapshot-container">
                <img id="plantSnapshot" src="http://192.168.4.1/snapshot" class="snapshot-img" alt="Loading Feed...">
            </div>
        </div>

        <div class="card">
            <div class="card-title">Sensor Telemetry & Actuators</div>
            <div class="grid">
                <div class="metric-box">
                    <div class="card-title" style="margin-bottom:4px;">Soil Moisture</div>
                    <div id="val-soil" class="metric-val">--</div>
                </div>
                <div class="metric-box">
                    <div class="card-title" style="margin-bottom:4px;">Temperature</div>
                    <div id="val-temp" class="metric-val">--</div>
                </div>
                <div class="metric-box">
                    <div class="card-title" style="margin-bottom:4px;">Humidity</div>
                    <div id="val-hum" class="metric-val">--</div>
                </div>
                <div class="metric-box">
                    <div class="card-title" style="margin-bottom:4px;">Water Pump</div>
                    <div id="val-pump" class="metric-val" style="margin-top:6px;"><span class="badge-danger">OFF</span></div>
                </div>
            </div>
        </div>

        <button onclick="manualRefresh()" class="btn">🔄 Refresh Data</button>
    </div>

    <script>
        function manualRefresh() {
            const img = document.getElementById('plantSnapshot');
            img.src = "http://192.168.4.1/snapshot?t=" + new Date().getTime();
            fetchTelemetry();
        }

        function fetchTelemetry() {
            fetch('/api/history')
                .then(res => res.json())
                .then(data => {
                    if (!data || data.length === 0) return;
                    const log = data[0];

                    let soilText = log.soilMoisture < 70 ? log.soilMoisture + '% (Dry)' : log.soilMoisture + '% (Moist)';
                    document.getElementById('val-soil').innerText = soilText;
                    document.getElementById('val-temp').innerText = log.temperature.toFixed(1) + ' °C';
                    document.getElementById('val-hum').innerText = log.humidity.toFixed(1) + ' %';
                    
                    const pumpDiv = document.getElementById('val-pump');
                    if (log.pumpState) {
                        pumpDiv.innerHTML = '<span class="badge-success">ON</span>';
                    } else {
                        pumpDiv.innerHTML = '<span class="badge-danger">OFF</span>';
                    }
                })
                .catch(err => console.error("Telemetry fetch error:", err));
        }

        window.onload = function() {
            fetchTelemetry();
            setInterval(fetchTelemetry, 3000);
            setInterval(() => {
                const img = document.getElementById('plantSnapshot');
                img.src = "http://192.168.4.2/snapshot?t=" + new Date().getTime();
            }, 5000);
        };
    </script>
</body>
</html>
)rawliteral";

// ---------------- ESP-NOW RECEIVE CALLBACK ----------------
void onDataRecv(const esp_now_recv_info *info, const uint8_t *incomingDataPtr, int len) {
  memcpy(&latestTelemetry, incomingDataPtr, sizeof(latestTelemetry));
  
  Serial.println("\n--- TELEMETRY PACKET RECEIVED ---");
  Serial.printf("Soil: %d%% | Temp: %.1fC | Pump: %s\n", 
                latestTelemetry.soilMoisture, 
                latestTelemetry.temperature, 
                latestTelemetry.pumpState ? "ON" : "OFF");

  if (latestTelemetry.pumpState) {
    pumpServo.write(PUMP_ON_ANGLE);
  } else {
    pumpServo.write(PUMP_OFF_ANGLE);
  }

  if (latestTelemetry.driveCommand == 1) { 
    digitalWrite(IN1_DRIVE, HIGH); digitalWrite(IN2_DRIVE, LOW);
    digitalWrite(IN3_DRIVE, HIGH); digitalWrite(IN4_DRIVE, LOW);
    
    for (int s = 80; s <= latestTelemetry.motorSpeed; s += 20) {
      analogWrite(ENA_DRIVE, s);
      analogWrite(ENB_DRIVE, s);
      delay(10);
    }
  } else { 
    digitalWrite(IN1_DRIVE, LOW); digitalWrite(IN2_DRIVE, LOW);
    digitalWrite(IN3_DRIVE, LOW); digitalWrite(IN4_DRIVE, LOW);
    analogWrite(ENA_DRIVE, 0); analogWrite(ENB_DRIVE, 0);
  }
}

void handleHistory() {
  String json = "[{";
  json += "\"id\":1,";
  json += "\"temperature\":" + String(latestTelemetry.temperature) + ",";
  json += "\"humidity\":" + String(latestTelemetry.humidity) + ",";
  json += "\"soilMoisture\":" + String(latestTelemetry.soilMoisture) + ",";
  json += "\"pumpState\":" + String(latestTelemetry.pumpState ? "true" : "false");
  json += "}]";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);

  ESP32PWM::allocateTimer(0);
  pumpServo.setPeriodHertz(50);             
  pumpServo.attach(SERVO_PIN, 500, 2400);   
  pumpServo.write(PUMP_OFF_ANGLE);

  pinMode(ENA_DRIVE, OUTPUT);
  pinMode(IN1_DRIVE, OUTPUT);
  pinMode(IN2_DRIVE, OUTPUT);
  pinMode(IN3_DRIVE, OUTPUT);
  pinMode(IN4_DRIVE, OUTPUT);
  pinMode(ENB_DRIVE, OUTPUT);

  WiFi.mode(WIFI_AP_STA); 
  IPAddress local_ip(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP(ap_ssid, ap_pass, 1);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Init Failed!");
    return;
  }
  esp_now_register_recv_cb(onDataRecv);

  server.on("/", []() { server.send_P(200, "text/html", MAIN_PAGE); });
  server.on("/api/history", HTTP_GET, handleHistory);
  server.begin();

  Serial.println("--- MOTOR DRIVER AP & WEB SERVER READY ---");
}

void loop() {
  server.handleClient(); 
}