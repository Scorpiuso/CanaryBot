#include <Arduino.h>
#include <ArduinoBLE.h>
#include <Servo.h>
#include <stdint.h>

const char *BLESERVUUID = "36124082-beb0-468d-878d-4e92e1d57754";
const char *BLECHARUUID = "2b1d2fc0-d457-4d7d-bcdc-5dc309b86e1d";

const char *DEVICENAME = "Jimmy's Pet";

const uint8_t SERVO_LIST[] = {2, 3, 6, 7};
const uint8_t SERVO_LIST_LENGTH = sizeof(SERVO_LIST) / sizeof(SERVO_LIST[0]);

// TIMING

unsigned long lastUpdate = 0;

BLEService bleService(BLESERVUUID);
BLECharacteristic bleCharacteristic(BLECHARUUID, BLEWrite | BLERead, SERVO_LIST_LENGTH);

void moveSingleServo(Servo **servoList, uint8_t index, uint8_t amount);
void moveAllServos(Servo **servoList, const uint8_t *amounts, const uint8_t arrayLength);

Servo *servos[SERVO_LIST_LENGTH];

// Setup to standing straight up
const uint8_t vals[] = {90, 90, 90, 90};
const uint8_t valsLength = sizeof(vals) / sizeof(vals[0]);

void setupBLE(const char *DEV_NAME, const char *SERVICE_UUID, const char *CHAR_UUID, BLEService &service, BLECharacteristic &characteristic);
void setupServosToPins(Servo **servolist, const uint8_t *pins, uint8_t length);

struct Pose
{
  int angles[SERVO_LIST_LENGTH]; // target angles for servos
  float duration;                // Degrees per second
};

void updateSequence(Servo **servoList, int *start_angles, int *current_angles_POS, Pose current_angles, int servo_length, int poses_total);
void moveToPose(int pose_index);

void setup()
{
  Serial.begin(115200);
  pinMode(8, INPUT_PULLUP);
  delay(2000);
  Serial.println("setup");

  setupBLE(DEVICENAME, BLESERVUUID, BLECHARUUID, bleService, bleCharacteristic);
  setupServosToPins(servos, SERVO_LIST, SERVO_LIST_LENGTH);
  moveAllServos(servos, vals, valsLength);

  // Send initial values to central
  bleCharacteristic.writeValue(vals, valsLength);

  moveToPose(0);
}

Pose sequence[] = {
    //{{90, 90, 90, 90}, 5},
    {{120, 60, 140, 30}, 500},
    {{45, 120, 60, 150}, 5000},
    {{90, 90, 90, 90}, 100},
    {{0, 0, 0, 0}, 12000},
    {{90, 0, 0, 0}, 1200},
    {{90, 90, 0, 0}, 1200},
    {{90, 90, 90, 0}, 1200},
    {{90, 90, 90, 90}, 1200},
};

Pose sequence2[] = {
    {{100, 90, 90, 12}, 900},
    {{200, 100, 0, 170}, 400},
    {{90, 0, 0, 0}, 500},
    {{90, 90, 0, 0}, 500},
    {{90, 90, 90, 0}, 500},
    {{90, 90, 90, 90}, 500},
};

const int totalPoses = sizeof(sequence2) / sizeof(sequence2[0]);

int currentAngles[SERVO_LIST_LENGTH] = {90, 90, 90, 90};
int startAngles[SERVO_LIST_LENGTH] = {0};

void loop()
{

  lastUpdate = millis();

  // updateSequence();
  updateSequence(servos, startAngles, currentAngles, *sequence2, SERVO_LIST_LENGTH, totalPoses);

  BLE.poll();

  if (bleCharacteristic.written())
  {
    Serial.println("wrote!");
    const uint8_t *value = bleCharacteristic.value();
    const int valueLength = bleCharacteristic.valueSize();
    Serial.print("received ");
    Serial.print(valueLength);
    Serial.println(" bytes!");

    moveAllServos(servos, value, SERVO_LIST_LENGTH);

    for (int i = 0; i < valueLength; i++)
    {
      Serial.print(value[i]);
      Serial.print(",");
    }
    Serial.println();
  }
}

