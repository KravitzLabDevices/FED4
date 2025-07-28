#include "FED4.h"

// Helper function to get days in a month (accounting for leap years)
int getDaysInMonth(int year, int month) {
    const int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
        return 29; // Leap year February
    }
    return daysInMonth[month - 1];
}

void FED4::menu() {
    menuStart();
    menuProgram();
    menuMouseId();
    menuSex();
    menuStrain();
    menuAge();
    menuAudio();
    menuRTC();
    menuEnd();
}

void FED4::menuStart() {
    Serial.println("********** MENU **********");

    //clear display area below mouse ID
    fillRect(0, 65, 144, 145, DISPLAY_WHITE);   
    // // draw line to split on screen text 
    drawLine(0,59,168,59, DISPLAY_WHITE);  
    drawLine(0,60,168,60, DISPLAY_WHITE);   
    refresh();
    displayDateTime();
    displayTask();
    displayMouseId();
    displaySex();
    displayStrain();
    displayAge();
    displayAudio();
    refresh();    
    delay (500); // more debounce
}

void FED4::menuProgram() {
    displayTask();
    refresh();
}

void FED4::menuMouseId() {
    displayMouseId();
    refresh();

    //display mouse ID
      // Get current mouseId
    mouseId = getMetaValue("subject", "id");
    
    String numericId = "";
    for (char c : mouseId) {
        if (isdigit(c)) {
            numericId += c;
        }
    }
    // Pad with leading zeros if less than 4 digits
    while (numericId.length() < 4) {
        numericId = "0" + numericId;
    }
    // Truncate to 4 digits if longer
    if (numericId.length() > 4) {
        numericId = numericId.substring(numericId.length() - 4);
    }

    int currentId = numericId.toInt(); // Convert to integer for math
    bool menuActive = true;
    while (menuActive) {
        
        fillRect(82, 40, 80, 16, DISPLAY_WHITE); // Clear area for mouse ID
        refresh();
        delay(100);
        displayMouseId();
        refresh();
        delay (100);

        // Wait for button press
        if (digitalRead(BUTTON_1) == HIGH) {
            currentId++; // Increment ID
            setMouseId(String(currentId)); // Update ID immediately
            mouseId = currentId;

            Serial.print("Current Mouse ID: ");
            Serial.println(currentId);
        }
        
        else if (digitalRead(BUTTON_3) == HIGH) {
            currentId = max(0, currentId - 1); // Decrement ID but don't go below 0
            setMouseId(String(currentId)); // Update ID immediately
            mouseId = currentId;    
            Serial.print("Current Mouse ID: ");
            Serial.println(currentId);
        }
        
        else if (digitalRead(BUTTON_2) == HIGH) {
            click();
            // Save the new ID and exit
            menuActive = false;
            displayMouseId();
            delay(200); // Debounce
        }
    }  
}

void FED4::menuSex() {
    // Get current sex from meta.json
    String currentSex = getMetaValue("subject", "sex");
    if (currentSex.length() == 0) {
        currentSex = "male"; // Default to male if not set
    }

    bool menuActive = true;
    while (menuActive) {
        fillRect(48, 58, 80, 16, DISPLAY_WHITE); // Clear area for sex display
        refresh();
        delay(100);
        displaySex();
        refresh();
        delay(100);

        // Wait for button press
        if (digitalRead(BUTTON_1) == HIGH || digitalRead(BUTTON_3) == HIGH) {
            // Toggle between male and female
            if (currentSex == "male") {
                currentSex = "female";
            } else {
                currentSex = "male";
            }
            setSex(currentSex); // Update sex immediately
            sex = currentSex;

            Serial.print("Current Sex: ");
            Serial.println(currentSex);
        }
        else if (digitalRead(BUTTON_2) == HIGH) {
            click();
            // Save and exit
            menuActive = false;
            delay(200); // Debounce
            displaySex();
        }
    }

}

