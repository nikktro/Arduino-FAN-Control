#include <ESP_EEPROM.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <OneWire.h>
#include <DallasTemperature.h>

MDNSResponder mdns;

// Data wire is plugged into pin D4 GPIO 2
#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
char temperatureCString[7];
char temperatureDCString[7];

// VARIABLES
const char* ssid = "Area53";
const char* password = "haveaniceday";
const char* devhost = "FAN1";
//IPAddress ip(192,168,1,130);
//IPAddress gateway(192,168,1,1);
//IPAddress subnet(255,255,255,0);
int D5_pin = 14;
int D6_pin = 12;
int DO_pin = 16;
int refresh = 1;

int state;
int eestate;
long previousMillis = 0;
long interval = 10000;
long Day = 0;
int Hour = 0;
int Minute = 0;
int Second = 0;
int HighMillis = 0;
int Rollover = 0;


ESP8266WebServer server(80);

void setup(void){
  // preparing GPIOs
  pinMode(D5_pin, OUTPUT);
  digitalWrite(D5_pin, HIGH);
  pinMode(D6_pin, OUTPUT);
  digitalWrite(D6_pin, HIGH);

  // begin
  DS18B20.begin();
  DS18B20.setResolution(9);
  EEPROM.begin(4);
  delay(100);
  Serial.begin(115200);

  //power restore wait delay
  delay(4000);

  //check pervious state from eeprom
  checkState();

  //connect wi-fi
  ConnectWIFI();

  //mdns
  if (mdns.begin("esp8266", WiFi.localIP())) {
    Serial.println("MDNS responder started");
  }

  //set commands
  server.on("/", [](){
    getTemperature();
    server.send(200, "text/html", webPage());
  });
  server.on("/normal", [](){
    digitalWrite(D5_pin, LOW);
    digitalWrite(D6_pin, HIGH);
    state=1;
    server.send(200, "text/html", webPage());
    delay(100);

  });
  server.on("/high", [](){
    digitalWrite(D5_pin, LOW);
    digitalWrite(D6_pin, LOW);
    state=2;
    server.send(200, "text/html", webPage());
    delay(100);

  });
  server.on("/off", [](){
    digitalWrite(D5_pin, HIGH);
    digitalWrite(D6_pin, HIGH);
    state=0;
    server.send(200, "text/html", webPage());
    delay(100);

  });
  server.on("/temp", [](){
    server.send(200, "text/html", temp());
    delay(100);

  });
  server.on("/pause", [](){
    server.send(200, "text/html", pause());
    delay(100);

  });
  server.on("/resume", [](){
    server.send(200, "text/html", resume());
    delay(100);

  });
  server.on("/state", [](){
    server.send(200, "text/html", checkstate());
    delay(100);

  });
  server.on("/flash", [](){
    server.send(200, "text/html", flash());
    delay(100);

  });
  server.on("/help", [](){
    server.send(200, "text/html", help());
    delay(100);

  });

   server.on("/update", HTTP_POST, [](){
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", (Update.hasError())?"FAIL":"OK");
      ESP.restart();
    },[](){
      HTTPUpload& upload = server.upload();
      if(upload.status == UPLOAD_FILE_START){
        Serial.setDebugOutput(true);
        WiFiUDP::stopAll();
        Serial.printf("Update: %s\n", upload.filename.c_str());
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if(!Update.begin(maxSketchSpace)){//start with max available size
          Update.printError(Serial);
        }
      } else if(upload.status == UPLOAD_FILE_WRITE){
        if(Update.write(upload.buf, upload.currentSize) != upload.currentSize){
          Update.printError(Serial);
        }
      } else if(upload.status == UPLOAD_FILE_END){
        if(Update.end(true)){ //true to set the size to the current progress
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } else {
          Update.printError(Serial);
        }
        Serial.setDebugOutput(false);
      }
      yield();
    });

  server.begin();
  Serial.println("HTTP server started");

}


