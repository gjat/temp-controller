#include "DeviceWebServer.h"
#include "CloudInterface.h"

const long NTP_MIN_VALID_EPOCH = 1104537600;  // Jan 01 2005

void DeviceWebServer::Setup() {
  
  // Using lambdas to pass the "this" pointer (instance pointer) to the class method.
  server.on("/", [&]() { handleRoot(); } );
  server.on("/configure", [&]() { handleConfigure(); });
  server.on("/testcode", [&]() { handleTestCode(); });
  server.on("/rootcert", HTTP_POST, [&]() { server.send(200); }, [&]() { handleRootCertUpload(); } );  // Not secure, if anyone on the local LAN can upload a root cert.  This should be password protected.
  server.on("/dir", [&]() { handleDirList(); } );
  
  server.onNotFound([&]() { handleNotFound(); });        // When a client requests an unknown URI (i.e. something other than "/"), call function "handleNotFound"
  server.begin();                           // Actually start the server
}

bool DeviceWebServer::RecordStartupTime() {
  if(startup_time != 0)
    return true;  // startup time already recorded.
  
  time_t now = time(NULL);
  if(now >= NTP_MIN_VALID_EPOCH) {
    startup_time = now;
    return true;
  }
  
  return false; // Not yet getting a valid NTP time.
}

// ----------------------------------------------------------------------
//  Fixed page content, using progmem to keep it out of RAM.

const char DEFAULT_PAGE_HEADER[] PROGMEM = 
"<!DOCTYPE HTML>"
"<html>"
"<head>"
"<meta name = \"viewport\" content = \"width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0\">"
"<style>"
"body { font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }"
"</style>";

const char INDEX_PAGE_HEADER[] PROGMEM = 
"<title>Beer Brew Monitor</title>"
"</head>"
"<body><h1>Beer Brew Monitor</h1>"
"<script src=\"https://cdn.jsdelivr.net/npm/chart.js\"></script>";  // May move this locally later

const char CONFIGURE_PAGE_HEADER[] PROGMEM = 
"<title>Beer Brew Monitor - Configuration</title>"
"</head>"
"<body><h1>Beer Brew Monitor</h1>";

const char HTML_FOOTER[] PROGMEM = 
"</body>"
"</html>";

// ----------------------------------------------------------------------
//  Static Helpers

String prepareConfigurationForm(const DeviceConfig& config) {
  String formContent = F("<form action=\"/configure\" method=\"post\">"
    "<label for=\"minset\">Heater on below : </label>"
    "<input type=\"text\" id=\"minset\" name=\"minset\" value=\"");
  formContent += String(config.relay_on_below_temp,1);
  formContent += F("\" size=\"4\"><p>"
    "<label for=\"maxset\">Heater off above : </label>"
    "<input type=\"text\" id=\"maxset\" name=\"maxset\" value=\"");
  formContent += String(config.relay_off_above_temp, 1);
  formContent += F("\" size=\"4\"><p>");

  formContent += F("<label for=\"cloudUrl\">Cloud API URL : </label>"
    "<input type=\"text\" id=\"cloudUrl\" name=\"cloudUrl\" value=\"");
  formContent += config.cloudLoggingUrl;
  formContent += F("\" size=\"30\"><p>");

  formContent += F("<label for=\"cloudApiKey\">Cloud API key : </label>"
    "<input type=\"text\" id=\"cloudApiKey\" name=\"cloudApiKey\" value=\"");
  // formContent += cloudLoggingApiKey;   -- Intentionally not re-exposing.
  formContent += F("********\" size=\"30\"><p>");

  formContent += F("<label for=\"cloudInstanceId\">Cloud InstanceId : </label>"
    "<input type=\"text\" id=\"cloudInstanceId\" name=\"cloudInstanceId\" value=\"");
  formContent += config.cloudInstanceId;
  formContent += F("\" size=\"30\"><p>");
  
  formContent += F("<input type=\"submit\" value=\"Save\"></form>");
  return formContent;
}

