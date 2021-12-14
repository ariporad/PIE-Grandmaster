#include <Wire.h>
#include <SpeedyStepper.h>

//
// Constants
//

// Motors
// NOTE: X is forms, Y is ranks
#define X_STEP_PIN 3
#define Y_STEP_PIN 2
#define X_DIR_PIN 6
#define Y_DIR_PIN 5
#define STEPPERS_ENABLE_PIN 8
#define LIMIT_SWITCH_X_PIN 10
#define LIMIT_SWITCH_Y_PIN 9

#define STEPS_PER_SQUARE -250

#define SPEED_STEPS_PER_SEC 200
#define ACCEL_STEPS_PER_SEC_PER_SEC 100

#define LOOPS_PER_UPDATE 500

//
// State Variables
//

int cur_pos_x = 0; // 0-9, where 0 is A, 7 is H, and 8-9 are the graveyard
int cur_pos_y = 0; // 0-7, where 0 is Rank 1 and 7 is Rank 7

//
// Motors
//
SpeedyStepper xMotor;
SpeedyStepper yMotor;

void setup()
{
  // Setup the Serial interface
  Serial.begin(115200);

  Serial.setTimeout(50); // Make sure we don't spend too much time waiting for serial input

  // Setup the Motors
  pinMode(STEPPERS_ENABLE_PIN, OUTPUT);
  xMotor.connectToPins(X_STEP_PIN, X_DIR_PIN);
  yMotor.connectToPins(Y_STEP_PIN, Y_DIR_PIN);
  digitalWrite(STEPPERS_ENABLE_PIN, LOW);
}

int loops_since_update = 0;
void loop()
{
  // Check for Serial communication
  if (Serial.available())
  {
    uint8_t cmd = Serial.parseInt();

    // Commands are in the form of 0bAAAABBBB, where AAAA is the index of the form to move to
    // and BBBB is the index of the rank to move to, but one-indexed.
    if (cmd != 0) {
      uint8_t new_pos_x = min(7, (cmd >> 4) - 1);
      uint8_t new_pos_y = min(7, (cmd & 0b1111) - 1);
      int diff_pos_x = new_pos_x - cur_pos_x;
      int diff_pos_y = new_pos_y - cur_pos_y;

      int steps_x = diff_pos_x * STEPS_PER_SQUARE;
      int steps_y = diff_pos_y * STEPS_PER_SQUARE;

      moveXYWithCoordination(steps_x, steps_y, SPEED_STEPS_PER_SEC, ACCEL_STEPS_PER_SEC_PER_SEC);

      send_position();

      cur_pos_x = new_pos_x;
      cur_pos_y = new_pos_y;
    }
  }

  loops_since_update++;
  if (loops_since_update >= UPDATE_INTERVAL_MS) {
    loops_since_update = 0;
    send_position();
  }
}

void send_position() {
  // Update format is 0bAAAABBBBB where AAAA is the rank and BBBB is the file
  // One indexed to avoid zero bytes
  Serial.write(((cur_pos_x + 1) << 4) | (cur_pos_y + 1));
}

//
// move both X & Y motors together in a coordinated way, such that they each
// start and stop at the same time, even if one motor moves a greater distance
//
// From Stan's example
void moveXYWithCoordination(long stepsX, long stepsY, float speedInStepsPerSecond, float accelerationInStepsPerSecondPerSecond)
{
  float speedInStepsPerSecond_X;
  float accelerationInStepsPerSecondPerSecond_X;
  float speedInStepsPerSecond_Y;
  float accelerationInStepsPerSecondPerSecond_Y;
  long absStepsX;
  long absStepsY;

  //
  // setup initial speed and acceleration values
  //
  speedInStepsPerSecond_X = speedInStepsPerSecond;
  accelerationInStepsPerSecondPerSecond_X = accelerationInStepsPerSecondPerSecond;

  speedInStepsPerSecond_Y = speedInStepsPerSecond;
  accelerationInStepsPerSecondPerSecond_Y = accelerationInStepsPerSecondPerSecond;

  //
  // determine how many steps each motor is moving
  //
  if (stepsX >= 0)
    absStepsX = stepsX;
  else
    absStepsX = -stepsX;

  if (stepsY >= 0)
    absStepsY = stepsY;
  else
    absStepsY = -stepsY;

  //
  // determine which motor is traveling the farthest, then slow down the
  // speed rates for the motor moving the shortest distance
  //
  if ((absStepsX > absStepsY) && (stepsX != 0))
  {
    //
    // slow down the motor traveling less far
    //
    float scaler = (float)absStepsY / (float)absStepsX;
    speedInStepsPerSecond_Y = speedInStepsPerSecond_Y * scaler;
    accelerationInStepsPerSecondPerSecond_Y = accelerationInStepsPerSecondPerSecond_Y * scaler;
  }

  if ((absStepsY > absStepsX) && (stepsY != 0))
  {
    //
    // slow down the motor traveling less far
    //
    float scaler = (float)absStepsX / (float)absStepsY;
    speedInStepsPerSecond_X = speedInStepsPerSecond_X * scaler;
    accelerationInStepsPerSecondPerSecond_X = accelerationInStepsPerSecondPerSecond_X * scaler;
  }

  //
  // setup the motion for the X motor
  //
  xMotor.setSpeedInStepsPerSecond(speedInStepsPerSecond_X);
  xMotor.setAccelerationInStepsPerSecondPerSecond(accelerationInStepsPerSecondPerSecond_X);
  xMotor.setupRelativeMoveInSteps(stepsX);

  //
  // setup the motion for the Y motor
  //
  yMotor.setSpeedInStepsPerSecond(speedInStepsPerSecond_Y);
  yMotor.setAccelerationInStepsPerSecondPerSecond(accelerationInStepsPerSecondPerSecond_Y);
  yMotor.setupRelativeMoveInSteps(stepsY);

  //
  // now execute the moves, looping until both motors have finished
  //
  while ((!xMotor.motionComplete()) || (!yMotor.motionComplete()))
  {
    xMotor.processMovement();
    yMotor.processMovement();
  }
}
