#include <Arduino.h>
#include <U8x8lib.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif

// Time offset for Thailand
int timeOffset = 7 * 3600;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", timeOffset);

// Required for the display using u8x8 library
U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/ 15, /* data=*/ 4, /* reset=*/ 16);

bool my_status;
int PM25 = 0;
int PM10 = 0;
int sampleInterval = 5000;
int unitId = 1;
int wifiTimeoutMax = 10;

// For epoch time
long epochTime = 0;

// Wifi stuff, case sensitive
const char* ssid = "xxxxxxx"; 
const char* password = "xxxxxxx";

// Web server handle
WebServer server(80);

String webPage;

void setup() {
  // Turn off the flashing LED
  digitalWrite(25, 0);
  
  // Start the OLED
  u8x8.begin();
  u8x8.setFont(u8x8_font_chroma48medium8_r);

  // Connect the monitor serial port
  Serial.begin(115200);
  u8x8.draw2x2String(4, 1, "\x2d\x2d\x2d\x2d");
  u8x8.draw2x2String(4, 5, "\x2d\x2d\x2d\x2d");
  delay(1000);

  // Connect the sensor serial port
  Serial2.begin(9600, SERIAL_8N1, 18, 17);
  u8x8.draw2x2String(4, 3, "I");
  delay(500);

  // Stop sensor autosend
  my_status = stop_autosend();
  u8x8.draw2x2String(6, 3, "O");
  delay(500);

  Serial.print("Stop autosend status is ");
  Serial.println(my_status, BIN);
  Serial.println(" ");

  // Start fan and start sampling
  my_status = start_measurement();
  u8x8.draw2x2String(8, 3, "T");
  delay(500);

  u8x8.draw2x2String(10, 3, "A");
    
  // Setup wifi and connect
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int wifiTimeoutCounter = 0;
  // Wait for connection
  while ((WiFi.status() != WL_CONNECTED) && (wifiTimeoutCounter < wifiTimeoutMax)) {
    delay(1000);
    Serial.print(".");
    wifiTimeoutCounter++;
    Serial.print(wifiTimeoutCounter);
  }

  // Do the network stuff if connected. Disable network functions if not.
  if(WiFi.status() == WL_CONNECTED){
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    u8x8.clearDisplay();
    u8x8.drawString(0, 0, "Wifi Connected");
    delay(1000);

    // Display the IP
    u8x8.clearDisplay();
    String wifiIpStr = String(WiFi.localIP()[0]);
    String wifiIpStr1 = String(WiFi.localIP()[1]);
    String wifiIpStr2 = String(WiFi.localIP()[2]);
    String wifiIpStr3 = String(WiFi.localIP()[3]);

    wifiIpStr += ".";
    wifiIpStr += wifiIpStr1;
    wifiIpStr += ".";
    wifiIpStr += wifiIpStr2;
    wifiIpStr += ".";
    wifiIpStr += wifiIpStr3;

    char wifiIpChar[16];
    wifiIpStr.toCharArray(wifiIpChar, sizeof(wifiIpChar));
    u8x8.drawString(0, 0, "IP Address");
    u8x8.drawString(0, 1, wifiIpChar);

    // Initialize the NTP client
    timeClient.begin();
    u8x8.drawString(0, 2, "NTP Connected");
    delay(10000);

    // Start the webserver
    server.on("/", handleRoot);

    server.onNotFound(handleNotFound);
    server.begin();
    Serial.println("HTTP server started");
  } else {
    u8x8.clearDisplay();
    u8x8.drawString(0, 2, "No Network");
    delay(2000);
  }

  // Almost ready
  u8x8.clearDisplay();
}

