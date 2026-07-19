#include <Stepper.h>


const int STEPS_PER_REV = 2048;  // 28BYJ-48 has 2048 steps/rev (with gear reduction)

// Pins connected to ULN2003 driver (IN1, IN2, IN3, IN4)
Stepper stepper(STEPS_PER_REV, 8, 10, 9, 11);

// Movement parameters
const int STEPS_FOR_180_DEG = STEPS_PER_REV / 2;
int currentStepPosition = 0;
int rpm = 2; // Start with slow speed

// Position targets (in steps)
const int POSITIONS[] = {
  0,                          // 0°
  STEPS_FOR_180_DEG / 4,      // 45°
  STEPS_FOR_180_DEG / 2,      // 90°
  STEPS_FOR_180_DEG * 3 / 4,  // 135°
  STEPS_FOR_180_DEG           // 180°
};

void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for serial connection
  
  Serial.println("Stepper Motor Test");
  Serial.println("------------------");
  
  // Initialize motor
  stepper.setSpeed(rpm);
  Serial.print("Initialized with ");
  Serial.print(STEPS_PER_REV);
  Serial.println(" steps/revolution");
  Serial.print("Moving at ");
  Serial.print(rpm);
  Serial.println(" RPM");
  Serial.println("Beginning main sequence...");
}

void loop() {
  for (int i = 0; i < sizeof(POSITIONS)/sizeof(POSITIONS[0]); i++) {
    moveToPosition(POSITIONS[i]);
    printPosition(POSITIONS[i]);
    delay(8000); // 10 second pause
  }
  
  // Return path
  for (int i = sizeof(POSITIONS)/sizeof(POSITIONS[0])-2; i >= 0; i--) {
    moveToPosition(POSITIONS[i]);
    printPosition(POSITIONS[i]);
    delay(8000); // 10 second pause
  }
}

void moveToPosition(int target) {
  int stepsToMove = target - currentStepPosition;
  Serial.print("Moving ");
  Serial.print(stepsToMove);
  Serial.println(" steps");
  
  stepper.step(stepsToMove);
  currentStepPosition = target;
  
  // Add small delay between steps for smoother movement
  delay(100);
}

void printPosition(int pos) {
  float angle = (pos * 180.0) / STEPS_FOR_180_DEG;
  Serial.print("Now at ");
  Serial.print(pos);
  Serial.print(" steps (");
  Serial.print(angle);
  Serial.println("°)");
}