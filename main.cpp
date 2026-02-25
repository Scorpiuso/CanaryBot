#include <Arduino.h>
#include <ArduinoBLE.h>
#include <Servo.h>
#include <stdint.h>
#include <string.h>

const char *BLESERVUUID = "36124082-beb0-468d-878d-4e92e1d57754";
const char *BLECHARUUID = "2b1d2fc0-d457-4d7d-bcdc-5dc309b86e1d";

const char *BLEPOSECHARUUID = "8bd3c001-a985-428e-9033-ef4ba970cc52";

const char *txUUID = "424c7441-deb7-41a6-ab07-91838c0ee835";
const uint8_t ack = 1;

const char *DEVICENAME = "Jimmy's Pet";

const uint8_t SERVO_LIST[] = {2, 3, 6, 7};
const uint8_t SERVO_LIST_LENGTH = sizeof(SERVO_LIST) / sizeof(SERVO_LIST[0]);

// TIMING

unsigned long lastUpdate = 0;

BLEService bleService(BLESERVUUID);
BLECharacteristic bleCharacteristic(BLECHARUUID, BLEWrite | BLERead, SERVO_LIST_LENGTH);

BLECharacteristic receivePoses(BLEPOSECHARUUID, BLEWrite | BLERead | BLENotify, 15);

BLECharacteristic tx(txUUID, BLENotify | BLERead, 1);

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
  int angles[SERVO_LIST_LENGTH];
  float duration;
  bool isBreak = false;
  String interpolationType;
  int index = 0;
};

// USE TASK BYTE TO REFERENCE!

enum TASKBYTE
{
  ANGLE,
  DURATION,
  ISBREAK,
  INERPOLATIONTYPE,
  INDEX,
};

struct POSE_LIST
{                                     // NEEDS SERVO LIST LENGTH!!!
  int startAngles[SERVO_LIST_LENGTH]; // Establish Initial Angles
  Pose *allPoses;                     // Pose array with poses and durations to be passed here
  int poseArrayLength;                // Establish how many poses will be in animation

  unsigned long startTime;   // Start (initial time) of run for each individual pose animation
  unsigned long currentTime; // Current time (relative to initial time) of run for each individual pose animation

  Servo **servoList; // List of Servos to manipulate
  int servoListLength;

  bool active; // Is active or animating

  int currentPoseIndex; // Which pose are we at

  String interpolationType = allPoses[currentPoseIndex].interpolationType;

  int isMoving = false;

  bool requestingAnimationChange = false;

  Pose *replacementPoses[25];
  int sizeOfReplacementArray; // LIST OF ACTUAL POSES TO CHANGE!

  bool temporaryIndexChanged = false;

  void nextPoseUp()
  {
    if (!temporaryIndexChanged)
    {
      memcpy(&startAngles, allPoses[currentPoseIndex].angles, sizeof(startAngles));
      currentPoseIndex = (currentPoseIndex + 1) % poseArrayLength;
      startTime = millis();
      Serial.println("WORKING");
      temporaryIndexChanged = true;
    }
  }

  void nextPoseCustom(int index)
  {
    if (!temporaryIndexChanged)
    {
      memcpy(&startAngles, allPoses[currentPoseIndex].angles, sizeof(startAngles));
      currentPoseIndex = index % poseArrayLength;
      startTime = millis();
    }
  }

  void replaceAnimationFrame(Pose *poseArray, int index)
  {
    requestingAnimationChange = true;

    if (!isMoving)
    {
      memcpy(&allPoses[index], poseArray, sizeof(allPoses[index]));
      requestingAnimationChange = false;
    }
    else
    {
      sizeOfReplacementArray = sizeof(replacementPoses) / sizeof(replacementPoses[0]);
      replacementPoses[sizeOfReplacementArray] = poseArray;
    }
  }

  void replaceAnimationFrames(Pose **poseArray)
  {
    int sizeOfIncomingArray = sizeof(poseArray) / sizeof(poseArray[0]);

    for (int i = 0; i < sizeOfIncomingArray; i++)
    {
      replaceAnimationFrame(poseArray[i], poseArray[i]->index);
    }
  }
};

