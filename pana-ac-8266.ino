#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRutils.h>
#include <WiFiClient.h>
#include <ir_Panasonic.h>
#include <time.h>
#include "config.h"
#include "wifi.h"

const uint16_t kIrLed = 12;  // ESP8266 GPIO pin to use. Recommended: 4 (D6).
IRPanasonicAc ac(kIrLed);    // Set the GPIO used for sending messages.
ESP8266WebServer server(80);
const time_t myZone = 3600 * 7;
const char *textPlain = "text/plain";
char timeBuff[128];

bool isTherePendingJob = false;
time_t setOffAt = 0;

void handleRoot() {
    server.send_P(200, "text/html", (const char *)index_html, index_html_len);
}

void handleControl() {
    String response = "POSTED";
    String action = server.arg("action");
    if (action == "on") {
        String fan = server.arg("fan");
        if (fan == "med") {
            ac.setFan(kPanasonicAcFanMed);
        } else if (fan == "max") {
            ac.setFan(kPanasonicAcFanMax);
        } else {
            ac.setFan(kPanasonicAcFanAuto);
        }

        long temp = server.arg("temp").toInt();
        if (temp == 0 || temp < 16 || temp > 30) {
            temp = 27;
        }
        ac.setTemp(temp);
        String mode = server.arg("mode");
        if (mode == "heat") {
            ac.setMode(kPanasonicAcHeat);
        } else if (mode == "dry") {
            ac.setMode(kPanasonicAcDry);
        } else {
            ac.setMode(kPanasonicAcCool);
        }
        uint8_t swingV = kPanasonicAcSwingVHigh;
        String swingv = server.arg("swingv");
        if (swingv == "highest"){
            swingV = kPanasonicAcSwingVHighest;
        }else if (swingv == "high"){
            swingV = kPanasonicAcSwingVHigh;
        }else if (swingv == "mid"){
            swingV = kPanasonicAcSwingVMiddle;
        }else if (swingv == "low"){
            swingV = kPanasonicAcSwingVLow;
        }else if (swingv == "lowest"){
            swingV = kPanasonicAcSwingVLowest;
        }else {
            swingV = kPanasonicAcSwingVAuto;
        }
        ac.setSwingVertical(swingV);
        uint8_t swingH;
        String swingh = server.arg("swingh");
        if (swingh == "fleft"){
            swingH = kPanasonicAcSwingHFullLeft;
        }else if (swingh == "left"){
            swingH = kPanasonicAcSwingHLeft;
        }else if (swingh == "mid"){
            swingH = kPanasonicAcSwingHMiddle;
        }else if (swingh == "right"){
            swingH = kPanasonicAcSwingHRight;
        }else if (swingh == "fright"){
            swingH = kPanasonicAcSwingHFullRight;
        }else {
            swingH = kPanasonicAcSwingHAuto;
        }

        ac.setSwingHorizontal(swingH);
        ac.on();
        ac.send();
    }
    if (action == "off") {
        ac.off();
        ac.send();
    }
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, textPlain, response);
    return;
}

char *timeToString(time_t v) {
    v += myZone;
    struct tm timeinfo;
    gmtime_r(&v, &timeinfo);
    strftime(timeBuff, sizeof(timeBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return timeBuff;
}

void handleSetTimeOff() {
    setOffAt = server.arg("at").toInt();
    time_t nowIs = time(nullptr);
    if (setOffAt == 0) {
        server.send(200, textPlain, "'at' parameter need set to timestamp in seconds");
        return;
    }
    if (setOffAt <= nowIs) {
        server.send(200, textPlain, "'at' parameter need set to future timestamp in seconds");
        return;
    }
    isTherePendingJob = true;
    char buff[256];
    sprintf(buff, "ok, turn off set to %s", timeToString(setOffAt));
    Serial.println(buff);
    server.send(200, textPlain, buff);
    return;
}

void handleNotFound() {
    server.send(404, textPlain, "Not found");
}

time_t updateClock() {  // 7 * 3600
    configTime(0, 0, "0.asia.pool.ntp.org", "1.asia.pool.ntp.org", "2.asia.pool.ntp.org");
    Serial.print("waiting for NTP time sync: ");
    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2) {
        delay(500);
        Serial.print(".");
        now = time(nullptr);
    }
    Serial.printf("current time: %s", timeToString(now));
    return now;
}
void connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, PASSWORD);
    Serial.print("Connecting... ");
    int i = 0;
    while (WiFi.status() != WL_CONNECTED) {  // Wait for the Wi-Fi to connect
        delay(1000);
        Serial.print(++i);
        Serial.print(' ');
    }
    Serial.println('\n');
    Serial.print("Connected to ");
    Serial.println(WiFi.SSID());  // Tell us what network we're connected to
    Serial.print("IP address:\t");
    Serial.println(WiFi.localIP());  // Send the IP address of the ESP8266 to the computer

    if (MDNS.begin("ac-remote"), WiFi.localIP()) {
        Serial.println("MDNS responder started");
        MDNS.addService("http", "tcp", 80);
    }
}

void setupAC() {
    ac.begin();
    ac.setModel(kPanasonicRkr);
    ac.setFan(kPanasonicAcFanMed);
    ac.setTemp(27);
    ac.setMode(kPanasonicAcCool);
    ac.setSwingVertical(kPanasonicAcSwingVHigh);
    ac.setSwingHorizontal(kPanasonicAcSwingHMiddle);
}

void handleGetTimeOff() {
    char buff[256];
    sprintf(buff, "%d;%d", isTherePendingJob, setOffAt);
    server.send(200, textPlain, buff);
}

void handleCancelTimer() {
    if (isTherePendingJob) {
        isTherePendingJob = false;
        server.send(200, textPlain, "ok");
        return;
    }
    server.send(200, textPlain, "nop");
}

auto wrapHandle(void (*f)()) {
    auto h = [f]() {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        f();
    };
    return h;
}

void setup(void) {
    Serial.begin(115200);
    while (!Serial)  // Wait for the serial connection to be established.
        delay(50);
    connectWiFi();
    setupAC();
    server.on("/", handleRoot);
    server.on("/set-time-off", HTTP_POST, wrapHandle(handleSetTimeOff));
    server.on("/set-time-off", HTTP_GET, wrapHandle(handleGetTimeOff));
    server.on("/control", HTTP_POST, wrapHandle(handleControl));
    server.on("/cancel-timer", HTTP_POST, wrapHandle(handleCancelTimer));

    server.onNotFound(handleNotFound);
    updateClock();
    server.begin();
    Serial.println();
    Serial.println("HTTP server started");
}

void checkTask() {
    if (isTherePendingJob) {
        time_t now = time(nullptr);
        if (now >= setOffAt) {
            Serial.printf("current time: %s, job now fired", timeToString(now));
            isTherePendingJob = false;
            ac.off();
            ac.send();
        }
    }
}

void loop(void) {
    checkTask();
    MDNS.update();
    server.handleClient();
}
