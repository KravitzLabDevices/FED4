#include "FED4.h"

void FED4::menu() {
    menuStart();
    menuProgram();
    menuMouseId();
    menuSex();
    menuStrain();
    menuAge();
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

void FED4::menuEnd() {
    click();
    clearDisplay();
    setCursor(15, 30);
    print("Menu saved...");
    setCursor(15, 51);
    print("Restarting!");
    refresh();
    resetJingle();
    esp_restart();
}