// Creating Poses through Struct

Pose &currentPose = sequence2[0];
int currentPoseIndex = 0;
unsigned long poseStartTime;
bool running = true;

void moveToPose(int pose_index)
{
  Serial.print("moving to pose ");
  Serial.println(pose_index);

  currentPoseIndex = pose_index;
  currentPose = sequence2[pose_index];
  poseStartTime = lastUpdate;
  memcpy(startAngles, currentAngles, sizeof(startAngles));
}

void updateSequence(Servo **servoList, int *start_angles, int *current_angles_POS, Pose current_angles, int servo_length, int poses_total)
{
  if (!running)
    return;

  float interpolation = (lastUpdate - poseStartTime) / current_angles.duration;
  bool poseReached = interpolation >= 1;
  float easedInterpolation = interpolation;

  // Serial.print("interpolation ");
  // Serial.println(interpolation);

  for (int i = 0; i < servo_length; i++)
  {
    int startAngle = start_angles[i];
    int targetAngle = current_angles.angles[i];

    current_angles_POS[i] = startAngle + (targetAngle - startAngle) * easedInterpolation;

    servoList[i]->write(current_angles_POS[i]);
  }

  // Move to next pose when all servos arrive
  if (poseReached)
  {
    int newPoseIndex = (currentPoseIndex + 1) % poses_total; // GLOBAL CURRENTPOSEINDEX
    moveToPose(newPoseIndex);
  }
}

// END TESTING CODE

void moveSingleServo(Servo **servoList, uint8_t index, uint8_t amount)
{
  // servoList[index].write(amount);
  servoList[index]->write(amount);
}

void moveAllServos(Servo **servoList, const uint8_t *amounts, const uint8_t arrayLength)
{
  for (uint8_t servoIndex = 0; servoIndex < arrayLength; servoIndex++)
  {
    // servoList[servoIndex].write(amounts[servoIndex]);
    servoList[servoIndex]->write(amounts[servoIndex]);
  }
}

void idleMovement(Servo **servoList, const uint8_t *amounts, const uint8_t arrayLength, const char *movementName)
{
  for (uint8_t servoIndex = 0; servoIndex < arrayLength; servoIndex++)
  {
    // servoList[servoIndex].write(amounts[servoIndex]);
    servoList[servoIndex]->write(amounts[servoIndex]);
  }
}

void setupBLE(const char *DEV_NAME, const char *SERVICE_UUID, const char *CHAR_UUID, BLEService &service, BLECharacteristic &characteristic)
{
  if (!BLE.begin())
  {
    Serial.println("BLE Configuration Failed");
    while (1)
      ;
  }
  Serial.println("setting up ble");

  BLE.setLocalName(DEV_NAME);
  BLE.setDeviceName(DEV_NAME);
  BLE.setAdvertisedService(service);

  bleService.addCharacteristic(characteristic);
  BLE.addService(service);

  BLE.advertise();

  Serial.println("Setup Complete");
}

void setupServosToPins(Servo **servolist, const uint8_t *pins, uint8_t length)
{
  for (uint8_t i = 0; i < length; i++)
  {
    servolist[i] = new Servo();
    servolist[i]->attach(pins[i]);
  }
}

void cleanupServos(Servo **servoList, const uint8_t length)
{
  for (uint8_t i = 0; i < length; i++)
  {
    delete servoList[i];
  }
}

/*void KILL_SERVOS(Servo **servolist, const uint8_t length)
{
  for (uint8_t i = 0; i < length; i++)
  {
    servolist[length]->detach();
  }
}

void UNKILL_SERVOS(Servo **servolist, const uint8_t *pins, uint8_t length)
{
  for (uint8_t i = 0; i < length; i++)
  {
    servolist[i]->attach(pins[i]);
  }
}*/
