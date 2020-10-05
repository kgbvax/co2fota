#include <Arduino.h>
#include <M5Stack.h>
#include <ArduinoOTA.h>
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

#include "version.h"

//networking
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClient.h>

//not in SCM, provide your own defining WIFI_SSID, WIFI_PASSWORD
#include "envs.h"

const char *ssid{WIFI_SSID}; //  SSID (2.4GHz)
const char *password{WIFI_PASSWORD};

const char *ntpServer = "europe.pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

String macStr;

static const char *TAG = "co2fota";


#define TRIANGLE(x1,y1,x2,y2 ,x3,y3) M5.Lcd.fillTriangle(pad+(x1),pad+(y1),pad+(x2),pad+(y2),pad+(x3),pad+(y3), WHITE)
void drawM() {
  
  const int pad=4;
  const int my = M5.Lcd.height()-pad*2;
  const int mx = M5.Lcd.width()-pad*2;

 
  TRIANGLE(0,my/2,mx/2,my/2,mx/4, 0); // ▲ Links
  TRIANGLE(mx/2,my/2,mx/2+mx/4,0,mx, my/2); // ▲ Rechts
  TRIANGLE(0,my/2,mx/8,my-my/4, mx/4,my/2); // ▾ Links
  TRIANGLE(mx-(mx/4),my/2,mx,my/2,mx-(mx/8),my-my/4); // ▾ Rechts
  TRIANGLE(mx/4,my/2,mx/2,my, mx-(mx/4),my/2); // ▼

}

void setup()
{

  Serial.begin(115200);
  esp_log_level_set("*", ESP_LOG_DEBUG);
  Serial.println("setup begin");

  delay(1250);
  M5.begin();
  M5.Lcd.fillScreen(BLACK);
  drawM();
  delay(3000);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0,2);
  M5.Lcd.setTextSize(1);
  esp_chip_info_t ci;
  esp_chip_info(&ci);
  Serial.println("chip model " + String(ci.model));
  Serial.println("chip revision " + String(ci.revision));
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  char macBuf[20]; //xx-xx-xx-xx-xx-xx
  sprintf(macBuf, "%x-%x-%x-%x-%x-%x", (unsigned int)mac[0], (unsigned int)mac[1], (unsigned int)mac[2], (unsigned int)mac[3], (unsigned int)mac[4], (unsigned int)mac[5]);
  macStr = String(macBuf);
  M5.Lcd.setTextSize(2);
  Serial.println("MAC: " + macStr);
  M5.Lcd.println("MAC: " + macStr);
  Serial.println("Version: " + String(VERSION));
  M5.Lcd.println("Version: " + String(VERSION));
  M5.Lcd.setTextSize(1);
  Serial.println("MD5: " + EspClass().getSketchMD5());
  M5.Lcd.println("MD5: " + EspClass().getSketchMD5());
  Serial.println("Build timestamp: " + String(BUILD_TIMESTAMP));
  M5.Lcd.println("Build timestamp:  " + String(BUILD_TIMESTAMP));
  M5.Lcd.println("configured SSID: " + String(ssid));

  WiFi.begin(ssid, password);
  WiFi.setHostname("co2fota");
  WiFi.setAutoReconnect(true);

  int waitConnected = 20;
  while (!WiFi.isConnected() && --waitConnected > 0)
  {
    delay(500);
    Serial.print(".");
  }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println();

  Serial.println("setup complete\n");
  M5.Lcd.println("setup complete\n");
  delay(5000);
}

void pollFOTAUpdate(String host, int port, String uriBase)
{
  static long int lastPoll = 0;
  const int _ota_timeout = 10000;

  WiFiClientSecure cl;

  HTTPClient http;
  // Variables to validate firmware content
  volatile int contentLength = 0;

 
  Serial.println("poll " + host + " - " + uriBase);
  http.begin(cl, host, port, uriBase+macStr+"/fw", true);

  //    http.setTimeout(2000);
  int httpStatus = http.GET();
  Serial.println("get returned " + String(httpStatus));
  if (httpStatus == 200)
  {

    contentLength = http.getSize();
    if (Update.begin(contentLength))
    {
      Serial.println("Starting Over-The-Air update. This may take some time to complete ...");

      
      uint32_t written = 0, total = 0, tried = 0;

      while (!Update.isFinished() && cl.connected())
      {
        size_t waited = _ota_timeout;
        size_t available = cl.available();
        while (!available && waited)
        {
          delay(1);
          waited -= 1;
          available = cl.available();
        }
        if (!waited)
        {
          if (written && tried++ < 3)
          {
            log_i("Try[%u]: %u", tried, written);
            if (!cl.printf("%u", written))
            {
              log_e("failed to respond");

              break;
            }
            continue;
          }
          log_e("Receive Failed");
          // if (_error_callback) {
          //     _error_callback(OTA_RECEIVE_ERROR);
          // }

          Update.abort();
          return;
        }
        if (!available)
        {
          log_e("No Data: %u", waited);

          break;
        }
        tried = 0;
        static uint8_t buf[1460];
        if (available > 1460)
        {
          available = 1460;
        }
        size_t r = cl.read(buf, available);
        if (r != available)
        {
          log_w("didn't read enough! %u != %u", r, available);
        }

        written = Update.write(buf, r);
        if (written > 0)
        {
          if (written != r)
          {
            log_w("didn't write enough! %u != %u", written, r);
          }
          if (!cl.printf("%u", written))
          {
            log_w("failed to respond");
          }
          total += written;
          //if(_progress_callback) {
          //    _progress_callback(total, _size);
          //}
        }
        else
        {
          log_e("Write ERROR: %s", Update.errorString());
        }
      }

      if (Update.canRollBack())
      {
        Update.rollBack();
        Serial.println("rolled back");
      }
    }
  }
  /*   if (Update.end())
      {
        if (Update.isFinished())
        {
          Serial.println("OTA update has successfully completed. Rebooting ...");
          ESP.restart();
        }
        else
        {
          Serial.println("Something went wrong! OTA update hasn't been finished properly.");
        }
      }
      else
      {
        Serial.println("An error Occurred. Error #: " + String(Update.getError()));
      }
    }
    else
    {
      Serial.println("There isn't enough space to start OTA update");
      cl.flush();
    }
  } */

  http.end();
}

void loop()
{
  Serial.println("-- V4 --");

  //pollFOTAUpdate("co2ampel.kgbvax.net",443,"/firmware-v4up.bin");

  delay(5000);
}