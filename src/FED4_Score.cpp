#include "FED4.h"

/*
  Pong Game for FED4
  A classic pong game that runs when mouseId == 99
  Play area limited to bottom 100 pixels of screen
  
  Controls:
  - Button 1 (Left): Move paddle up
  - Button 3 (Right): Move paddle down
  - Button 2 (Center): Pause/Resume or restart after game over
*/

void FED4::pong() {
    // Set mouseId to 100 immediately to prevent auto-launch on next boot
    setMouseId("100");
    
    // Game constants
    const int PADDLE_WIDTH = 4;
    const int PADDLE_HEIGHT = 40;  // Doubled from 20 to 40
    const int BALL_SIZE = 4;
    const int SCREEN_WIDTH = 144;
    const int SCREEN_HEIGHT = 168;
    const int PLAY_AREA_TOP = 58;  // Top of play area (leaves top 58 pixels for score/info)
    const int PLAY_AREA_HEIGHT = 110; // Bottom 110 pixels for gameplay
    const int SCORE_TO_WIN = 10;
    
    // Game variables
    static bool firstRun = true;
    static float ballX, ballY;
    static float ballSpeedX, ballSpeedY;
    static int paddleY;
    static int cpuPaddleY;
    static int playerScore;
    static int cpuScore;
    static bool gameOver;
    static bool paused;
    static unsigned long lastUpdateTime;
    static float cpuReactionDelay; // CPU reaction time
    static int cpuTargetY; // Where CPU is trying to move
    
    // Initialize game on first run
    if (firstRun) {
        clearDisplay();
        setTextColor(DISPLAY_BLACK);
        setTextSize(2);
        setCursor(5, 70);
        print("WELCOME #99!");
        setCursor(15, 100);
        print("Press a");
        setCursor(15, 115);
        print("button");
        refresh();
        
        // Wait for button press
        while (digitalRead(BUTTON_1) == LOW && 
               digitalRead(BUTTON_2) == LOW && 
               digitalRead(BUTTON_3) == LOW) {
            delay(50);
        }
        delay(200); // Debounce
        
        // Wait for button release
        while (digitalRead(BUTTON_1) == HIGH || 
               digitalRead(BUTTON_2) == HIGH || 
               digitalRead(BUTTON_3) == HIGH) {
            delay(50);
        }
        
        firstRun = false;
    }
    
    // Reset game state
    ballX = SCREEN_WIDTH / 2;
    ballY = PLAY_AREA_TOP + PLAY_AREA_HEIGHT / 2;
    ballSpeedX = 1.2;  // Slower starting speed (was 2.0)
    ballSpeedY = 0.8;  // Slower starting speed (was 1.5)
    paddleY = PLAY_AREA_TOP + PLAY_AREA_HEIGHT / 2 - PADDLE_HEIGHT / 2;
    cpuPaddleY = PLAY_AREA_TOP + PLAY_AREA_HEIGHT / 2 - PADDLE_HEIGHT / 2;
    cpuTargetY = cpuPaddleY;
    playerScore = 0;
    cpuScore = 0;
    gameOver = false;
    paused = false;
    lastUpdateTime = millis();
    cpuReactionDelay = 0;
    
    // Main game loop
    while (!gameOver) {
        unsigned long currentTime = millis();
        float deltaTime = (currentTime - lastUpdateTime) / 16.0; // Normalize to ~60fps
        
        if (deltaTime < 0.5) deltaTime = 0.5;
        if (deltaTime > 3.0) deltaTime = 3.0;
        
        // Check for pause button
        if (digitalRead(BUTTON_2) == HIGH) {
            paused = !paused;
            if (paused) {
                hapticBuzz(50);
            }
            delay(200); // Debounce
            while (digitalRead(BUTTON_2) == HIGH) {
                delay(50);
            }
        }
        
        if (!paused) {
            // Player paddle control
            if (digitalRead(BUTTON_1) == HIGH) {
                paddleY -= 4;
                if (paddleY < PLAY_AREA_TOP) paddleY = PLAY_AREA_TOP;
            }
            if (digitalRead(BUTTON_3) == HIGH) {
                paddleY += 4;
                if (paddleY > PLAY_AREA_TOP + PLAY_AREA_HEIGHT - PADDLE_HEIGHT) {
                    paddleY = PLAY_AREA_TOP + PLAY_AREA_HEIGHT - PADDLE_HEIGHT;
                }
            }
            
            // Update ball position
            ballX += ballSpeedX * deltaTime;
            ballY += ballSpeedY * deltaTime;
            
            // Ball collision with top and bottom of play area
            if (ballY <= PLAY_AREA_TOP || ballY >= PLAY_AREA_TOP + PLAY_AREA_HEIGHT - BALL_SIZE) {
                ballSpeedY = -ballSpeedY;
                ballY = constrain(ballY, PLAY_AREA_TOP, PLAY_AREA_TOP + PLAY_AREA_HEIGHT - BALL_SIZE);
                click();
            }
            
            // Ball collision with player paddle (left side)
            if (ballX <= PADDLE_WIDTH + 2 && 
                ballY + BALL_SIZE >= paddleY && 
                ballY <= paddleY + PADDLE_HEIGHT) {
                ballSpeedX = abs(ballSpeedX); // Ensure it goes right
                ballX = PADDLE_WIDTH + 2;
                
                // Add spin based on where ball hits paddle
                float hitPos = (ballY + BALL_SIZE/2 - paddleY) / PADDLE_HEIGHT;
                ballSpeedY = (hitPos - 0.5) * 4;
                
                // Increase speed slightly
                ballSpeedX *= 1.05;
                ballSpeedY *= 1.05;
                
                // Cap max speed
                if (abs(ballSpeedX) > 5) ballSpeedX = (ballSpeedX > 0) ? 5 : -5;
                if (abs(ballSpeedY) > 5) ballSpeedY = (ballSpeedY > 0) ? 5 : -5;
                
                hapticBuzz(30);
            }
            
            // Ball collision with CPU paddle (right side)
            if (ballX >= SCREEN_WIDTH - PADDLE_WIDTH - BALL_SIZE - 2 && 
                ballY + BALL_SIZE >= cpuPaddleY && 
                ballY <= cpuPaddleY + PADDLE_HEIGHT) {
                ballSpeedX = -abs(ballSpeedX); // Ensure it goes left
                ballX = SCREEN_WIDTH - PADDLE_WIDTH - BALL_SIZE - 2;
                
                // Add spin
                float hitPos = (ballY + BALL_SIZE/2 - cpuPaddleY) / PADDLE_HEIGHT;
                ballSpeedY = (hitPos - 0.5) * 4;
                
                // Increase speed
                ballSpeedX *= 1.05;
                ballSpeedY *= 1.05;
                
                // Cap max speed
                if (abs(ballSpeedX) > 5) ballSpeedX = (ballSpeedX > 0) ? 5 : -5;
                if (abs(ballSpeedY) > 5) ballSpeedY = (ballSpeedY > 0) ? 5 : -5;
                
                click();
            }
            
            // CPU AI - tracks ball with some delay and imperfection
            cpuReactionDelay -= deltaTime;
            if (cpuReactionDelay <= 0) {
                cpuTargetY = ballY - PADDLE_HEIGHT / 2 + random(-10, 10); // Add some randomness
                cpuReactionDelay = 8 + random(0, 5); // Reaction delay
            }
            
            // Move CPU paddle towards target
            if (cpuPaddleY < cpuTargetY - 2) {
                cpuPaddleY += 2;
            } else if (cpuPaddleY > cpuTargetY + 2) {
                cpuPaddleY -= 2;
            }
            
            // Keep CPU paddle in bounds
            if (cpuPaddleY < PLAY_AREA_TOP) cpuPaddleY = PLAY_AREA_TOP;
            if (cpuPaddleY > PLAY_AREA_TOP + PLAY_AREA_HEIGHT - PADDLE_HEIGHT) {
                cpuPaddleY = PLAY_AREA_TOP + PLAY_AREA_HEIGHT - PADDLE_HEIGHT;
            }
            
            // Scoring
            if (ballX <= 0) {
                cpuScore++;
                lowBeep();
                hapticDoubleBuzz(100);
                
                // Reset ball
                ballX = SCREEN_WIDTH / 2;
                ballY = PLAY_AREA_TOP + PLAY_AREA_HEIGHT / 2;
                ballSpeedX = 1.2;  // Slower starting speed
                ballSpeedY = random(-1, 2) * 0.5;
                delay(500);
            }
            
            if (ballX >= SCREEN_WIDTH - BALL_SIZE) {
                playerScore++;
                highBeep();
                hapticBuzz(50);
                
                // Reset ball
                ballX = SCREEN_WIDTH / 2;
                ballY = PLAY_AREA_TOP + PLAY_AREA_HEIGHT / 2;
                ballSpeedX = -1.2;  // Slower starting speed
                ballSpeedY = random(-1, 2) * 0.5;
                delay(500);
            }
            
            // Check for win condition
            if (playerScore >= SCORE_TO_WIN || cpuScore >= SCORE_TO_WIN) {
                gameOver = true;
            }
        }
        
        // Draw everything
        clearDisplay();
        
        // Draw scores at top (above play area)
        setTextSize(3);
        setTextColor(DISPLAY_BLACK);
        setCursor(20, 20);
        print(playerScore);
        setCursor(100, 20);
        print(cpuScore);
        
        // Draw play area boundary line
        drawLine(0, PLAY_AREA_TOP, SCREEN_WIDTH, PLAY_AREA_TOP, DISPLAY_BLACK);
        
        // Draw center line in play area
        for (int y = PLAY_AREA_TOP + 4; y < PLAY_AREA_TOP + PLAY_AREA_HEIGHT; y += 8) {
            fillRect(SCREEN_WIDTH/2 - 1, y, 2, 4, DISPLAY_BLACK);
        }
        
        // Draw paddles
        fillRect(2, paddleY, PADDLE_WIDTH, PADDLE_HEIGHT, DISPLAY_BLACK);
        fillRect(SCREEN_WIDTH - PADDLE_WIDTH - 2, cpuPaddleY, PADDLE_WIDTH, PADDLE_HEIGHT, DISPLAY_BLACK);
        
        // Draw ball
        fillRect((int)ballX, (int)ballY, BALL_SIZE, BALL_SIZE, DISPLAY_BLACK);
        
        // Draw pause indicator
        if (paused) {
            setTextSize(2);
            setCursor(35, 50);
            print("PAUSED");
        }
        
        refresh();
        
        lastUpdateTime = currentTime;
        delay(16); // ~60fps
    }
    
    // Game over screen
    clearDisplay();
    setTextSize(2);
    setTextColor(DISPLAY_BLACK);
    
    if (playerScore > cpuScore) {
        setCursor(25, 60);
        print("YOU WIN!");
        hapticTripleBuzz(100);
        resetJingle();
    } else {
        setCursor(25, 60);
        print("YOU LOSE");
        hapticDoubleBuzz(200);
        lowBeep();
        delay(100);
        lowBeep();
    }
    
    setTextSize(2);
    setCursor(10, 90);
    print("Score: ");
    print(playerScore);
    print(" - ");
    print(cpuScore);
    
    setCursor(10, 110);
    print("Press button");
    setCursor(10, 125);
    print("to play again");
    
    refresh();
    
    // Wait for button press
    while (digitalRead(BUTTON_1) == LOW && 
           digitalRead(BUTTON_2) == LOW && 
           digitalRead(BUTTON_3) == LOW) {
        delay(50);
    }
    
    delay(200); // Debounce
    
    // Wait for button release
    while (digitalRead(BUTTON_1) == HIGH || 
           digitalRead(BUTTON_2) == HIGH || 
           digitalRead(BUTTON_3) == HIGH) {
        delay(50);
    }
    
    // Restart game
    firstRun = false;
    pong();
}

