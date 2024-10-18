#include "Inkplate.h"
#include "htmlCode.h"
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <uri/UriBraces.h>
#include "Network.h" // Include network functions for quote retrieval

#define ssid "Greenlight"
#define pass "ageoflords303"
#define REFRESH_DELAY 6000 

Inkplate display(INKPLATE_1BIT);
WebServer server(80);
Network network; // Create network object for quote retrieval

IPAddress serverIP;
std::vector<String> texts;
std::vector<bool> checkedStatus;

char quote[256];
char author[64];
int len;
char date[64];

unsigned long lastSwitchTime = 0;
bool showTodoList = true;
const unsigned long displayInterval = 300000; // 5 minutes in milliseconds
unsigned long refreshTime;       // Time for measuring refresh in millis
int n = 0;

void setup() {
    Serial.begin(115200);
    display.begin();
    display.setTextWrap(true);

    // Initialize touchscreen
    if (!display.tsInit(true))
        Serial.println("Touchscreen init failed!");

    network.begin(ssid, pass);

    serverIP = WiFi.localIP();

    Serial.print("Local IP");
    Serial.print(serverIP);

    server.on("/", handleRoot);
    server.on(UriBraces("/string/{}"), handleString);
    server.begin();

    drawToDo(); // Initial display of to-do list
}


void loop() {
    server.handleClient();

    if (millis() - lastSwitchTime > displayInterval) {
        lastSwitchTime = millis();
        showTodoList = !showTodoList;

        if (showTodoList) {
            drawToDo();
           
        } else {
            if (network.getData(quote, author, &len, &display)) {
                drawQuote();
            }
        }
    }

    if (showTodoList) {
        for (int i = 0; i < texts.size(); ++i) {
          if (display.touchInArea(0, 500, display.width(), display.height())) {
                Serial.println("Touch occured");
            }
            
            if (display.touchInArea(0, 220 + 60 * i, display.width() - 100, 60)) {
                toggleCheckbox(i);
            }
            if (display.touchInArea(display.width() - 100, 220 + 60 * i, 100, 60)) {
                deleteTodoItem(i);
                delay(200); // Debounce delay
            }
        }
    }

    if ((unsigned long)(millis() - refreshTime) > REFRESH_DELAY)
    {
        displayTime();
        
        if (n > 9) // Check if you need to do full refresh or you can do partial update
        {
            display.display(true); // Do a full refresh
            n = 0;
        }
        else
        {
            display.partialUpdate(false, true); // Do partial update and keep e-papr power supply on
            n++;                                // Keep track on how many times screen has been partially updated
        }

        refreshTime = millis(); // Store current millis
    }

    delay(15);
}

void drawToDo() {
    display.clearDisplay();

    // Display current time
    displayTime();

    int startY = 110;

    display.setTextSize(2);
    display.setCursor(50, startY); // Adjusted position
    display.setTextSize(6);
    display.print("To do: ");
    
    display.fillRect(10, startY + 70, 580, 3, BLACK);

    int toDoYPosition = startY * 2 ; // Adjusted start position for displaying texts
    for (int i = 0; i < texts.size(); ++i) {
        if (toDoYPosition > 400) break;

        if (checkedStatus[i]) {
            display.fillCircle(60, toDoYPosition + 20, 20, BLACK);
        } else {
            display.drawCircle(60, toDoYPosition + 20, 20, BLACK);
        }

        display.setTextSize(5);
        display.setCursor(100, toDoYPosition);
        display.print(texts[i]);

         // Draw "X" button
        display.setTextSize(3);
        display.setCursor(display.width() - 60, toDoYPosition + 10 );
        display.print("X");
        
        toDoYPosition += 60;
    }
    
    display.display();
}


void drawQuote() {
    display.clearDisplay();

    // Display current time
    displayTime();

    int startX = 50;
    int startY = 150;
    int lineHeight = 48; // Adjust based on your font size

    display.setCursor(startX, startY);

    std::vector<String> words;
    char *word = strtok(quote, " ");
    while (word != nullptr) {
        words.push_back(String(word));
        word = strtok(nullptr, " ");
    }

    int currentRow = 0;
    String currentLine;

    for (const auto &word : words) {
        String tempLine = currentLine + (currentLine.isEmpty() ? "" : " ") + word;
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(tempLine.c_str(), startX, startY + (lineHeight * currentRow), &x1, &y1, &w, &h);

        if ((x1 + w) > 560) { // If the word doesn't fit in the current line
            // Print the current line and move to the next row
            display.print(currentLine);
            display.setCursor(startX, startY + (lineHeight * ++currentRow));
            currentLine = word; // Start new line with the current word
        } else {
            currentLine = tempLine; // Add word to the current line
        }
    }

    // Print the last line if there's any remaining text
    if (!currentLine.isEmpty()) {
        display.print(currentLine);
    }

    // Print author
    display.setCursor(startX, startY + (lineHeight * (++currentRow)));
    display.print("- ");
    display.println(author);

    display.display(); // Refresh screen with new content
}

// Function to print the current time at the top of the screen
void displayTime() {
    // Define timezone offset for EEST (UTC+3)
    int timezoneOffsetHours = 3;

    // Get the current time from the network
    network.getTime(date);

    // Convert the date string to a time_t structure
    struct tm timeinfo;
    strptime(date, "%a %b %d %H:%M:%S %Y", &timeinfo);

    // Adjust for timezone
    time_t localTime = mktime(&timeinfo) + timezoneOffsetHours * 3600;

    // Convert back to tm structure for display
    localtime_r(&localTime, &timeinfo);

    // Extract weekday and month abbreviations
    char weekday[4], month[4];
    strncpy(weekday, "SunMonTueWedThuFriSat" + (timeinfo.tm_wday * 3), 3);
    strncpy(month, "JanFebMarAprMayJunJulAugSepOctNovDec" + (timeinfo.tm_mon * 3), 3);
    weekday[3] = '\0';
    month[3] = '\0';

    // Format and display the local time and date
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%s, %02d %s, %02d:%02d",
             weekday, timeinfo.tm_mday, month, timeinfo.tm_hour, timeinfo.tm_min);

    // Clear the area where the time is displayed
    display.fillRect(display.width() / 2 - 220, 20, 480, 84, WHITE); // Adjust size as needed

    display.setTextSize(4);
    display.setCursor(display.width() / 2 - 220, 35);
    display.println(buffer);
}


void updateHTML() {
    server.send(200, "text/html", s);
}

void handleRoot() {
    updateHTML();
}

void handleString() {
    texts.push_back(server.arg(0));
    checkedStatus.push_back(false);
    updateHTML();
    drawToDo();
}

void toggleCheckbox(int index) {
    if (index < checkedStatus.size()) {
        checkedStatus[index] = !checkedStatus[index];
        drawToDo();
    }
}

void clearTexts() {
    texts.clear();
    checkedStatus.clear();
    drawToDo();
}

void deleteTodoItem(int index) {
  if (index < texts.size()) {
      texts.erase(texts.begin() + index);
      checkedStatus.erase(checkedStatus.begin() + index);
      drawToDo();
  }
}
