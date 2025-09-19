#ifndef ESC_CONTROLLER_H
#define ESC_CONTROLLER_H

#include <Arduino.h>
#include <ESP32Servo.h>

class ESCController {
private:
    Servo esc;
    int pin;
    int minPulse;
    int maxPulse;
    bool initialized;
    float currentSpeed;

public:
    // Constructor with default PWM values for standard ESCs
    ESCController(int escPin, int minPulseWidth = 1000, int maxPulseWidth = 2000);
    
    // Initialize the ESC (includes calibration sequence)
    bool initialize();
    
    // Set motor speed (0.0 to 1.0, where 0 is stopped and 1 is full throttle)
    void setSpeed(float speed);
    
    // Stop the motor (convenience function that sets speed to 0)
    void stop();
    
    // Get current speed setting
    float getSpeed() const;
    
    // Check if ESC is initialized
    bool isInitialized() const;
    
    // Arm the ESC (some ESCs require this after power-on)
    void arm();
};

#endif // ESC_CONTROLLER_H
