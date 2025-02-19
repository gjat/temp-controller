#include "DeviceConfig.h"
#include "SampleBuffer.h"
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

class CloudInterface
{
public:
    void LoadRootCert();
    String WriteDataToCloud(SampleBuffer& samples, DeviceConfig& config);

    static const char ROOT_CERT_FILE[];

private:
    bool PostData(WiFiClientSecure client, String &json, DeviceConfig& config);

private:
    X509List* certs = NULL;
    String lastResult;
};
