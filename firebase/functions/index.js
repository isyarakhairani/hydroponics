const functions = require('firebase-functions');
const { initializeApp, applicationDefault, cert } = require('firebase-admin/app');
const { getFirestore, Timestamp, FieldValue, Filter } = require('firebase-admin/firestore');
const iot = require('@google-cloud/iot');

initializeApp();

const db = getFirestore();
const iotClient = new iot.v1.DeviceManagerClient();

exports.httpSendCommand = functions.region('asia-southeast2').https.onRequest(async (req, res) => {
  const projectId = 'hydroponics-378311';
  const region = 'asia-east1';
  const registryId = 'hydroponics';
  const deviceId = req.body.deviceId;

  const name = iotClient.devicePath(projectId, region, registryId, deviceId);
  const data = Buffer.from(JSON.stringify(req.body)).toString('base64');

  const request = {
    name: name,
    binaryData: data,
  };
  functions.logger.info(request);

  try {
    const [response] = await iotClient.sendCommandToDevice(request);
    res.sendStatus(200);
  } catch (e) {
    functions.logger.error(e);
    res.sendStatus(404);
  }
});

exports.pubsubEventData = functions.region('asia-southeast2').pubsub.topic('event').onPublish(async (msg) => {
  functions.logger.log(Buffer.from(msg.data, 'base64').toString());
  const deviceId = msg.attributes.deviceId;

  const realtimeRef = db.collection('hydroponics').doc('realtime_update');
  const storedRef = db.collection('stored');

  const data = {
    initialized: msg.json.initialized,
    elapsedDays: msg.json.elapsedDays,
    tdsValue: msg.json.tdsValue,
    phValue: msg.json.phValue,
    temperature: msg.json.temperature,
    humidity: msg.json.humidity,
    tankLevel: msg.json.tankLevel,
    timestamp: FieldValue.serverTimestamp(),
  };

  const update = await realtimeRef.update(data);
  functions.logger.log('Update: ', update);

  const currentTms = Timestamp.now().toMillis();
  const snapshot = await storedRef.where('timestamp', '>', new Date(currentTms - (60 * 60 * 1000))).get();
  if (snapshot.empty) {
    const add = await storedRef.add(data);
    functions.logger.log('Add: ', add);
  }
});
