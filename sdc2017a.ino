#include <ESP8266WiFi.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <WiFiClient.h>

#include <SPI.h>

#include "SH1106.h"
#include "SSD1306.h"

#include "Adafruit_MCP23008.h"

extern "C" {
  #include "user_interface.h"
} 


ADC_MODE(ADC_VCC);          // So we can measure battery level.
SH1106 display(0x3c, 5,4);  // Our Display
Adafruit_MCP23008 mcp;      // IO Port Expander (mainly Joystick)

WiFiManager wifiManager;    // So people can move this to any network they want
WiFiUDP UdpOut;             // UDP is da bomb

// CONFIGURATION ZONE
const char*     server = "evil.red";
unsigned int localPort = 2390;
const char*        ver = "3.1";

// Globals
int loopMe = 0;
int bu = 0;
int bd = 0;
int bl = 0;
int br = 0;
int bp = 0;
int bb = 1;
String inData;

byte packetBuffer[2000];

void setup()
{
  mcp.begin();
  mcp.pinMode(0, INPUT);
  mcp.pullUp(0, LOW);
  mcp.pinMode(1, INPUT);
  mcp.pullUp(1, LOW);
  mcp.pinMode(2, INPUT);
  mcp.pullUp(2, LOW);
  mcp.pinMode(3, INPUT);
  mcp.pullUp(3, LOW);
  mcp.pinMode(4, INPUT);
  mcp.pullUp(4, LOW);

  mcp.pinMode(5, OUTPUT);
  mcp.pinMode(6, OUTPUT); 
  mcp.pinMode(7, OUTPUT);

  mcp.digitalWrite(5, HIGH);
  mcp.digitalWrite(6, HIGH);
  mcp.digitalWrite(7, HIGH);

  UdpOut.begin(localPort);
  
  Serial.begin(115200);
  while (!Serial);

  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);  // Only used during boot seq,
                                      // because, display is server driven

  Serial.print("Calling bootloader...\n");
  displayBootloader();

  while(WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.println("nope still broke");
  }

  Serial.println("I appear to be connected, we'll see");
  display.clear();

  display.drawString(0,0,"SSID: ");
  display.drawString(33,0,WiFi.SSID());

  char message[50];
  IPAddress ip = WiFi.localIP();
  sprintf(message, " ipAddr: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  display.drawString(0,16, message);

  ip = WiFi.subnetMask();
  sprintf(message, "netmask: %d,%d,%d.%d", ip[0], ip[1], ip[2], ip[3]);
  display.drawString(0,26, message);

  ip = WiFi.gatewayIP();
  sprintf(message, "gateway: %d,%d,%d.%d", ip[0], ip[1], ip[2], ip[3]);
  display.drawString(0,36, message);

  sprintf(message, "Version: %s", ver);
  display.drawString(0,46, message);
  
  display.display();
  sendPacket("COLDBOOT");  // This is a signal to the server
  sendPacket("COLDBOOT");  // that a new badge is online.


}

void loop() {
  float volt = 0.00f;
  delay(75);
  loopMe++;

  if (loopMe > 89) {
    char pmessage[50];
    sprintf(pmessage, "%s%d", "BATT", ESP.getVcc());
    sendPacket(pmessage);
    loopMe = 0;
  }

  if (loopMe == 100) {  // This of course should never happen
    ESP.restart();
  }
 
  int noBytes = UdpOut.parsePacket();
  if ( noBytes ) {
    Serial.print(".");

    UdpOut.read(packetBuffer,noBytes); // read the packet into the buffer

    const char *image = reinterpret_cast<const char*>(packetBuffer);

    display.clear();   
    display.drawXbm(0,0,128,64, image);
    display.display();
    
    memset(packetBuffer,128,2000);  
  };

  // Pay no attention to the function behind the curtain
  while (Serial.available() > 0) {
    char received = Serial.read();
    inData += received;

    if (received == '\n') {
      char s[8];
      sprintf(s, "MK%s", inData.c_str());
      sendPacket(s);
      inData = "";
    }
  }


  if ((digitalRead(0) == 1) && (bb == 0)) {
    Serial.println("B up");
    sendPacket("BU");
    bb = 1;
  }

  if ((digitalRead(0) == 0) && (bb == 1)) {
    Serial.println("B down");
    sendPacket("BD");
    bb = 0;
  }

  if ((mcp.digitalRead(0) == 1) && (bp == 0)) {    
    Serial.println("P down");
    sendPacket("PD");
    bp = 1;
  }
  if ((mcp.digitalRead(0) == 0) && (bp == 1)) {    
    Serial.println("P up");
    sendPacket("PU");
    bp = 0;
  }
  if ((mcp.digitalRead(1) == 1) && (bu == 0)) {    
    Serial.println("U down");
    sendPacket("UD");
    bu = 1;
  }
  if ((mcp.digitalRead(1) == 0) && (bu == 1)) {    
    Serial.println("U up");
    sendPacket("UU");
    bu = 0;
  }
  if ((mcp.digitalRead(3) == 1) && (bd == 0)) {    
    Serial.println("D down");
    sendPacket("DD");
    bd = 1;
  }
  if ((mcp.digitalRead(3) == 0) && (bd == 1)) {    
    Serial.println("D up");
    sendPacket("DU");
    bd = 0;
  }
  if ((mcp.digitalRead(2) == 1) && (bl == 0)) {    
    Serial.println("L down");
    sendPacket("LD");
    bl = 1;
  }
  if ((mcp.digitalRead(2) == 0) && (bl == 1)) {    
    Serial.println("L up");
    sendPacket("LU");
    bl = 0;
  }
  if ((mcp.digitalRead(4) == 1) && (br == 0)) {    
    Serial.println("R down");
    sendPacket("RD");
    br = 1;
  }
  if ((mcp.digitalRead(4) == 0) && (br == 1)) {    
    Serial.println("R up");
    sendPacket("RU");
    br = 0;
  }

}

void sendPacket(const char *data) { 
  int id = ESP.getChipId();
  char message[50];
  sprintf(message, "%010d%s", id, data);
  UdpOut.beginPacket(server, 2391);
  UdpOut.write(message);
  UdpOut.endPacket();
}

void displayBootloader() {

  for (int i = 5 ; i > 0 ; i--) {
    char message[40];
    char passwd[40];

    display.drawString(0,0,"Connecting to:");
    display.drawString(13,13,WiFi.SSID());
    display.drawString(0,33, "Press button to switch!");

    sprintf(message, "Connecting in ...%d", i);
    display.drawString(0,43, message);
    display.display();

    if ( digitalRead(0) == LOW ) {
      int id = ESP.getChipId();
      sprintf(message, "SSID%d", id);
      
      sprintf(passwd, "%d", id);
      display.drawString(0,53, passwd);
      display.display();

      Serial.print("Starting WiFi manager\n");
      wifiManager.resetSettings();
      wifiManager.startConfigPortal(message);
      
    }
   
    delay(1000);
    display.clear();
    
  };

  display.display();
};