void FED4::menuStrain() {
    // Get current strain from meta.json
    String currentStrain = getMetaValue("subject", "strain");
    if (currentStrain.length() == 0) {
        currentStrain = "C57BL/6"; // Default strain if not set
    }

    // Define available strains (since we can't read from JSON array)
    const char* strainOptions[] = {"C57BL/6", "CD1", "BALB/c", "Other"};
    const int numStrains = 4;
    int currentStrainIndex = 0;

    // Find index of current strain
    for (int i = 0; i < numStrains; i++) {
        if (String(strainOptions[i]) == currentStrain) {
            currentStrainIndex = i;
            break;
        }
    }

    bool menuActive = true;
    while (menuActive) {
        fillRect(60, 76, 160, 16, DISPLAY_WHITE);
        refresh();
        delay(100);
        displayStrain();
        refresh();
        delay(100);

        if (digitalRead(BUTTON_1) == HIGH) {
            currentStrainIndex = (currentStrainIndex - 1 + numStrains) % numStrains;
            currentStrain = strainOptions[currentStrainIndex];
            setMetaValue("subject", "strain", strainOptions[currentStrainIndex]);
            strain = currentStrain;

            Serial.print("Current Strain: ");
            Serial.println(currentStrain);
        }
        else if (digitalRead(BUTTON_3) == HIGH) {
            currentStrainIndex = (currentStrainIndex + 1) % numStrains;
            currentStrain = strainOptions[currentStrainIndex];
            setMetaValue("subject", "strain", strainOptions[currentStrainIndex]);
            strain = currentStrain;

            Serial.print("Current Strain: ");
            Serial.println(currentStrain);
        }
        else if (digitalRead(BUTTON_2) == HIGH) {
            click();
            menuActive = false;
            delay(200);
            displayStrain();
        }
    }
}

void FED4::menuAge() {
    String currentAge = getMetaValue("subject", "age");
    if (currentAge.isEmpty()) {
        currentAge = "8"; // Default age if not set
    }
    int currentAgeValue = currentAge.toInt();
    const int maxAge = 36;

    bool menuActive = true;
    while (menuActive) {
        fillRect(42, 94, 160, 16, DISPLAY_WHITE);
        refresh();
        delay(100);
        displayAge();
        refresh();
        delay(100);

        if (digitalRead(BUTTON_1) == HIGH) {
            currentAgeValue = max(4, currentAgeValue + 1); // Don't go below 4 weeks
            setMetaValue("subject", "age", String(currentAgeValue).c_str());
            age = String(currentAgeValue);

            Serial.print("Current Age: ");
            Serial.println(currentAgeValue);
        }
        else if (digitalRead(BUTTON_3) == HIGH) {
            currentAgeValue = min(maxAge, currentAgeValue - 1); // Don't exceed maxAge
            setMetaValue("subject", "age", String(currentAgeValue).c_str());
            age = String(currentAgeValue);

            Serial.print("Current Age: ");
            Serial.println(currentAgeValue);
        }
        else if (digitalRead(BUTTON_2) == HIGH) {
            menuActive = false;
        }
    }
}

void FED4::menuAudio() {
    // Wait for all buttons to be released before starting
    while (digitalRead(BUTTON_1) == HIGH || digitalRead(BUTTON_2) == HIGH || digitalRead(BUTTON_3) == HIGH) {
        delay(50);
    }
    delay(200); // Additional debounce delay
    
    bool menuActive = true;
    while (menuActive) {
        fillRect(48, 112, 80, 16, DISPLAY_WHITE); // Clear area for audio display
        refresh();
        delay(100);
        displayAudio();
        refresh();
        delay(100);

        // Wait for button press
        if (digitalRead(BUTTON_1) == HIGH || digitalRead(BUTTON_3) == HIGH) {
            // Toggle audio on/off
            if (audioSilenced) {
                unsilence();
                audioSilenced = false;
            } else {
                silence();
                audioSilenced = true;
            }
            
            Serial.print("Audio: ");
            Serial.println(audioSilenced ? "Off" : "On");
            delay(200); // Debounce
        }
        else if (digitalRead(BUTTON_2) == HIGH) {
            click();
            // Save and exit
            menuActive = false;
            delay(200); // Debounce
            displayAudio();
        }
    }
}

