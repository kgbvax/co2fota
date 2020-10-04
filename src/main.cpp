#include <Arduino.h>
#include <M5Stack.h>
#include <ArduinoOTA.h>
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

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

static const char *TAG = "co2fota";

void setup()
{

  Serial.begin(115200);
  esp_log_level_set("*", ESP_LOG_DEBUG);
  Serial.println("setup begin");

  delay(1250);
  M5.begin();
  WiFi.begin(ssid, password);
  WiFi.setHostname("co2fota");
  WiFi.setAutoReconnect(true);

  int waitConnected=20;
  while(!WiFi.isConnected() && --waitConnected>0) {
    delay(500);
    Serial.print(".");
  }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("sketch MD5 "+EspClass().getSketchMD5());
  
  Serial.println("setup end");
}

// A helper function to extract header value from header
inline String getHeaderValue(String header, String headerName)
{
  return header.substring(strlen(headerName.c_str()));
}


void pollFOTAUpdate(String host,int port, String uri)
{
  static long int lastPoll=0;
   
  WiFiClientSecure cl;

  HTTPClient http;
  // Variables to validate firmware content
  volatile int contentLength = 0;
  volatile bool isValidContentType = false;

  //String base = "https://co2ampel.kgbvax.net/firmware.bin";

  Serial.println("poll "+host+" - "+uri);
  http.begin(cl, host, port, uri, true);

  //    http.setTimeout(2000);
  int httpStatus = http.GET();
  Serial.println("get returned " + String(httpStatus));
  if (httpStatus == 200)
  {

    contentLength = http.getSize();
    if (Update.begin(contentLength))
    {
      Serial.println("Starting Over-The-Air update. This may take some time to complete ...");
      size_t written = Update.writeStream(cl);
       
      if (written == contentLength)
      {
        Serial.println("Written : " + String(written) + " successfully");
      }
      else
      {
        Serial.println("Written only : " + String(written) + "/" + String(contentLength) + ". Retry?");
        // Retry??
      }

      
      if (Update.end())
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
  }
 
http.end();
}

void loop()
{
  Serial.println("-- V4 --");
  //pollFOTAUpdate("co2ampel.kgbvax.net",443,"/firmware-v4up.bin");
  
  delay(5000);
}