void loop() {
  // Read the particle data, wait for valid data
  do{
    my_status = read_measurement();
    Serial.print("Read measurement status is ");
    Serial.println(my_status, BIN);
    if(my_status == 1){
      Serial.print("PM2.5 value is ");
      Serial.println(PM25, DEC);
      Serial.print("PM10 value is ");
      Serial.println(PM10, DEC);
    } else {
      PM25 = 0;
      PM10 = 0;
      delay(1000);
    }
    Serial.println(" ");
  }while(my_status == 0);
  
  if(WiFi.status() == WL_CONNECTED){
    // Webserver
    server.handleClient();

    // Update time
    timeClient.update();
    epochTime = timeClient.getEpochTime();

    // Get the date and time convert to char
    String curMonth = String(month(epochTime));
    String curDay = String(day(epochTime));
    String curYear = String(year(epochTime));
    String curHour = String(hour(epochTime));
    String curMinute = String(minute(epochTime));

    String dateTimeStr = curMonth;
    dateTimeStr += "/";
    dateTimeStr += curDay;
    dateTimeStr += "/";
    dateTimeStr += curYear;
    dateTimeStr += " ";
    dateTimeStr += curHour;
    dateTimeStr += ":";
    dateTimeStr += curMinute;

    // Display time if available
    char dtgChar[16];
    dateTimeStr.toCharArray(dtgChar, sizeof(dtgChar));
    u8x8.drawString(0, 0, dtgChar);
    
  } else {
    // Display message if no network connection
    String dateTimeStr = "No Network";
    char dtgChar[16];
    dateTimeStr.toCharArray(dtgChar, sizeof(dtgChar));
    u8x8.drawString(3, 0, dtgChar);
  }

  // Convert PM2.5 to string
  String pm25Str = String(PM25);

  // Convert PM10 to string
  String pm10Str = String(PM10);

  // Make the char array for the data
  int screenSize = 8;

  // Complicated way of formatting the data display
  String leftRightPad = " ";
  String dataStr = leftRightPad;
  dataStr += pm25Str;
  for (int startFor = 0; startFor < screenSize - (pm25Str.length() + pm10Str.length() + ( 2 * leftRightPad.length())); startFor++) {
    dataStr += " ";
  }
  dataStr += pm10Str;
  dataStr += leftRightPad;

  // Make the data a char for display
  char dataChar[16];
  dataStr.toCharArray(dataChar, sizeof(dataChar));

  // Display the data line
  u8x8.draw2x2String(0, 3, dataChar);

  // Make the footer
  String footerStr = " PM25      PM10 ";
  char footerChar[16];
  footerStr.toCharArray(footerChar, sizeof(footerChar));

  // Display the footer
  u8x8.drawString(0, 7, footerChar);

  if(WiFi.status() == WL_CONNECTED){
    // Send the data to the web server,
    /**
      {
      "measurement":{
        "unitId":"1",
        
        "epochTime":"1549805422",
        "data":[
          {
            "PM25":"22"
          },
          {
            "PM10":"23
          }
        ]
        }
      }
    */
    webPage = "{\n\"measurement\":\{";
    webPage += "\n\t\"unitId\":";
    webPage += "\"";
    webPage += String(unitId);
    webPage += "\"\,";
    webPage += "\n\t\"epochTime\":";
    webPage += "\"";
    webPage += String(epochTime);
    webPage += "\"\,\n\t";
    webPage += "\"data\":\[\n\t\t\{\n\t\t\t\"PM25\":";
    webPage += "\"";
    webPage += pm25Str;
    webPage += "\"\n\t\t\}\,\n\t\t\{\n\t\t\t\"PM10\":";
    webPage += "\"";
    webPage += pm10Str;
    webPage += "\n\t\t\}\n\t\]\n\t\}\n\}";
  }

  // Wait
  delay(sampleInterval);
}

bool stop_autosend(void)
{
  // Stop auto send
  byte stop_autosend[] = {0x68, 0x01, 0x20, 0x77 };
  Serial2.write(stop_autosend, sizeof(stop_autosend));
  //Then we wait for the response
  while (Serial2.available() < 2);
  byte read1 = Serial2.read();
  byte read2 = Serial2.read();
  // Test the response
  if ((read1 == 0xA5) && (read2 == 0xA5)) {
    // ACK
    return 1;
  }
  else if ((read1 == 0x96) && (read2 == 0x96))
  {
    // NACK
    return 0;
  }
  else return 0;
}

bool start_measurement(void)
{
  // First, we send the command
  byte start_measurement[] = {0x68, 0x01, 0x01, 0x96 };
  Serial2.write(start_measurement, sizeof(start_measurement));
  //Then we wait for the response
  while (Serial2.available() < 2);
  byte read1 = Serial2.read();
  byte read2 = Serial2.read();
  // Test the response
  if ((read1 == 0xA5) && (read2 == 0xA5)) {
    // ACK
    return 1;
  }
  else if ((read1 == 0x96) && (read2 == 0x96))
  {
    // NACK
    return 0;
  }
  else return 0;
}

bool read_measurement (void)
{
  // Send the command 0x68 0x01 0x04 0x93
  byte read_particle[] = {0x68, 0x01, 0x04, 0x93 };
  Serial2.write(read_particle, sizeof(read_particle));
  // A measurement can return 0X9696 for NACK
  // Or can return eight bytes if successful
  // We wait for the first two bytes
  while (Serial2.available() < 1);
  byte HEAD = Serial2.read();
  while (Serial2.available() < 1);
  byte LEN = Serial2.read();
  // Test the response
  if ((HEAD == 0x96) && (LEN == 0x96)) {
    // NACK
    Serial.println("NACK");
    return 0;
  }
  else if ((HEAD == 0x40) && (LEN == 0x05))
  {
    // The measuremet is valid, read the rest of the data
    // wait for the next byte
    while (Serial2.available() < 1);
    byte COMD = Serial2.read();
    while (Serial2.available() < 1);
    byte DF1 = Serial2.read();
    while (Serial2.available() < 1);
    byte DF2 = Serial2.read();
    while (Serial2.available() < 1);
    byte DF3 = Serial2.read();
    while (Serial2.available() < 1);
    byte DF4 = Serial2.read();
    while (Serial2.available() < 1);
    byte CS = Serial2.read();
    // Now we shall verify the checksum
    if (((0x10000 - HEAD - LEN - COMD - DF1 - DF2 - DF3 - DF4) % 0XFF) != CS) {
      Serial.println("Checksum fail");
      return 0;
    }
    else
    {
      // Checksum OK, we compute PM2.5 and PM10 values
      PM25 = DF1 * 256 + DF2;
      PM10 = DF3 * 256 + DF4;
      return 1;
    }
  }
}

void handleRoot() {
  server.send(200, "text/plain", webPage);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}
