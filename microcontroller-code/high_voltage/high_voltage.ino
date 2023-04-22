#include <SPI.h>
#include <MFRC522.h>
#include<LiquidCrystal.h>

#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
 
#define SS_PIN 21
#define RST_PIN 22
#define LED_G 2 //define green LED pin
#define LED_R 12 //define red LED
#define BUZZER 13 //buzzer pin

int rs=32,en=33,d4=25,d5=26,d6=27,d7=14;
LiquidCrystal lcd(rs,en,d4,d5,d6,d7);

MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance.

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 19800;
const int   daylightOffset_sec = 0;

#define WIFI_SSID "TRIN 7151"
#define WIFI_PASSWORD "Trin@7161"

#define API_KEY "AIzaSyA328XAjBnlh7zLhlL4KDg0cU1KZWxlYyM"
#define DATABASE_URL "rfid-attendance-system-fbc24-default-rtdb.asia-southeast1.firebasedatabase.app"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK=false;
FirebaseJson json;

void connectToWifi()
{
  WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  Serial.print("Connecting to WiFi: ");
  Serial.print(WIFI_SSID);
  while(WiFi.status()!=WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();
}


void setup()
{
  Serial.begin(115200);   // Initiate a serial communication
  SPI.begin();      // Initiate  SPI bus
  mfrc522.PCD_Init();   // Initiate MFRC522
  pinMode(LED_G, OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER,LOW);
  Serial.println("Put your card to the RFID reader...");
  Serial.println();
  lcd.begin(16,2);

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  connectToWifi();
  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}
void loop()
{
  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent())
  {
    return;
  }
  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial())
  {
    return;
  }
  //Show UID on serial monitor
  Serial.print("UID tag :");
  String content= "";
  byte letter;
  for (byte i = 0; i < mfrc522.uid.size; i++)
  {
     Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
     Serial.print(mfrc522.uid.uidByte[i], HEX);
     content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
     content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  Serial.println();
  content.toUpperCase();
  lcd.setCursor(0,0);
  lcd.print(content);
  lcd.setCursor(0,1);
  Serial.print("Message : ");
  content.toUpperCase();

  struct tm timeinfo;
  if(!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return;
  }
  
  //Time
  char timeHour[3],timeMinute[3],timeSeconds[3];
  strftime(timeHour,3, "%H", &timeinfo);
  strftime(timeMinute,3, "%M", &timeinfo);
  strftime(timeSeconds,3, "%S", &timeinfo);

  String hour=String(timeHour);
  String min=String(timeMinute);
  String sec=String(timeSeconds);

  String time=hour+":"+min+":"+sec;

  //Date
  char timeDay[3],timeMonth[20],timeWeekDay[10],timeYear[5];
  strftime(timeDay,3, "%d", &timeinfo);
  strftime(timeMonth,20, "%B", &timeinfo);
  strftime(timeYear,5, "%Y", &timeinfo);
  strftime(timeWeekDay,10, "%A", &timeinfo);

  String day=String(timeDay);
  String month=String(timeMonth);
  String year=String(timeYear);  
  String weekday=String(timeWeekDay);

  String date=day+"-"+month+"-"+year+", "+weekday; 
  
  if (content.substring(1) == "6C D6 AB 51") //change here the UID of the card/cards that you want to give access
  {
    lcd.print("Access granted");
    Serial.println(" Access granted");
    digitalWrite(LED_G, HIGH); 
    String pth="Authorized/"+content;
    json.set("UID",content);
    json.set("TimeStamp",time);
    json.set("Date",date);
    json.set("Zone","High Voltage Zone");
    if (Firebase.RTDB.setJSON(&fbdo, pth, &json ))
    {
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType());
    }
    else
    {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
    Serial.println();
    delay(500);
    delay(3000);
    digitalWrite(LED_G, LOW);
  }
  else 
  {
    lcd.print("Access Denied");
    Serial.println(" Access denied");
    digitalWrite(LED_R, HIGH);
    digitalWrite(BUZZER,HIGH);
    String pth="Unauthorized/"+content;
    json.set("UID",content);
    json.set("TimeStamp",time);
    json.set("Date",date);
    json.set("Zone","High Voltage Zone");
    if (Firebase.RTDB.setJSON(&fbdo, pth, &json ))
    {
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType());
    }
    else
    {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
    delay(3000);
    digitalWrite(LED_R, LOW);
    digitalWrite(BUZZER,LOW);
  }
  lcd.clear();
} 