import * as functions from 'firebase-functions';
import * as admin from 'firebase-admin';
const loggingClient = require('@google-cloud/logging');
const logging = new loggingClient({ projectId: process.env.GCLOUD_PROJECT, });
const iot = require('@google-cloud/iot');
const iotClient = new iot.v1.DeviceManagerClient();
admin.initializeApp();

exports.deviceLog = functions.region('asia-northeast1').pubsub.topic('device-logs').onPublish((message) => {
  admin.firestore().collection(`serial`).add(message.json);
  console.log("メッセージ受信" + message.attributes.deviceNumId);
  const log = logging.log('device-logs');
  const metadata = {
    resource: {
      type: 'cloudiot_device',
      labels: {
        project_id: message.attributes.projectId,
        device_num_id: message.attributes.deviceNumId,
        device_registry_id: message.attributes.deviceRegistryId,
        location: message.attributes.location,
      }
    },
    labels: {
      device_id: message.attributes.deviceId,// note device_id is not part of the monitored resource, but you can include it as another log label
    }
  };
  const logData = message.json;
  const validSeverity = ['DEBUG', 'INFO', 'NOTICE', 'WARNING', 'ERROR', 'ALERT', 'CRITICAL', 'EMERGENCY'];// Here you optionally extract a severity value from the log payload if it is present
  if (logData.severity && validSeverity.indexOf(logData.severity.toUpperCase()) > -1) {
    (metadata as any)['severity'] = logData.severity.toUpperCase();
    delete (logData.severity);
  }
  const entry = log.entry(metadata, logData);
  return log.write(entry);
});
export const led1Update = functions.region('asia-northeast1').firestore.document('state/{deviceId}').onUpdate((change, context) => {
  console.log(`コマンド受付 deviceId:${context.params.deviceId}`);
  const request = generateRequest(context.params.deviceId, change.after.data(), false);
  //return iotClient.modifyCloudToDeviceConfig(request);
  return iotClient.sendCommandToDevice(request);
});
function generateRequest(deviceId: string, configData: any, isBinary: Boolean) {
  const formattedName = iotClient.devicePath("iot-core8266", "asia-east1", "test_registry", deviceId);
  let dataValue = Buffer.from(JSON.stringify(configData)).toString("base64");
  console.log(`コマンドデータ:${dataValue}`);
  return {
    name: formattedName,
    binaryData: dataValue
  };
}