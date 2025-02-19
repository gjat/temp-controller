#include "DeviceConfig.h"
#include "SampleBuffer.h"
#include <WiFiManager.h>
#include <ESP8266WebServer.h>

class DeviceWebServer 
{
public:
    DeviceWebServer(DeviceConfig &config, SampleBuffer &samples) :
      server(80), 
      configRef(config),
      samplesRef(samples)      
    {};

    void Setup();

    // To be called in the main loop, to service requests.
    void handleClient() { server.handleClient(); }
    
    bool RecordStartupTime();

    void OnRootCertChanged(std::function<void()> certChanged)  { onCertChanged = certChanged; }
    void OnTestCall(std::function<String()> testFunction)      { onTestCall = testFunction; }  // returns a result to display
    void OnResetWiFiSettings(std::function<void()> resetWifi)  { onResetWifi = resetWifi; }

private:
    ESP8266WebServer server;    // Create a webserver object that listens for HTTP request on port 80    

    DeviceConfig& configRef;
    SampleBuffer& samplesRef;

    // Record when the system started, to display "uptime" information.
    time_t startup_time = 0;

    // Used during file uploads.
    File rootCertUploadFile;

    std::function<void()> onCertChanged;
    std::function<String()> onTestCall;
    std::function<void()> onResetWifi;

private:
    void handleRoot();              // function prototypes for HTTP handlers
    void handleConfigure();
    void handleRootCertUpload();
    void handleDirList();
    void handleTestCode();
    void handleNotFound();

    String getUpTime();
    String getChartHtml(int startAtSample);

    void processConfigSet();
    void processResetMinMaxTemps();
    void processResetSamples();
    void redirectBackToRoot();
};