Pose myPoses[] = {
    {{100, 100, 100, 100}, 1000, true},
    {{0, 0, 0, 0}, 2000, false, "linear"},
    {{100, 120, 5, 90}, 5000, false, "highquad"}};

Pose disagree[] = {
    {{90, 90, 90, 90}, 500, true},
    {{90, 130, 90, 130}, 300, false},
    {{90, 45, 90, 60}, 300, false},
    {{90, 130, 90, 130}, 300, false},
    {{90, 45, 90, 60}, 300, false},
    {{90, 90, 90, 90}, 500, false}};

// TO BE LOOPED

void updateSequenceServos(POSE_LIST &pose_list)
{
  if (!pose_list.active)
    return;

  pose_list.temporaryIndexChanged = false;

  // unsigned long now = millis();
  pose_list.currentTime = millis();

  int num_of_servos = pose_list.servoListLength;

  unsigned long startTime = pose_list.startTime;
  unsigned long currentTime = pose_list.currentTime;

  float timeInterpolation = ((float)currentTime - (float)startTime) / static_cast<float>(pose_list.allPoses[pose_list.currentPoseIndex].duration);

  if (!pose_list.allPoses[pose_list.currentPoseIndex].interpolationType)
  {
    if (pose_list.interpolationType == "linear")
    {
      timeInterpolation = timeInterpolation;
    }
    else if (pose_list.interpolationType == "quadratic")
    {
      timeInterpolation = pow(timeInterpolation, 2);
    }
    else if (pose_list.interpolationType == "lowquad")
    {
      timeInterpolation = pow(timeInterpolation, 4);
    }
    else if (pose_list.interpolationType == "midquad")
    {
      timeInterpolation = pow(timeInterpolation, 7);
    }
    else if (pose_list.interpolationType == "highquad")
    {
      timeInterpolation = pow(timeInterpolation, 10);
    }
  }
  else
  {
    if (pose_list.allPoses[pose_list.currentPoseIndex].interpolationType == "linear")
    {
      timeInterpolation = timeInterpolation;
    }
    else if (pose_list.allPoses[pose_list.currentPoseIndex].interpolationType == "quadratic")
    {
      timeInterpolation = pow(timeInterpolation, 2);
    }
    else if (pose_list.allPoses[pose_list.currentPoseIndex].interpolationType == "lowquad")
    {
      timeInterpolation = pow(timeInterpolation, 4);
    }
    else if (pose_list.allPoses[pose_list.currentPoseIndex].interpolationType == "midquad")
    {
      timeInterpolation = pow(timeInterpolation, 7);
    }
    else if (pose_list.allPoses[pose_list.currentPoseIndex].interpolationType == "highquad")
    {
      timeInterpolation = pow(timeInterpolation, 10);
    }
  }

  if (!pose_list.allPoses[pose_list.currentPoseIndex].isBreak)
  {
    pose_list.isMoving = true;
    for (int i = 0; i < num_of_servos; i++)
    {
      Servo *servo = pose_list.servoList[i];
      int currentServoPosition = servo->read();
      int targetServoPosition = pose_list.allPoses[pose_list.currentPoseIndex].angles[i];

      int startAngle = pose_list.startAngles[i];

      float interpolationAngle = static_cast<float>(((targetServoPosition - currentServoPosition) * (timeInterpolation)) + startAngle);

      if (timeInterpolation < 1)
      {
        servo->write(static_cast<int>(interpolationAngle));
      }
      else
      {

        pose_list.nextPoseUp();

        pose_list.isMoving = false;

        if (pose_list.requestingAnimationChange)
        {
          for (int i = 0; i < pose_list.sizeOfReplacementArray; i++)
          {
            memcpy(&pose_list.allPoses[pose_list.replacementPoses[i]->index], pose_list.replacementPoses[i], sizeof(pose_list.allPoses[pose_list.replacementPoses[i]->index]));
          }

          pose_list.requestingAnimationChange = false;
          pose_list.sizeOfReplacementArray = 0;
          *pose_list.replacementPoses = nullptr;
        }
      }
    }
  }
  else
  {

    if (timeInterpolation >= 1)
    {

      pose_list.nextPoseUp();
      pose_list.isMoving = false;

      if (pose_list.requestingAnimationChange)
      {
        for (int i = 0; i < pose_list.sizeOfReplacementArray; i++)
        {
          memcpy(&pose_list.allPoses[pose_list.replacementPoses[i]->index], pose_list.replacementPoses[i], sizeof(pose_list.allPoses[pose_list.replacementPoses[i]->index]));
        }

        pose_list.requestingAnimationChange = false;
        pose_list.sizeOfReplacementArray = 0;
        *pose_list.replacementPoses = nullptr;
      }
    }
  }
}

