#include "ESCController.h"

ESCController::ESCController(int escPin, int minPulseWidth, int maxPulseWidth) 
    : pin(escPin), minPulse(minPulseWidth), maxPulse(maxPulseWidth), 
      initialized(false), currentSpeed(0.0) {
}

bool ESCController::initialize() {
    // Attach the ESC to the pin with specified pulse width range
    esc.attach(pin, minPulse, maxPulse);
    
    // ESC calibration sequence
    // Most ESCs require seeing max then min throttle at power-on
    // esc.writeMicroseconds(maxPulse);
    delay(500); // Wait for ESC to register max throttle
    
    esc.writeMicroseconds(minPulse);
    delay(3000); // Wait for ESC to complete initialization (listen for beeps)
    
    // Some ESCs need a slight throttle bump to confirm arming
    esc.writeMicroseconds(minPulse + 50);
    delay(500);
    esc.writeMicroseconds(minPulse);
    delay(500);
    
    initialized = true;
    currentSpeed = 0.0;
    
    return true;
}

void ESCController::setSpeed(float speed) {
    if (!initialized) {
        return;
    }
    
    // Clamp speed between 0 and 1
    speed = constrain(speed, 0.0, 1.0);
    
    // Calculate pulse width based on speed
    int pulseWidth = minPulse + (speed * (maxPulse - minPulse));
    
    // Send the pulse width to the ESC
    esc.writeMicroseconds(pulseWidth);
    
    currentSpeed = speed;
}

void ESCController::stop() {
    setSpeed(0.0);
}

float ESCController::getSpeed() const {
    return currentSpeed;
}

bool ESCController::isInitialized() const {
    return initialized;
}

void ESCController::arm() {
    if (!initialized) {
        return;
    }
    
    // Standard arming sequence
    esc.writeMicroseconds(minPulse);
    delay(1000);
    
    // Some ESCs need a small throttle bump
    esc.writeMicroseconds(minPulse + 50);
    delay(100);
    esc.writeMicroseconds(minPulse);
    
    Serial.println("ESC armed!");
}
