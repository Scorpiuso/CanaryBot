const connect = document.getElementById("bluetooth");
const serviceUUID = "36124082-beb0-468d-878d-4e92e1d57754";
const charUUID = "2b1d2fc0-d457-4d7d-bcdc-5dc309b86e1d";

const txUUID = "424c7441-deb7-41a6-ab07-91838c0ee835";

let encoder = new TextEncoder();

const sendPosesUUID = "8bd3c001-a985-428e-9033-ef4ba970cc52";

const totalStepsServo = 180;
const sliders = Array.from(document.querySelectorAll("input"));
const numberOfServos = 4;

class Pose {
  constructor(angle, duration, isBreak, interpolationType, index) {
    this.angle = angle;
    this.duration = duration;
    this.isBreak = isBreak;
    this.interpolationType = interpolationType;
    this.index = index;
  }
}

const interpMapping = {
  linear: 1,
  quadratic: 2,
  hardquad: 3,
};

import {
  HandLandmarker,
  FilesetResolver,
} from "https://cdn.jsdelivr.net/npm/@mediapipe/tasks-vision@latest";

import * as BS from "./brilliantsole/brilliantsole.module.js";

// document.querySelector(".local-transform input").click() Use this bc aframe need

let jointArray = Array.from(document.querySelectorAll("[rotary]"));
console.log(jointArray);

let currentServoPositions = [];
for (let i = 0; i < numberOfServos; i++) {
  currentServoPositions.push(0);
}

async function connectAndInteract() {
  try {
    const device = await navigator.bluetooth.requestDevice({
      filters: [{ services: [serviceUUID] }],
    });

    const server = await device.gatt.connect();

    const service = await server.getPrimaryService(serviceUUID);

    const characteristic = await service.getCharacteristic(charUUID);

    const txCharacteristic = await service.getCharacteristic(txUUID);
    await txCharacteristic.startNotifications();

    let readyForNextBytePackage = true;

    // await txCharacteristic.startNotifications().then((characteristic) => {

    // });

    txCharacteristic.addEventListener("characteristicvaluechanged", (event) => {
      if (!readyForNextBytePackage) {
        readyForNextBytePackage = true;
      }
    });

    const characteristicPoseData =
      await service.getCharacteristic(sendPosesUUID);

    //characteristicPoseData.addEventListener("characteristicvaluechanged", modifyCurrentAnimationData);

    const initialVals = await characteristic.readValue();

    const dataByteArray = new Uint8Array(initialVals.buffer);

    let pending = null;
    let running = true;
    let pendingArray = null;
    let writerStarted = false;

    window.dispatchEvent(
      new CustomEvent("initializeServos", {
        detail: dataByteArray,
      }),
    );

    window.waitForAck = async () => {
      return new Promise((resolve) => {
        function handler(event) {
          const value = event.target.value.buffer;

          if (value == 1) {
            txCharacteristic.removeEventListener(
              "characteristicvaluechanged",
              handler,
            );

            readyForNextBytePackage = true;
            resolve();
          }
        }

        txCharacteristic.addEventListener(
          "characteristicvaluechanged",
          handler,
        );
      });
    };

    window.sendAnimationData = async (poseArray) => {
      const taskByte = {
        angle: 0,
        duration: 1,
        isBreak: 2,
        interpolationType: 3,
        index: 4,
      };

      let sendingIndex = 0;

      for (let poseFrame of poseArray) {
        if (poseFrame instanceof Pose) {
          // Perform task on each individual pose object

          // this.angle = angle;
          // this.duration = duration;
          // this.isBreak = isBreak;
          // this.interpolationType = interpolationType;
          // this.index = index;

          let dataForSending = [];
          let instructionByte = [];

          const dataPlusInstructionByte = {};

          for (const [key, value] of Object.entries(poseFrame)) {
            if (value != null) {
              //Splitting into 2-Byte Chunks
              if (value instanceof Array) {
                dataForSending.push(Uint16Array.from(value));
                dataPlusInstructionByte[value] = taskByte[key];
              } else {
                dataForSending.push(Uint16Array.from([value]));
                dataPlusInstructionByte[value] = taskByte[key];
              }
            }
          }

          for (const [key, value] of Object.entries(dataPlusInstructionByte)) {
            if (readyForNextBytePackage) {
              if (key instanceof Array) {
                const sendArray = [...key, value];
              } else {
                const sendArray = [key, value];
              }

              readyForNextBytePackage = false;

              const prommy = waitForAck();

              await characteristicPoseData.writeValue(
                Uint8Array.from(sendArray),
              );

              await prommy;

              readyForNextBytePackage = true;
            }
          }
        }
      }
    };

    window.bleWriterLoop = async (nums) => {
      while (true) {
        if (pendingArray) {
          const data = pendingArray;
          //pendingArray = null;

          try {
            if (characteristic.properties.writeWithoutResponse) {
              characteristic.writeValueWithoutResponse(data);
            } else {
              await characteristic.writeValue(data);
            }
          } catch (e) {
            console.error("BLE write failed", e);
          }

          //console.log(`${data[0]} is the vals`);
        }

        // BLE pacing — REQUIRED
        await new Promise((r) => setTimeout(r, 10)); // ~30Hz
      }
    };

    // window.writeValue = async (numbers) => {
    //   const array = Uint8Array.from(numbers);
    //   await characteristic.writeValue(array);
    //   return array;
    // };

    window.writeValue = async (numbers) => {
      // Store latest data ONLY
      pendingArray = Uint8Array.from(numbers);

      // Start writer loop once
      if (!writerStarted) {
        writerStarted = true;
        //console.log(numbers);
        bleWriterLoop(pendingArray);
      }

      return pendingArray;
    };
    //window.writeValue = throttler(window.writeValue, 10);
    console.log("connected");
  } catch (error) {
    console.error(error);
  }
}