String prepareControlButtonsForm() {
  String formContent = F("<form action=\"/configure\" method=\"post\">"
  "<input type=\"submit\" name=\"resetminmax\" value=\"Reset min and max\"> "
  "<input type=\"submit\" name=\"resetall\" value=\"Reset All Values\"> "
  "<input type=\"submit\" name=\"resetwifi\" value=\"Reset Wifi\"> "
  "</form>");
  return formContent;
}

String prepareRootCertsForm() {
  String formContent = F("<form action=\"/rootcert\" method=\"post\" enctype=\"multipart/form-data\">Root Certificate ");
  formContent += LittleFS.exists(CloudInterface::ROOT_CERT_FILE) ? F("(loaded): ") : F("(none): ");
  formContent += F("<input type=\"file\" name=\"name\"> "
    "<input type=\"submit\" class=\"button\" value=\"Upload\"> "
    "</form>");
  return formContent;  
}

String DeviceWebServer::getUpTime() {
  unsigned long uptime_total_seconds = time(NULL) - startup_time;
  int days = uptime_total_seconds / 86400;
  uptime_total_seconds -= days * 86400;
  int hours = uptime_total_seconds / 3600;
  uptime_total_seconds -= hours * 3600;
  int minutes = uptime_total_seconds / 60;
  uptime_total_seconds -= minutes * 60;

  char tempBuf[20];
  sprintf_P(tempBuf, PSTR("%d days %02d:%02d:%02d"), days, hours, minutes, uptime_total_seconds);
  return String(tempBuf);
}


String getTimeFromSampleIndex(int sampleIdx) {
  int sample_time_minutes = sampleIdx * MINUTES_PER_SAMPLE;
  char tempBuf[6];
  sprintf(tempBuf, "%02d:%02d", sample_time_minutes / 60, sample_time_minutes % 60);
  return String(tempBuf);
}

String DeviceWebServer::getChartHtml(int startAtSample) {
  String labels;
  String data_values;
  int sample;
  for(sample = startAtSample; sample < NUM_SAMPLES; sample++) {
    if(sample > startAtSample) {
      labels += ",";  data_values += ",";
    }
    labels += "\"" + getTimeFromSampleIndex(sample) + "\"";
    data_values += String(samplesRef.sample_average_temps[sample], 1);
  }
  
  for(sample = 0; sample < startAtSample; sample++) {
    labels += ",\"" + getTimeFromSampleIndex(sample) + "\"";
    data_values += "," + String(samplesRef.sample_average_temps[sample], 1);  
  }
  
  String chartHtml = F("<div><canvas id=\"tempChart\" style=\"max-height=300px\"></canvas></div>"
    "<script>const ctx = document.getElementById('tempChart');"
    "new Chart(ctx, {"
      "type: 'line',"
      "data: {"
       "labels: [");
  chartHtml += labels;
  chartHtml += F(
       "],"
       "datasets: [{"
         "label: 'temperature (C)',"
         "data: [");
  chartHtml += data_values;
  chartHtml += F(
         "],"
         "borderWidth: 1"
       "}]"
      "},"
     "options: {"
     " scales: {"
      " y: { "
         " suggestedMin: 15, "
         " suggestedMax: 30"
       " } "
     " } "
   " } "
  " }); "
  "</script>");
  return chartHtml;
}

void DeviceWebServer::handleRoot() {
  String response = FPSTR(DEFAULT_PAGE_HEADER);
  response += FPSTR(INDEX_PAGE_HEADER);
  time_t nowtime = time(NULL);
  response += "Time: ";
  response += ctime(&nowtime);
  response += "<p>";
  response += "Uptime: " + getUpTime() + "<p>";
  response += samplesRef.GetTempSummary() + "<p>";
  
  int startAt = samplesRef.GetCurrentSampleIndex() + 1;
  if(startAt >= NUM_SAMPLES) {
    startAt = 0;
  }
  response += getChartHtml(startAt);
  response += "<p>";
  response += F("<a href=\"/configure\">Configure</a>");
  response += FPSTR(HTML_FOOTER);
  server.send(200, "text/html", response);   // Send HTTP status 200 (Ok) and send some text to the browser/client
}

