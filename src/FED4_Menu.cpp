#include "FED4.h"

void FED4::menu() {
    startMenu();
    menuMouseId();
}

void FED4::startMenu() {
    Serial.println("********** MENU **********");

    //clear display area below mouse ID
    fillRect(0, 65, 144, 145, DISPLAY_WHITE);    
    displayTaskandMouseId();
    refresh();
    
    //wait for button 3 to be released
    while (digitalRead(BUTTON_3) == LOW) {
        delay(10); // Small delay to prevent busy waiting
    }

    displayTaskandMouseId();
    refresh();
    
}

void FED4::menuMouseId() {

    //display "Set MouseID"
    setFont(&FreeSans9pt7b);
    setTextSize(1);
    setTextColor(DISPLAY_BLACK);
    setCursor(20, 100);
    print("Set MouseID");
    displayTaskandMouseId();
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
        
        // Wait for button press
        if (digitalRead(BUTTON_1) == HIGH) {
            currentId++; // Increment ID
            setMouseId(String(currentId)); // Update ID immediately
            mouseId = currentId;

            displayTaskandMouseId();
            refresh();
            delay(200); // Debounce
            Serial.print("Current Mouse ID: ");
            Serial.println(currentId);
        }
        
        else if (digitalRead(BUTTON_3) == HIGH) {
            currentId = max(0, currentId - 1); // Decrement ID but don't go below 0
            setMouseId(String(currentId)); // Update ID immediately
            mouseId = currentId;    
            displayTaskandMouseId();
            refresh();  
            delay(200); // Debounce
            Serial.print("Current Mouse ID: ");
            Serial.println(currentId);
        }
        
        else if (digitalRead(BUTTON_2) == HIGH) {
            // Save the new ID and exit
            menuActive = false;
            delay(200); // Debounce
            resetJingle();
            esp_restart();
        }
    }  
}