function sendRotationBytesDEMAND(array, totalStepsServo) {
  let newRotArray = []; // To be built upon

  if (
    array instanceof Array &&
    typeof totalStepsServo == "number" &&
    totalStepsServo > 0
  ) {
    array.forEach((value) => {
      newRotArray.push(Math.trunc(180 * (value / totalStepsServo)));
    });

    return newRotArray;
  } else {
    if (!(array instanceof Array)) {
      throw new Error("First parameter must be an array of integers");
    } else if (typeof totalStepsServo != "number") {
      throw new Error("Steps per servo must be an integer");
    } else if (totalStepsServo <= 0) {
      throw new Error("Steps per servo must be a NON ZERO POSITIVE value");
    }
  }
}

let values = [];

function throttler(callerFunction, interval) {
  let beginningTime = 0;
  return (...args) => {
    let now = Date.now();
    if (now - beginningTime >= interval) {
      callerFunction(...args);
      beginningTime = now;
    }
  };
}

sliders.forEach((slider) => {
  slider.addEventListener("input", function () {
    currentServoPositions[sliders.indexOf(this)] = Number(this.value);

    writeValue(sendRotationBytesDEMAND(currentServoPositions, totalStepsServo));
  });
});

connect.addEventListener("click", () => {
  let returnedVal = connectAndInteract(numberOfServos, currentServoPositions);
  console.log(returnedVal);
});

window.addEventListener("initializeServos", (event) => {
  const rotArray = event.detail;

  console.log(rotArray);
});

// MEDIAPIPE

let handLandmarker;
function detectHandLandmarker(media) {
  let results = handLandmarker.detectForVideo(media, performance.now());

  if (results.landmarks.length > 0) {
    if (results.landmarks.length == 1) {
      results.landmarks[0].forEach((value) => {
        value.x *= -1;
        value.x++;

        value.y *= -1;
        value.y++;
      });
    } else {
      results.landmarks.forEach((hand) => {
        hand.forEach((point) => {
          point.x *= -1;
          point.x++;

          point.y *= -1;
          point.y++;
        });
      });
    }
    window.dispatchEvent(
      new CustomEvent("landmarkData", { detail: results.landmarks }),
    );
  }
}
async function startVideo() {
  let stream = await navigator?.mediaDevices?.getUserMedia({
    video: true,
  });
  video.srcObject = stream;

  video.addEventListener("loadeddata", () => {
    console.log("Video ready:", video.videoWidth, video.videoHeight);

    function loop() {
      if (video.videoWidth === 0 || video.videoHeight === 0) {
        requestAnimationFrame(loop);
        return;
      }

      detectHandLandmarker(video);

      requestAnimationFrame(loop);
    }

    loop();
  });
}
async function loadHandLandmarker() {
  const vision = await FilesetResolver.forVisionTasks(
    "https://cdn.jsdelivr.net/npm/@mediapipe/tasks-vision@latest/wasm",
  );

  handLandmarker = await HandLandmarker.createFromOptions(vision, {
    baseOptions: {
      modelAssetPath:
        "https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/1/hand_landmarker.task",
      delegate: "GPU",
    },
    runningMode: "VIDEO",
    numHands: 2,
  });
}

const handBox = document.querySelectorAll("[hand]")[0];

window.addEventListener("landmarkData", (data) => {
  // const valueX = (data.detail[0][0].x * Math.PI - Math.PI / 2) * 180;
  // const valueY = (data.detail[0][0].y * Math.PI - Math.PI / 2) * 180;

  const valueX = data.detail[0][0].x * 180;
  const valueY = data.detail[0][0].y * 180;
  console.log();

  handBox.object3D.position.x = data.detail[0][8].x * 30 - 15;

  handBox.object3D.position.y = data.detail[0][8].y * 36 - 18;

  try {
    writeValue([valueX, 90, valueY, 90]);
    jointArray[0].object3D.rotation.y = (valueX / 90) * 2 - (3 * Math.PI) / 4;
    jointArray[2].object3D.rotation.x = (valueY / 90) * -2 + (3 * Math.PI) / 4;
    //console.log("GREEN FN!");
  } catch {
    return;
  } finally {
    return;
  }
});

const bsDevice = new BS.Device();
const toggleBSConnectionButton = document.getElementById("toggleBSConnection");
toggleBSConnectionButton.addEventListener("click", () => {
  bsDevice.toggleConnection();
});
bsDevice.addEventListener("connectionStatus", () => {
  let innerText = bsDevice.connectionStatus;
  switch (bsDevice.connectionStatus) {
    case "notConnected":
      innerText = "connect";
      break;
    case "connected":
      innerText = "disconnect";
      break;
  }
  toggleBSConnectionButton.innerText = innerText;
});

bsDevice.addEventListener("connected", async () => {
  await bsDevice.setCameraConfiguration({ resolution: 200, qualityFactor: 60 });
  bsDevice.autoPicture = true;
  bsDevice.takePicture();
});

/** @type {HTMLImageElement} */
const cameraImage = document.getElementById("cameraImage");
bsDevice.addEventListener("cameraImage", (event) => {
  cameraImage.src = event.message.url;
});
cameraImage.addEventListener("load", () => {
  detectHandLandmarker(cameraImage);
});

loadHandLandmarker();
//startVideo();
