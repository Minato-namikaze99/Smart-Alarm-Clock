#include <WiFi.h>
#include <HTTPClient.h>
#include "time.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>


const char* ssid = "<WIFI NAME>";
const char* password = "<WIFI PASSWORD>";

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800;
const int daylightOffset_sec = 0;

const char* serverUrl = "http://<PUBLIC URL OF THE FLASK SERVER>:5000/events";

const int buzzerPin = 13;
const int snoozeButtonPin = 12;
const int stopButtonPin = 14;

AsyncWebServer server(80);

int alarmHour = -1;
int alarmMinute = -1;
bool alarmTriggered = false;
bool snoozed = false;
time_t snoozeStart = 0;

String calendarData = "";

// Time structure
struct tm timeinfo;

void setup() {
  Serial.begin(115200);
  pinMode(buzzerPin, OUTPUT);
  pinMode(snoozeButtonPin, INPUT_PULLUP);
  pinMode(stopButtonPin, INPUT_PULLUP);

  connectToWiFi();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Host Web Server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    char timeStr[6];
    getLocalTime(&timeinfo);
    sprintf(timeStr, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

    String html = R"rawliteral(
    <html>
    <body>
    <h2>Smart Alarm Clock</h2>
    <p>Current Time: <span id="clock">--:--</span></p>
    <script>
    function updateClock() {
    const now = new Date();
    const hours = now.getHours().toString().padStart(2, '0');
    const minutes = now.getMinutes().toString().padStart(2, '0');
    document.getElementById("clock").textContent = hours + ":" + minutes;
    }
    updateClock();
    setInterval(updateClock, 60000);
    </script>
    <form action="/set_alarm" method="GET">
    Set Alarm: <input name='hour' type='number' min='0' max='23'> : <input name='minute' type='number' min='0' max='59'>
    <input type='submit' value='Set Alarm'>
    </form>
    <p>Alarm set for: )rawliteral" + String(alarmHour) + ":" + String(alarmMinute) + R"rawliteral(</p>
    <hr><h3>Calendar Tasks</h3>)rawliteral" + calendarData + R"rawliteral(
    </body>
    </html>
    )rawliteral";

    
    request->send(200, "text/html", html);
  });

  server.on("/set_alarm", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("hour") && request->hasParam("minute")) {
      alarmHour = request->getParam("hour")->value().toInt();
      alarmMinute = request->getParam("minute")->value().toInt();
      alarmTriggered = false;
      request->redirect("/");
    }
  });

  server.begin();
}

void loop() {
  getLocalTime(&timeinfo);

  int currentHour = timeinfo.tm_hour;
  int currentMinute = timeinfo.tm_min;
  int currentSecond = timeinfo.tm_sec;

  Serial.printf("⏳ Current time: %02d:%02d:%02d\n", currentHour, currentMinute, currentSecond);

  // Trigger alarm
  if (!alarmTriggered && currentHour == alarmHour && currentMinute == alarmMinute) {
    Serial.println("🔔 Alarm Time! Buzzing...");
    digitalWrite(buzzerPin, HIGH);
    alarmTriggered = true;
  }

  // Handle Snooze
  if (alarmTriggered && digitalRead(snoozeButtonPin) == LOW) {
    Serial.println("😴 Snooze pressed. Delaying for 2 mins.");
    digitalWrite(buzzerPin, LOW);
    snoozed = true;
    snoozeStart = time(nullptr);
    delay(1000); // debounce
  }

  if (snoozed && (time(nullptr) - snoozeStart >= 120)) {
    Serial.println("🔁 Snooze time over. Buzzing again!");
    digitalWrite(buzzerPin, HIGH);
    snoozed = false;
  }

  // Stop and fetch calendar
  if (alarmTriggered && digitalRead(stopButtonPin) == LOW) {
    Serial.println("🛑 Alarm stopped. Fetching calendar...");
    digitalWrite(buzzerPin, LOW);
    fetchCalendarEvents();
    delay(1000); // debounce
  }

  delay(1000);
}

void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected!");
  Serial.println(WiFi.localIP());
}

void fetchCalendarEvents() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);
    int httpCode = http.GET();

    if (httpCode == 200) {
      String response = http.getString();
      calendarData = "<ul>";

      DynamicJsonDocument doc(2048);
      deserializeJson(doc, response);

      for (JsonObject event : doc.as<JsonArray>()) {
        String summary = event["summary"] | "No Title";
        String start = event["start"]["dateTime"] | event["start"]["date"];
        calendarData += "<li><b>" + summary + "</b><br>" + start + "</li>";
      }

      calendarData += "</ul>";
      Serial.println("✅ Calendar data updated.");
    } else {
      Serial.println("❌ Failed to fetch events.");
    }

    http.end();
  }
}
