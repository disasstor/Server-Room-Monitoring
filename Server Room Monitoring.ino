#include <SPI.h>
#include <Ethernet.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
#include "Wire.h"
#include "SHT31.h"
#include "GyverTimer.h"



#define LCD_ADDRESS 0x3F            // i2c address of display
#define LCD_H 16                    // number of horizontal screen characters
#define LCD_V 2                     // number of vertical screen characters
#define SHT31_ADDRESS 0x44 		      // i2c address of SHT3X
#define ONE_WIRE_BUS 2 			        // Onewire DT18B20 bus
#define TEMPERATURE_PRECISION 10  	// DT18B20 temperature conversion accuracy
#define MAX_COMMAND_LENGTH 32 	    // Maximum Zabbix command length
#define MEASUREMENTDELTA 10000 	    // Sensor polling interval
#define LCDINTERVAL 5000 		        // Display refresh interval

String ItemKey = "GetData"          // Key of Zabbix item. Read more about item keys here: https://www.zabbix.com/documentation/current/en/manual/config/items/item

GTimer myTimer1(MS, LCDINTERVAL);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensdt(&oneWire);
SHT31 sht;
uint32_t start;
uint32_t stop;

LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_H, LCD_V);

byte mac[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };		// Important! Change MAC address! It must be unique on your local network.
IPAddress ip(192, 168, 0, 100);						              // Change IP address if you need.
IPAddress gateway(192, 168, 0, 1);					            // Change GW if you need.
IPAddress subnet(255, 255, 255, 0);					            // Change MASK if you need.
EthernetServer server(10050);							              // Change port if you need.
EthernetClient client;

boolean alreadyConnected = false;
String command; 									                      // Zabbix command buffer
unsigned int temperature[] = { 0, 0, 0, 0, 0, 0 };
static unsigned long lcdcounter = 5000;
int lcdflag = 0;
boolean sensping[] = { 0, 0, 0, 0, 0, 0 };
unsigned int humidity = 0;
unsigned int voltage[] = { 0, 0, 0, 0 };
unsigned int flagLCD = 0;

DeviceAddress addrsensdt[] = {                        // OneWire sensor address array(you need to change the address to yours)
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },	// Must be emty (0x00)
  { 0x28, 0x66, 0x66, 0x83, 0x18, 0x20, 0x01, 0x43 },	// Address of the first sensor
  { 0x28, 0xC0, 0x69, 0x8C, 0x18, 0x20, 0x01, 0x58 },	// Address of the second sensor
  { 0x28, 0x2C, 0xBA, 0xC3, 0x18, 0x20, 0x01, 0x66 },	// Address of the third sensor
  { 0x28, 0x09, 0x0F, 0xA9, 0x18, 0x20, 0x01, 0x2D },	// Address of the fourth sensor
  { 0x28, 0x7E, 0x30, 0x83, 0x18, 0x20, 0x01, 0xB0 },	// Address of the fifth sensor
};

void readsens() {
  static unsigned long measurementdelta = MEASUREMENTDELTA;
  static unsigned long prevmillis = 0;
  static byte measurementiteration = 97;

  if ((millis() - prevmillis) >= measurementdelta) {
    prevmillis = millis();
    measurementiteration = 0;
  }

  if (measurementiteration == 0) {
    readsensSHT3x();
  }

  if (measurementiteration == 32) {
    for (byte i = 1; i <= 5; i++) {
      readsensdt(i);
    }
  }

  if (measurementiteration == 64) {
    readvoltages();
  }

  if (measurementiteration == 96) {
    sensdt.requestTemperatures();
  }

  if (measurementiteration <= 96) {
    measurementiteration = measurementiteration + 1;
  }
}
// Get data from SHT3X
void readsensSHT3x() {
  start = micros();
  sht.read();         // default = true/fast       slow = false
  stop = micros();
  float ttemperature = sht.getTemperature();
  float thumidity = sht.getHumidity();
  if (isnan(ttemperature) || isnan(thumidity)) {
    sensping[0] = 0;
    temperature[0] = 0;
    humidity = 0;
    Serial.println("Failed to read from SHT3X sensor!");
  }
  else
  {
    sensping[0] = 1;
    temperature[0] = ttemperature;
    humidity = thumidity;
  }
}
// Get data from DS18B20
void readsensdt(byte sensdtindex) {
  float ttemperature = sensdt.getTempC(addrsensdt[sensdtindex]);
  if (ttemperature == -127) {
    sensping[sensdtindex] = 0;
    temperature[sensdtindex] = 0;
    Serial.print("Failed to read temperature from DT.");
    Serial.print(sensdtindex);
    Serial.println(" sensor!");
  }
  else if (ttemperature == 85) {
    if (sensping[sensdtindex] == 0) {
      sensdt.setResolution(addrsensdt[sensdtindex], TEMPERATURE_PRECISION);
    }
    sensping[sensdtindex] = 1;
    Serial.print("Sensor DT.");
    Serial.print(sensdtindex);
    Serial.println(" not ready. Skip");
  }
  else {
    if (sensping[sensdtindex] == 0) {
      sensdt.setResolution(addrsensdt[sensdtindex], TEMPERATURE_PRECISION);
    }
    sensping[sensdtindex] = 1;
    temperature[sensdtindex] = ttemperature;
  }
}