void ConnectWIFI()
{
  Serial.println("Connecting to WiFi");
  WiFi.hostname(devhost);
  WiFi.begin(ssid, password);
  //WiFi.config(ip, gateway, subnet);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(DO_pin, !digitalRead(DO_pin));
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void getTemperature() {
  float tempC;
  do {
    DS18B20.requestTemperatures();
    tempC = DS18B20.getTempCByIndex(0);
    dtostrf(tempC, 3, 1, temperatureCString);
    dtostrf(tempC*10, 3, 0, temperatureDCString);
    delay(100);
  } while (tempC == 85.0 || tempC == (-127.0));
  Serial.print("Temperature: ");
  Serial.println(tempC);
}


void checkState() {
  state = EEPROM.read(0);
  Serial.print("Start State ");
  Serial.println(state);
  if(state == 0) {
    Serial.println("State OFF");
    digitalWrite(D5_pin, HIGH);
    digitalWrite(D6_pin, HIGH);
   }
  if(state == 1) {
    Serial.println("State Normal");
    digitalWrite(D5_pin, LOW);
    digitalWrite(D6_pin, HIGH);
   }
  if(state == 2) {
    Serial.println("State High");
    digitalWrite(D5_pin, LOW);
    digitalWrite(D6_pin, LOW);
   }
}


String webPage()
{
  //getTemperature();
  String web;
  web += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/> <meta charset=\"utf-8\"><title>ESP FAN</title><style>button{color:red;padding: 10px 27px;}</style></head>";
  web += "<h1 style=\"text-align: center;font-family: Open sans;font-weight: 100;font-size: 20px;\">FAN Control</h1><div>";

  //++++++++++ Temperature  +++++++++++++
  web += "<p style=\"text-align: center;margin-top: 0px;margin-bottom: 5px;\">Temperature: ";
  web += "<b>";
  web += temperatureCString;
  web += "</b>";
  web += " &#8451;</p>";
  //++++++++++ Temperature  +++++++++++++

  //++++++++++ Uptime  +++++++++++++
  web += "<p style=\"text-align: center;margin-top: 0px;margin-bottom: 5px;\">Uptime: ";
  web += Day;
  web += " Day(s) ";
  if (Hour < 10) {web += "0";};
  web += Hour;
  web += ":";
  if (Minute < 10) {web += "0";};
  web += Minute;
  web += ":";
  if (Second < 10) {web += "0";};
  web += Second;
  web += "</p>";
  //++++++++++ Uptime  +++++++++++++

  //++++++++++ FAN Speed  +++++++++++++
  //web += "<p style=\"text-align: center;margin-top: 0px;margin-bottom: 5px;\">FAN Speed:</p>";
  if (digitalRead(D6_pin) == 0)
  {
    web += "<div style=\"text-align: center;width: 98px;color:white ;padding: 10px 30px;background-color: #0066ff;margin: 0 auto;\">HIGH</div>";
  }
  else
  if (digitalRead(D5_pin) == 0)
  {
    web += "<div style=\"text-align: center;width: 98px;color:white ;padding: 10px 30px;background-color: #43a209;margin: 0 auto;\">NORMAL</div>";
  }
  else
  {
    web += "<div style=\"text-align: center;width: 98px;color:white ;padding: 10px 30px;background-color: #ec1212;margin: 0 auto;\">OFF</div>";
  }

  web += "<div style=\"text-align: center;margin: 5px 0px;\"> <a href=\"normal\"><button>NORMAL</button></a>&nbsp;<a href=\"high\"><button>HIGH</button></a><a href=\"off\"><button>OFF</button></a></div>";
  // ++++++++ FAN Speed +++++++++++++

  // ========REFRESH=============
  web += "<div style=\"text-align:center;margin-top: 20px;\"><a href=\"/\"><button style=\"width:158px;\">REFRESH</button></a></div>";
  // ========REFRESH=============

  web += "</div>";
  return(web);
}

String temp()
{
  getTemperature();
  String web;
  web += temperatureDCString;
  return(web);
}

String pause()
{
  Serial.println("Pause Request");
  digitalWrite(D5_pin, HIGH);
  digitalWrite(D6_pin, HIGH);
  String web;
  web += "<p>Paused</p>";
  //web += "<div style=\"text-align:center;margin-top: 20px;\"><a href=\"/\"><button style=\"width:158px;\">Home</button></a></div>";
  return(web);
}

String resume()
{
  //check pervious state from eeprom
  Serial.println("Resume Request");
  checkState();
  String web;
  web += "<p>Resumed</p>";
  //web += "<div style=\"text-align:center;margin-top: 20px;\"><a href=\"/\"><button style=\"width:158px;\">Home</button></a></div>";
  return(web);
}

String checkstate()
{
  String web;
  web += state;
  return(web);
}

String flash()
{
  const char* web = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
  return(web);
}

String help()
{
  String web;
  web += "<p style=\"text-align: center;margin-top: 0px;margin-bottom: 5px;\"><b>Available commands:</b> normal, high, off, temp (current Temperature), pause, resume, state (0-off, 1-normal, 2-high), flash, help</p>";
  web += "<div style=\"text-align:center;margin-top: 20px;\"><a href=\"/\"><button style=\"width:158px;\">Home</button></a></div>";
  return(web);
}

void loop(void){

  //check wifi connection
  if (WiFi.status() != WL_CONNECTED) {
    ConnectWIFI();
  }

  // first run temp poll
  server.handleClient();
  if (refresh == 1) {
    getTemperature();
    refresh = 0;
  }

  //uptime tic
  if(millis()>=3000000000){
    HighMillis=1;
  }
  if(millis()<=100000&&HighMillis==1){
    Rollover++;
    HighMillis=0;
  }
  long secsUp = millis()/1000;
  Second = secsUp%60;
  Minute = (secsUp/60)%60;
  Hour = (secsUp/(60*60))%24;
  Day = (Rollover*50)+(secsUp/(60*60*24));

  //save eeprom
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    //getTemperature();
    eestate = EEPROM.read(0);
    if (state != eestate) {
      EEPROM.put(0, state);
      EEPROM.commit();
      Serial.print("Save EEPROM State to ");
      Serial.println(state);
    }

    previousMillis = currentMillis;
  }

}
