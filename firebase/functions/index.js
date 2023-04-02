const functions = require('firebase-functions');
const admin = require('firebase-admin');
admin.initializeApp();

const iot = require('@google-cloud/iot');
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
