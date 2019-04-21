#include <Arduino.h>
#include <U8x8lib.h>
#include <WiFi.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WebServer.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif

typedef struct _tinfo
{
  char tarif[16];
  char iinst[16];
  char papp[16];
  char isousc[16];
  char hchc[16];
  char hchp[16];
  char base[16];

} tinfo;

tinfo info = {
  "", 
  "",
  "",
  "",
  "",
  "",
  ""
} ; 

U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/4, /* data=*/5, /* reset=*/U8X8_PIN_NONE); // Digispark ATTiny85
WebServer server(80);
HardwareSerial TeleinfoSer(2);

const char *ssid = "******";
const char *password = "******";

int Donnee = 0;
String Ligne;     // la ligne complette (entre LF(0x0A) et CR(0x0D))
String Etiquette; // le nom de la mesure via teleinfo
String Valeur;    // la valeur de la mesure
char Checksum;    // pour controle de la validit d des donnees recues
char Controle = 0;


#define SCREEN_DELAY 2000
int Screen = 0;
unsigned long ScreenTime = 0;

void DisplayMessage(const char *szMsg, int size)
{
  char * szpos ; 
  u8x8.clear();
  u8x8.setFont(u8x8_font_8x13_1x2_r);
  u8x8.drawString(0, 1, "__________");

  u8x8.setFont(u8x8_font_8x13_1x2_r);
  u8x8.drawString(0, 0, "TELEINFO :");

  switch (size)
  {
  case 1:
    u8x8.setFont(u8x8_font_8x13B_1x2_r);
    break;
  case 2:
  default:
    u8x8.setFont(u8x8_font_inb21_2x4_r);
    break;
  }
  if ((szpos = strchr(szMsg, '\n'))!= NULL )
  {
    *szpos = '\0';
    szpos++ ; 
  }
    u8x8.drawString(0, 4, szMsg);
  if (szpos != NULL )     u8x8.drawString(0, 6, szpos);

  u8x8.refreshDisplay();
}

void DisplayLoop(void)
{
  char lcdBuffer[64];

  switch (Screen)
  {
  case 0:
  {
    IPAddress ip = WiFi.localIP();
    sprintf(lcdBuffer, "%03d.%03d.%03d.%03d\n%s", ip[0], ip[1], ip[2], ip[3],ssid);
    DisplayMessage(lcdBuffer, 1);
  }
  break;
  case 1:
  {
    if (strstr(info.tarif, "HC") != NULL)
    {
      DisplayMessage("HEURE CREUSE", 1);
    }
    else if (strstr(info.tarif, "HP") != NULL)
    {
      DisplayMessage("HEURE PLEINE", 1);
    }
  }
  break ; 
  case 2 : 
  {
    sprintf(lcdBuffer, "%dA/%dA", atoi(info.iinst),atoi(info.isousc)) ; 
        DisplayMessage(lcdBuffer, 2);
  }
  break;
  case 3 :
  {
    sprintf(lcdBuffer, "%d W", atoi(info.papp));
    DisplayMessage(lcdBuffer, 2);
  }
  break ; 
  case 4 :
    if (strlen (info.hchc) != 0 )
    {
      sprintf(lcdBuffer, "HP:%.2f kWh\nHC:%.2f kWh", (float)(atoi(info.hchp)) / 1000, (float)(atoi(info.hchc) / 1000));
    }
    else
    {
      sprintf(lcdBuffer, "BASE:%.2f kWh", (float)(atoi(info.base)) / 1000);
    }
    DisplayMessage(lcdBuffer, 1);
    break ;
  default:
    DisplayMessage("        ", 2);
    break;
  }
  Screen++;
  if (Screen > 4)
    Screen = 0;
}

void Gestion_Teleinfo(String szEtiquette, String szValeur)
{
  Serial.print(szEtiquette);
  Serial.print(" ");
  Serial.println(szValeur);
  if (strstr(szEtiquette.c_str(), "PTEC") != NULL)
  {
    strcpy(info.tarif, szValeur.c_str());
  }
  else if (strstr(szEtiquette.c_str(), "IINST") != NULL)
  {
    strcpy(info.iinst, szValeur.c_str());
  }
  else if (strstr(szEtiquette.c_str(), "PAPP") != NULL)
  {
    strcpy(info.papp, szValeur.c_str());
  }
  else if (strstr(szEtiquette.c_str(), "ISOUSC") != NULL)
  {
    strcpy(info.isousc, szValeur.c_str());
  }
  else if (strstr(szEtiquette.c_str(), "HCHC") != NULL)
  {
    strcpy(info.hchc, szValeur.c_str());
  }
  else if (strstr(szEtiquette.c_str(), "HCHP") != NULL)
  {
    strcpy(info.hchp, szValeur.c_str());
  }
  else if (strstr(szEtiquette.c_str(), "BASE") != NULL)
  {
    strcpy(info.base, szValeur.c_str());
  }
}

void handle_OnConnect()
{
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>Teleinfo</title>\n";
  ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
  ptr += "p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
  ptr += "</style>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<h1>ESP32 Web Server</h1>\n";
  ptr += "</body>\n";
  ptr += "</html>\n";
  server.send(200, "text/html", ptr);
}

void setup()
{
  Preferences preferences;

  TeleinfoSer.begin(1200, SERIAL_7E1, -1, -1, false);
  Serial.begin(9600);

  u8x8.begin();

  DisplayMessage("Booting.", 2);

  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  ArduinoOTA.setHostname("ESP32 Teleinfo");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)
      Serial.println("End Failed");
  });

  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handle_OnConnect);
  server.begin();

  DisplayMessage("        ", 2);
  ScreenTime = millis();
}

void loop()
{
  server.handleClient();
  ArduinoOTA.handle();

  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  while (TeleinfoSer.available() > 0)
  {
    char CharTeleinfo = TeleinfoSer.read();

    switch (CharTeleinfo)
    {
    case 0x0A:
      Etiquette = "";
      Valeur = "";
      Checksum = 0;
      Donnee = 1;
      Controle = 0;
      break;
    case 0x20:
      Donnee++;
      break;
    case 0x0D:
      Donnee = 0;
      Ligne = Etiquette + " " + Valeur;
      for (byte i = 0; i < (Ligne.length()); i++)
      {
        Controle += Ligne[i];
      }
      Controle = (Controle & 0x3F) + 0x20;
      if (Controle == Checksum)
      {
        Gestion_Teleinfo(Etiquette, Valeur);
      }
      else
      {
        Serial.print("Error : ");
        Gestion_Teleinfo(Etiquette, Valeur);
      }

      break;
    default:
      if (Donnee == 1)
      {
        Etiquette = Etiquette + CharTeleinfo;
      }

      if (Donnee == 2)
      {
        Valeur = Valeur + CharTeleinfo;
      }

      if (Donnee == 3)
      {
        Checksum = Checksum + CharTeleinfo;
      }
      break;
    }

    //Serial.write(CharTeleinfo);

    yield();
  }

  if (millis() > (SCREEN_DELAY + ScreenTime))
  {
    ScreenTime = millis();
    DisplayLoop();
  }
}
