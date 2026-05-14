#define BLYNK_TEMPLATE_ID "TMPL6OlSHqVJn"
#define BLYNK_TEMPLATE_NAME "HE THONG GIAM SAT CHAT LUONG KHONG KHI"
#define BLYNK_AUTH_TOKEN "QXgsGfrwr2UVeWhjzRA_CdPxsRN7VPuq"
#include "esp_system.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <PMS.h>
// tên + mật khẩu wifi
char ssid[] = "KHONGKHI";
char pass[] = "11111111";
#define DHTPIN 32
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
#define MQ135_PIN 34 
#define PMS_RX 16
#define PMS_TX 17
#define RELAY_FAN   25
#define RELAY_BUZZER 26
HardwareSerial SerialPMS(2);
PMS pms(SerialPMS);
PMS::DATA data;
LiquidCrystal_I2C lcd(0x27, 16, 2);
BlynkTimer timer; // khai báo timer để quản lý việc gửi dữ liệu
// khai báo biến
float t = 0, h = 0, AIRPPM = 0;
int PM25 = 0;
int lastSecondCount = -1;
unsigned long lastDHT = 0;
unsigned long bootTime = 0;
bool fanManual = false, buzzerManual = false;
void sendDataToBlynk() {
    if (Blynk.connected()) 
    {
        // gửi các thông số cảm biến
        Blynk.virtualWrite(V3, t);
        Blynk.virtualWrite(V4, h);
        Blynk.virtualWrite(V5, AIRPPM);
        Blynk.virtualWrite(V6, PM25);
        bool currentFan = digitalRead(RELAY_FAN); // đọc trạng thái relay hiện tại
        bool currentBuzzer = digitalRead(RELAY_BUZZER);
        Blynk.virtualWrite(V9, currentFan ? 255 : 0); // update trạng thái led
        Blynk.virtualWrite(V10, currentBuzzer ? 255 : 0);
    }
}
BLYNK_WRITE(V0) // callback khi app ghi vào v0
{ 
    fanManual = param.asInt(); // đọc giá trị 0 hoặc 1
    digitalWrite(RELAY_FAN, fanManual); // điều khiển relay
}
BLYNK_WRITE(V1) 
{ 
    buzzerManual = param.asInt(); 
    digitalWrite(RELAY_BUZZER, buzzerManual); 
}
BLYNK_WRITE(V2) 
{ 
    if (param.asInt() == 1) ESP.restart(); // reset bằng phần mềm, 0: bình thường, 1: reset
}
void setup() 
{
    Serial.begin(115200);
    bootTime = millis();
    pinMode(RELAY_FAN, OUTPUT); 
    pinMode(RELAY_BUZZER, OUTPUT);
    digitalWrite(RELAY_FAN, LOW);
    digitalWrite(RELAY_BUZZER, LOW);
    lcd.init();
    lcd.backlight();
    Wire.begin(21, 22);
    lcd.setCursor(2, 0); lcd.print("LAI QUOC BAO");
    lcd.setCursor(1, 1); lcd.print("MSV:2022606785");
    delay(2000);
    for (int i = 5; i > 0; i--) 
    {
        lcd.clear(); lcd.setCursor(2, 0); lcd.print("SYSTEM START"); lcd.setCursor(7, 1); lcd.print(i); lcd.print("s");
        delay(1000);
    }
    lcd.setCursor(2, 1); lcd.print("SYSTEM READY");
    WiFi.begin(ssid, pass);
    unsigned long startAttempt = millis();while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 1000)
    {
        delay(500);
    }
    if(WiFi.status() == WL_CONNECTED)
    {
        Blynk.config(BLYNK_AUTH_TOKEN);
        Blynk.connect();
    }
    dht.begin();
    analogReadResolution(12);
    SerialPMS.begin(9600, SERIAL_8N1, PMS_RX, PMS_TX);
    timer.setInterval(2000L, sendDataToBlynk);
    delay(500);
    lcd.clear();
}
void loop() 
{
    // kiểm tra wifi, nếu mất thì kết nối lại
    if (WiFi.status() != WL_CONNECTED) 
    {
        static unsigned long lastReconnect = 0;
        if (millis() - lastReconnect > 5000) 
        {
            lastReconnect = millis();
            WiFi.begin(ssid, pass);
        }
    } 
    else 
    {
        Blynk.run();
    }
    timer.run(); // chạy bộ quản lý thời gian của blynk
    unsigned long currentMillis = millis();
    // đọc cảm biến dht
    if (millis() - lastDHT >= 1000) 
    {
        lastDHT = millis();
        float nt = dht.readTemperature(); 
        float nh = dht.readHumidity();
        if (!isnan(nt) && !isnan(nh)) // chống lỗi not a number
        { 
            t = nt; 
            h = nh; 
        }
    }
    // đọc cảm biến mq-135 và pms7003
    static float mqFilter = 400;
    int raw = analogRead(MQ135_PIN);
    mqFilter = (mqFilter * 0.9) + (map(raw, 0, 4095, 400, 4000) * 0.1); // quy đổi ADC sang ppm tương đối để đánh giá chất lượng không khí
    AIRPPM = mqFilter;
    if (AIRPPM > 4000) AIRPPM = 4000; 
    if (AIRPPM < 400) AIRPPM = 400;
    if (pms.read(data)) 
    { 
        PM25 = data.PM_AE_UG_2_5; 
    }
    // logic điều khiển thực tế
    bool badAir = (PM25 > 300 || AIRPPM > 1300);
    bool danger = (PM25 > 600 || t > 40 || h > 98 || AIRPPM > 3998);
    bool highHumi = (h > 90);
    if (!fanManual) 
    {
        digitalWrite(RELAY_FAN, (badAir || danger || highHumi));
    }
    if (!buzzerManual) 
    {
        digitalWrite(RELAY_BUZZER, danger);
    }
    // đồng bộ hiển thị trên lcd thực tế và lcd trên blynk
    static unsigned long lastLCD = 0;
    if (currentMillis - lastLCD > 1000) 
    {
        lastLCD = currentMillis;
        String line1 = "";
        String line2 = "";
        if (t > 40) 
        {
            line1 = "WARNING:NHIET DO      ";
            line2 = "TEMP:" + String(t, 1) + "         ";
        } 
        else if (PM25 > 300) 
        {
            line1 = "WARNING: BUI MIN      ";
            line2 = "PMS:" + String(PM25) + "ug/m3      ";
        } 
        else if (AIRPPM > 1300) 
        {
            line1 = "WARNING: O NHIEM      ";
            line2 = "AIR QUAL:" + String((int)AIRPPM) + "ppm      ";
        } 
        else if (highHumi) 
        {
            line1 = "WARNING: DO AM      ";
            line2 = "HUMIDITY:" + String(h, 0) + "%      ";
        } 
        else 
        {
            line1 = "T:" + String(t, 1) + " AQ:" + String((int)AIRPPM) + "ppm      ";
            line2 = "H:" + String(h, 0) + "% P:" + String(PM25) + "ug/m3      ";
        }
        // hiển thị LCD thực tế
        lcd.setCursor(0, 0); lcd.print(line1);
        lcd.setCursor(0, 1); lcd.print(line2);
        // Hiển thị LCD trên blynk
        if (Blynk.connected()) 
        {
            Blynk.virtualWrite(V7, line1);
            Blynk.virtualWrite(V8, line2);
        }
    }
}