void FED4::menuRTC() {
    // Get current time from RTC
    DateTime current = rtc.now();
    int currentYear = current.year();
    int currentMonth = current.month();
    int currentDay = current.day();
    int currentHour = current.hour();
    int currentMinute = current.minute();
    
    // Wait for all buttons to be released before starting
    while (digitalRead(BUTTON_1) == HIGH || digitalRead(BUTTON_2) == HIGH || digitalRead(BUTTON_3) == HIGH) {
        delay(50);
    }
    delay(200); // Additional debounce delay
    
    bool menuActive = true;
    bool timeVisible = true; // For blinking effect
    unsigned long lastBlink = 0;
    const unsigned long blinkInterval = 250; // 100ms blink interval
    
    while (menuActive) {
        unsigned long currentTime = millis();
        
        // Handle blinking effect
        if (currentTime - lastBlink >= blinkInterval) {
            timeVisible = !timeVisible;
            lastBlink = currentTime;
            
            // Clear the entire date-time area and redraw
            fillRect(0, 146, 144, 22, DISPLAY_BLACK);
            refresh();
            
            if (timeVisible) {
                // Display the full date and time
                setFont(&Org_01);
                setTextSize(2);
                setTextColor(DISPLAY_WHITE);
                
                // Display date
                setCursor(5, 160);
                char dateStr[9];
                snprintf(dateStr, sizeof(dateStr), "%02d.%02d.%02d", 
                         currentMonth, currentDay, currentYear - 2000);
                print(dateStr);
                
                // Display time
                setCursor(94, 160);
                char timeStr[6];
                snprintf(timeStr, sizeof(timeStr), "%02d:%02d", currentHour, currentMinute);
                print(timeStr);
                refresh();
            }
        }
        
        // Handle button presses with different increment speeds
        if (digitalRead(BUTTON_1) == HIGH) {
            unsigned long pressStart = millis();
            unsigned long lastUpdate = 0;
            int increment = 1; // Start with 1 minute increment
            
            // Update while button is held
            while (digitalRead(BUTTON_1) == HIGH) {
                unsigned long pressDuration = millis() - pressStart;
                unsigned long currentTime = millis();
                
                // Determine increment based on press duration
                if (pressDuration > 2000) { // 2+ seconds = 1 hour increment
                    increment = 60;
                } else if (pressDuration > 500) { // 0.5+ seconds = 10 minute increment
                    increment = 10;
                }
                
                // Update time every 200ms while button is held
                if (currentTime - lastUpdate >= 200) {
                    // Apply the increment
                    currentMinute += increment;
                    
                    // Handle rollover
                    while (currentMinute >= 60) {
                        currentMinute -= 60;
                        currentHour++;
                        if (currentHour >= 24) {
                            currentHour = 0;
                            // Roll over to next day
                            currentDay++;
                            if (currentDay > getDaysInMonth(currentYear, currentMonth)) {
                                currentDay = 1;
                                currentMonth++;
                                if (currentMonth > 12) {
                                    currentMonth = 1;
                                    currentYear++;
                                }
                            }
                        }
                    }
                    
                    // Update RTC immediately
                    DateTime newTime = DateTime(currentYear, currentMonth, currentDay, 
                                              currentHour, currentMinute, current.second());
                    rtc.adjust(newTime);
                    
                    // Update display in real-time
                    fillRect(0, 146, 144, 22, DISPLAY_BLACK);
                    setFont(&Org_01);
                    setTextSize(2);
                    setTextColor(DISPLAY_WHITE);
                    
                    // Display date
                    setCursor(5, 160);
                    char dateStr[9];
                    snprintf(dateStr, sizeof(dateStr), "%02d.%02d.%02d", 
                             currentMonth, currentDay, currentYear - 2000);
                    print(dateStr);
                    
                    // Display time
                    setCursor(94, 160);
                    char timeStr[6];
                    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", currentHour, currentMinute);
                    print(timeStr);
                    refresh();
                    
                    // Audio feedback for each update
                    click();
                    
                    lastUpdate = currentTime;
                }
                
                delay(50);
            }
            
            Serial.printf("Time set to: %04d-%02d-%02d %02d:%02d\n", 
                         currentYear, currentMonth, currentDay, currentHour, currentMinute);
            delay(200); // Debounce
        }
        else if (digitalRead(BUTTON_3) == HIGH) {
            unsigned long pressStart = millis();
            unsigned long lastUpdate = 0;
            int decrement = 1; // Start with 1 minute decrement
            
            // Update while button is held
            while (digitalRead(BUTTON_3) == HIGH) {
                unsigned long pressDuration = millis() - pressStart;
                unsigned long currentTime = millis();
                
                // Determine decrement based on press duration
                if (pressDuration > 2000) { // 2+ seconds = 1 hour decrement
                    decrement = 60;
                } else if (pressDuration > 500) { // 0.5+ seconds = 10 minute decrement
                    decrement = 10;
                }
                
                // Update time every 200ms while button is held
                if (currentTime - lastUpdate >= 200) {
                    // Apply the decrement
                    currentMinute -= decrement;
                    
                    // Handle rollunder
                    while (currentMinute < 0) {
                        currentMinute += 60;
                        currentHour--;
                        if (currentHour < 0) {
                            currentHour = 23;
                            // Roll under to previous day
                            currentDay--;
                            if (currentDay < 1) {
                                currentMonth--;
                                if (currentMonth < 1) {
                                    currentMonth = 12;
                                    currentYear--;
                                }
                                currentDay = getDaysInMonth(currentYear, currentMonth);
                            }
                        }
                    }
                    
                    // Update RTC immediately
                    DateTime newTime = DateTime(currentYear, currentMonth, currentDay, 
                                              currentHour, currentMinute, current.second());
                    rtc.adjust(newTime);
                    
                    // Update display in real-time
                    fillRect(0, 146, 144, 22, DISPLAY_BLACK);
                    setFont(&Org_01);
                    setTextSize(2);
                    setTextColor(DISPLAY_WHITE);
                    
                    // Display date
                    setCursor(5, 160);
                    char dateStr[9];
                    snprintf(dateStr, sizeof(dateStr), "%02d.%02d.%02d", 
                             currentMonth, currentDay, currentYear - 2000);
                    print(dateStr);
                    
                    // Display time
                    setCursor(94, 160);
                    char timeStr[6];
                    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", currentHour, currentMinute);
                    print(timeStr);
                    refresh();
                    
                    // Audio feedback for each update
                    click();
                    
                    lastUpdate = currentTime;
                }
                
                delay(50);
            }
            
            Serial.printf("Time set to: %04d-%02d-%02d %02d:%02d\n", 
                         currentYear, currentMonth, currentDay, currentHour, currentMinute);
            delay(200); // Debounce
        }
        else if (digitalRead(BUTTON_2) == HIGH) {
            click();
            // Save and exit - time is already saved to RTC
            menuActive = false;
            delay(200); // Debounce
            
            // Show final date and time without blinking
            fillRect(0, 146, 144, 22, DISPLAY_BLACK);
            setFont(&Org_01);
            setTextSize(2);
            setTextColor(DISPLAY_WHITE);
            
            // Display date
            setCursor(5, 160);
            char dateStr[9];
            snprintf(dateStr, sizeof(dateStr), "%02d.%02d.%02d", 
                     currentMonth, currentDay, currentYear - 2000);
            print(dateStr);
            
            // Display time
            setCursor(94, 160);
            char timeStr[6];
            snprintf(timeStr, sizeof(timeStr), "%02d:%02d", currentHour, currentMinute);
            print(timeStr);
            refresh();
        }
        
        delay(50); // Small delay to prevent excessive CPU usage
    }
}

void FED4::menuEnd() {
    click();    
    // Fill display with black background
    fillRect(0, 0, 144, 168, DISPLAY_BLACK);
    refresh();
    // Set up for white text on black background
    setFont(&FreeSans9pt7b);
    setTextSize(1);
    setTextColor(DISPLAY_WHITE);
    setCursor(15, 30);
    print("Menu saved.");
    setCursor(15, 51);
    print("Restarting...");
    refresh();
    delay(1000); // Show the message for 1 second before restarting
    resetJingle();
    esp_restart();
}
