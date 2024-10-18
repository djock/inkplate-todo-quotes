/*
    Network.cpp
    Inkplate Arduino library
    David Zovko, Borna Biro, Denis Vajak, Zvonimir Haramustek @ e-radionica.com
    September 24, 2020
    https://github.com/e-radionicacom/Inkplate-6-Arduino-library

    For support, please reach over forums: forum.e-radionica.com/en
    For more info about the product, please check: www.inkplate.io

    This code is released under the GNU Lesser General Public License v3.0:
   https://www.gnu.org/licenses/lgpl-3.0.en.html Please review the LICENSE file included with this example. If you have
   any questions about licensing, please contact techsupport@e-radionica.com Distributed as-is; no warranty is given.
*/

#include "Network.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>



void Network::begin(char * ssid, char * pass)
{
    // Initiating wifi, like in BasicHttpClient example
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    int cnt = 0;
    Serial.print(F("Waiting for WiFi to connect..."));
    while ((WiFi.status() != WL_CONNECTED))
    {
        Serial.print(F("."));
        delay(1000);
        ++cnt;

        if (cnt == 20)
        {
            Serial.println("Can't connect to WIFI, restarting");
            delay(100);
            ESP.restart();
        }
    }
    Serial.println(F(" connected"));

    // Find and print internet time, the timezone will be added later
    setTime();
}

// Gets time from ntp server
void Network::getTime(char *timeStr, long offSet, int timeZone)
{
    // Get seconds since 1.1.1970. and add timezone
    time_t nowSecs = time(nullptr) + (long)timeZone * 3600L + offSet;

    // Used to store time
    struct tm timeinfo;
    gmtime_r(&nowSecs, &timeinfo);

    // Copies time string into timeStr
    strcpy(timeStr, asctime(&timeinfo));
}

bool Network::getData(char *quote, char *author, int *len, Inkplate * display)
{
    bool f = 0;

    // If not connected to wifi reconnect wifi
    if (WiFi.status() != WL_CONNECTED)
    {
        WiFi.reconnect();

        delay(5000);

        int cnt = 0;
        Serial.println(F("Waiting for WiFi to reconnect..."));
        while ((WiFi.status() != WL_CONNECTED))
        {
            // Prints a dot every second that wifi isn't connected
            Serial.print(F("."));
            delay(1000);
            ++cnt;

            if (cnt == 7)
            {
                Serial.println("Can't connect to WIFI, restart initiated.");
                delay(100);
                ESP.restart();
            }
        }
    }

    // Wake up if sleeping and save inital state
    bool sleep = WiFi.getSleep();
    WiFi.setSleep(false);

    // For HTTPS
    WiFiClientSecure client;
    client.setInsecure();

    // Http object used to make get request
    HTTPClient http;

    http.getStream().setTimeout(10);
    http.getStream().flush();

    // Initiate http
    char link[] = "https://api.quotable.io/random?maxLength=175";
    http.begin(client, link);

    // Dynamic Json from ArduinoJson library
    DynamicJsonDocument doc(4096);

    Serial.println("Fetching data");

    // Actually do request  
    int httpCode = http.GET();
    if (httpCode == 200)
    {
        while (http.getStream().available() && http.getStream().peek() != '{')
            (void)http.getStream().read();

        // Try parsing JSON object
        DeserializationError error = deserializeJson(doc, http.getStream());

        if (error)
        {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.c_str());
            f = 1;
        }
        else
        {
            // Set all data got from internet using formatTemp and formatWind defined above
            // This part relies heavily on ArduinoJson library

            Serial.println("Success");

            const char *buff1 = doc["content"];
            strcpy(quote, buff1);

            const char *buff2 = doc["author"];
            strcpy(author, buff2);

            *len = doc["length"];

            // Save our data to data pointer from main file
            f = 0;
        }
    }
    else if (httpCode == 404)
    {
        // Coin id not found
        display->clearDisplay();
        display->setCursor(5, 5);
        display->setTextSize(1);
        display->println(F("Info has not been found!"));
        display->display();
        while (1)
            ;
    }
    else
    {
        f = 1;
    }

    // Clear document and end http
    doc.clear();
    http.end();

    // Return to initial state
    WiFi.setSleep(sleep);

    return !f;
}

// Find internet time
void Network::setTime()
{
    // Used for setting correct time
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    Serial.print(F("Waiting for NTP time sync: "));
    time_t nowSecs = time(nullptr);
    while (nowSecs < 8 * 3600 * 2)
    {
        delay(500);
        Serial.print(F("."));
        yield();
        nowSecs = time(nullptr);
    }
    Serial.println();

    // Used to store time info
    struct tm timeinfo;
    gmtime_r(&nowSecs, &timeinfo);

    // Print the current time without adding a timezone
    Serial.print(F("Current time: "));
    Serial.print(asctime(&timeinfo));
}