void DeviceWebServer::handleConfigure() {
  if(server.hasArg("minset")) {
    return processConfigSet();
  }

  if(server.hasArg("resetminmax")) {
    return processResetMinMaxTemps();
  }

  if(server.hasArg("resetall")) {
    return processResetSamples();
  }

  if(server.hasArg("resetwifi")) {
    if(onResetWifi)
      onResetWifi();  // this should reset the device and not return
    return redirectBackToRoot();
  }
  
  String response = FPSTR(DEFAULT_PAGE_HEADER);
  response += FPSTR(CONFIGURE_PAGE_HEADER);
  time_t nowtime = time(NULL);
  response += "Time: ";
  response += ctime(&nowtime);
  response += "<p>";
  
  response += prepareConfigurationForm(configRef) + "<p>";
  response += prepareRootCertsForm() + "<p>";
  response += prepareControlButtonsForm() + "<p>";
  response += F("<a href=\"/\">back to main page</a>");
  response += FPSTR(HTML_FOOTER);
  server.send(200, "text/html", response);   // Send HTTP status 200 (Ok) and send some text to the browser/client 
}

void DeviceWebServer::redirectBackToRoot() {
  // This clears the browser's "form data", so that refreshing the page doesn't re-submit
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void DeviceWebServer::processResetMinMaxTemps() {
  samplesRef.ResetMinMaxTemps();
  samplesRef.WriteToFS();
  redirectBackToRoot();
}

void DeviceWebServer::processResetSamples() {
  samplesRef.ClearAll();
  samplesRef.WriteToFS();
  redirectBackToRoot();
}

void DeviceWebServer::processConfigSet() {
  String minsetvalue = server.arg("minset");
  String maxsetvalue = server.arg("maxset");
  String cloudUrlValue = server.arg("cloudUrl");
  String cloudApiKeyValue = server.arg("cloudApiKey");
  String cloudInstanceIdValue = server.arg("cloudInstanceId");
  
  float fminsetvalue = minsetvalue.toFloat();
  float fmaxsetvalue = maxsetvalue.toFloat();

  if(minsetvalue < maxsetvalue) {
    configRef.relay_on_below_temp = fminsetvalue;
    configRef.relay_off_above_temp = fmaxsetvalue;

    configRef.cloudLoggingUrl = cloudUrlValue;

    if(!cloudApiKeyValue.startsWith("**"))
    {
      configRef.cloudLoggingApiKey = cloudApiKeyValue;
    }
    configRef.cloudInstanceId = cloudInstanceIdValue;
    
    // Store the new values (along with everything else)
    configRef.WriteToFS();  
  }
  redirectBackToRoot();
}

void DeviceWebServer::handleRootCertUpload() {
  HTTPUpload& upload = server.upload();
  
  if(upload.status == UPLOAD_FILE_START) {
    rootCertUploadFile = LittleFS.open(CloudInterface::ROOT_CERT_FILE, "w");
    if(rootCertUploadFile) Serial.println("file created");
  } else if(upload.status == UPLOAD_FILE_WRITE) {
    if(rootCertUploadFile) {
      rootCertUploadFile.write(upload.buf, upload.currentSize);
    } else {
      Serial.println("RootCertUpload: no file to write to");
    }
  } else if(upload.status == UPLOAD_FILE_END) {
    if(rootCertUploadFile) {
      rootCertUploadFile.flush();
      rootCertUploadFile.close();
      server.sendHeader("Location", "/", true);
      server.send(303);
      if(onCertChanged) {
        onCertChanged();
      }
    } else {
      Serial.println("RootCertUpload: No file to close.");
      server.send(500, "text/plain", "500: Couldn't create file");
    }
  }
}

void DeviceWebServer::handleTestCode() {
    String result = "not implemented";
    if(onTestCall) {
      result = onTestCall();    
    }
    server.send(200, "text/plain", result);
};

void DeviceWebServer::handleDirList() {
  Dir dirList = LittleFS.openDir("/");
  String result = "Listing of /\n";
  while(dirList.next())
  {
    result += dirList.fileName() + " " + dirList.fileSize() + "\n";
  }
  server.send(200, "text/plain", result);
}

void DeviceWebServer::handleNotFound() {
  server.send(404, "text/plain", "404: Not found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}