void readvoltages() {
  for (int i = 0; i <= 3; i++) {
    voltage[i] = 0;
  }
}
// Parsing data coming from zabbix
void readtcpstream(char character) {
  if (command.length() == MAX_COMMAND_LENGTH) {
    command = "";
  }
  command += character;
  if (character == 10 || character == 0) {
    if (command.length() > 8) {

//      Serial.print("Receive Raw Data from Zabbix : ");
//      Serial.println(command);

//      Serial.print("Position Z: ");
//      Serial.println(command.indexOf('Z'));

//      Serial.print("RemoveCount: ");
//      Serial.println((command.length() - command.indexOf('Z')));

      command.remove(command.indexOf('Z'), (command.length() - command.indexOf('Z'))); // find the index of the character "Z" in the string, subtract the index "Z" from the length of the string, thus deleting everything from the command string, starting with the character "Z"
      Serial.print("Receive Data from Zabbix: ");
      Serial.println(command);
      zbxexecutecommand();
    }
    command = "";
  }
}
// Display data
void lcdprint() {
  if (myTimer1.isReady()) {
    //Serial.println("Timer LCD");
    lcdflag++;
    if (lcdflag > 3) {lcdflag = 0;}
  }
  if (lcdflag == 0) {
    lcd.setCursor(0, 0);
    lcd.print("Server Room Temp  ");
    lcd.setCursor(0, 1);
    lcd.print("T1:");
    lcd.print(temperature[0]);
    lcd.print(char(223));
    lcd.print("C  ");
    lcd.print("T2:");
    lcd.print(temperature[1]);
    lcd.print(char(223));
    lcd.print("C");
  }
  if (lcdflag == 1) {
    lcd.setCursor(0, 0);
    lcd.print(" Server #1 Temp  ");
    lcd.setCursor(0, 1);
    lcd.print("T3:");
    lcd.print(temperature[2]);
    lcd.print(char(223));
    lcd.print("C  ");
    lcd.print("T4:");
    lcd.print(temperature[3]);
    lcd.print(char(223));
    lcd.print("C");
  }
  if (lcdflag == 2) {
    lcd.setCursor(0, 0);
    lcd.print(" Server #2 Temp   ");
    lcd.setCursor(0, 1);
    lcd.print("T5:");
    lcd.print(temperature[4]);
    lcd.print(char(223));
    lcd.print("C  ");
    lcd.print("T6:");
    lcd.print(temperature[5]);
    lcd.print(char(223));
    lcd.print("C");
  }
  if (lcdflag == 3) {
    lcd.setCursor(0, 0);
    lcd.print("Server Room Hum  ");
    lcd.setCursor(0, 1);
    lcd.print("     H1:");
    lcd.print(humidity);
    lcd.print("%         ");
  }
}

void zbxexecutecommand() { // Collecting and sending a data packet to Zabbix Server
//<HEADER> - "ZBXD\x01" (5 байт)
//<DATALEN> - data length (4 bytes or 8 bytes for large packet). 1 will be formatted as 01/00/00/00 (four bytes, 32 bit number in little-endian format) or 01/00/00/00/00/00/00/00 (eight bytes, 64 bit number in little-endian format) for large packet.
//<RESERVED> - uncompressed data length (4 bytes or 8 bytes for large packet). 1 will be formatted as 01/00/00/00 (four bytes, 32 bit number in little-endian format) or 01/00/00/00/00/00/00/00 (eight bytes, 64 bit number in little-endian format) for large packet.
// Read more about headers here: https://www.zabbix.com/documentation/current/ru/manual/appendix/protocols/header_datalen
  client.print("ZBXD\x01"); // Send header "ZBXD\x01" first
  // If the command coming from Zabbix is equal to ItemKey, execute "IF"
  if (command.equals(ItemKey)) { 
    String StrSpacer = ",";
    String SendStrData = "";
    
    for (int i = 0; i <= 5; i++) { 														// Collects data from all temperature sensors into a string with a separator ","
      String TempStrData = temperature[i] + StrSpacer;
      SendStrData = SendStrData + TempStrData; 
    }
    SendStrData = SendStrData + humidity; 													// Appends the humidity sensor data to the string
    byte responseBytes [] = {(byte) SendStrData.length(), 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // Save datalen
    client.write(responseBytes, 8); 														// Send datalen to Zabbix Server
    client.print(SendStrData); 															// Send data to Zabbix Server
    client.stop(); 																	// Close connection
  } else { }
  alreadyConnected = false;
}

void setup() {
  Serial.begin(9600);
  Serial.println("Server Room Monitoring 1.0");
  Serial.println("Initialize sensors...");
  Wire.begin();
  sensdt.begin(); 				    	// Start DS18D20
  sht.begin(SHT31_ADDRESS); 		    	// Start SHT31_ADDRESS
  Wire.setClock(100000);
  uint16_t stat = sht.readStatus();


  for (int i = 1; i <= 5; i++) {
    sensdt.setResolution(addrsensdt[i], TEMPERATURE_PRECISION);
    sensping[i] = 0;
    temperature[i] = 0;
  }
  for (int i = 1; i <= 5; i++) {
    Serial.print("DT.");
    Serial.print(i);
    Serial.print(" resolution set to ");
    Serial.print(sensdt.getResolution(addrsensdt[i]));
    Serial.println(" bit");
  }
  Serial.println("Initialize ethernet...");
  Ethernet.begin(mac, ip, gateway, subnet); // Start Ethernet
  server.begin();
  Serial.println("Initialize done");

  lcd.init();                      	// Display initialization
  lcd.backlight();                  	// Turn on the backlight
  lcd.clear();                     	// Screen cleaning
}

void loop() {
  client = server.available();
  if (client) {
    if (!alreadyConnected) {
      client.flush();
      alreadyConnected = true;
    }
    if (client.available() > 0) {
      readtcpstream(client.read());
    }
  }
  readsens();
  lcdprint();
  delay(10);
}
