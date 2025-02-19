#include "CloudInterface.h"

const char CloudInterface::ROOT_CERT_FILE[] = "/rootcert.crt";

void CloudInterface::LoadRootCert()
{
  if(LittleFS.exists(ROOT_CERT_FILE)) {
    File rootCertFile = LittleFS.open(ROOT_CERT_FILE, "r");
    if(certs != NULL) { delete certs; }
    certs = new X509List(rootCertFile);
    rootCertFile.close();
  }
}

String CloudInterface::WriteDataToCloud(SampleBuffer& samples, DeviceConfig& config)
{
  if(config.cloudLoggingUrl == NULL || config.cloudLoggingUrl.length() < 8 ||
    config.cloudLoggingApiKey == NULL || config.cloudLoggingApiKey.length() < 4 )
  {
    // Nothing valid to do.
    return String("Not configured");
  }
   
  WiFiClientSecure client;
  if(certs != NULL && certs->getCount() > 0)
  {
    client.setTrustAnchors(certs);
  }
  else
  {
    Serial.println("SendToCloud: using insecure.  No root cert to verify server.");
    client.setInsecure();
  }

  String jsonData = F("{ \"instanceId\": ");
  jsonData += config.cloudInstanceId + ",";
  jsonData += F("\"minimumValue\": ") + String(samples.min_temp, 1) + ",";
  jsonData += F("\"maximumValue\": ") + String(samples.max_temp, 1) + ",";  
  jsonData += F("\"timestamp\": ") + String(time(NULL)) + ",";

  // This is really the "current value".   Trying to get the previous sample period's average
  //  is problematic if the device has just been reset, and hasn't been running for long enough.
  // TODO: Think about a better way of reporting this to the cloud.
  int sampleIndex = samples.GetCurrentSampleIndex();
  jsonData += "\"value\": " + String(samples.sample_average_temps[sampleIndex]);
  jsonData += "}";


  int retries = 3;
  bool postSuccess = false;
  while(retries > 0 && !postSuccess)
  {
    postSuccess = PostData(client, jsonData, config);
    delay(10);
    retries--;
  }
  return lastResult;
}

bool CloudInterface::PostData(WiFiClientSecure client, String &json, DeviceConfig& config)
{
  HTTPClient https;
  
  // AWS dotnet Lambdas can be slow to "cold start".
  https.setTimeout(15000);
  
  if(!https.begin(client, config.cloudLoggingUrl))
  {
    lastResult = "Failed to begin";
    Serial.print("SendToCloud: ");
    Serial.println(lastResult);
    return false; 
  }

  https.addHeader("Content-Type", "application/json");
  https.addHeader("x-api-key", config.cloudLoggingApiKey);
  int httpCode = https.POST(json); 


  if(httpCode > 0)
  {
    lastResult = "HTTP Response: ";
    lastResult += String(httpCode) + "\n";
    lastResult += https.getString();
  }
  else
  {
    lastResult = "Failed. err=";
    lastResult += https.errorToString(httpCode);
  }
  Serial.print("SendToCloud: ");
  Serial.println(lastResult);

  return httpCode >= 200 && httpCode < 300;
}