void updateSequence(Servo **servoList, int *start_angles, int *current_angles_POS, Pose current_angles, int servo_length, int poses_total);
void moveToPose(int pose_index);
POSE_LIST my_animation = {{90, 90, 90, 90}, myPoses, 3, 0, 0, servos, SERVO_LIST_LENGTH, true, 0, "quadratic"};

POSE_LIST disagreeAnimation = {{90, 90, 90, 90}, disagree, 6, 0, 0, servos, SERVO_LIST_LENGTH, true, 0, "linear"};

void setup()
{
  Serial.begin(115200);
  pinMode(8, INPUT_PULLUP);
  delay(2000);

  tx.writeValue(&ack, 1);

  // setupBLE(DEVICENAME, BLESERVUUID, BLECHARUUID, bleService, bleCharacteristic);
  if (!BLE.begin())
  {
    Serial.println("BLE Configuration Failed");
    while (1)
      ;
  }
  Serial.println("setting up ble");

  BLE.setLocalName(DEVICENAME);
  BLE.setDeviceName(DEVICENAME);
  BLE.setAdvertisedService(bleService);

  bleService.addCharacteristic(bleCharacteristic);
  bleService.addCharacteristic(receivePoses);
  bleService.addCharacteristic(tx);
  BLE.addService(bleService);

  BLE.advertise();

  Serial.println("Setup Complete");
  setupServosToPins(servos, SERVO_LIST, SERVO_LIST_LENGTH);
  moveAllServos(servos, vals, valsLength);

  // Send initial values to central
  bleCharacteristic.writeValue(vals, valsLength);

  // moveToPose(0);
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
  // lastUpdate = millis();

  // updateSequenceServos(my_animation);

  // updateSequence();
  // updateSequence(servos, startAngles, currentAngles, *sequence2, SERVO_LIST_LENGTH, totalPoses);

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

  if (receivePoses.written())
  {
    // Change to become a uint16_t and recieve all values and send through a Uint16Array.of(array or value);

    const uint8_t *values = receivePoses.value();
    const uint8_t taskByte = values[0];
    const uint16_t *angleValues = (uint16_t *)&values[1];
    const uint8_t size = receivePoses.valueLength();

    const uint8_t numofValues = (size - 1) / 2;

    // Serial.println(taskByte);

    uint16_t valuesManipulate[numofValues - 1];

    for (uint8_t i = 1; i + 1 < numofValues; i++)
    {
      // Serial.println(angleValues[i]);
      valuesManipulate[i - 1] = angleValues[i];
    }

    switch (taskByte)
    {
    case 0:
      Serial.println("Executing nubers though array");

      for (uint8_t i = 0; i < numofValues; i++)
      {
        Serial.println("VALUES: \n");
        Serial.print(valuesManipulate[i]);
      }
      break;
    case 1:
      break;
    case 2:
      break;
    case 3:
      break;
    case 4:
      break;
    default:
      Serial.print("ERROR! THIS EXECUTION DOES NOT EXIST");
      return;
    }

    tx.writeValue(&ack, 1);
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
  BLE.setAdvertisedService(bleService);

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
