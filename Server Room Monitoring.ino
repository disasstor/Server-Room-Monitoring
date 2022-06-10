#include <SPI.h>
#include <Ethernet.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
#include "Wire.h"
#include "SHT31.h"
#include "GyverTimer.h"



#define LCD_ADDRESS 0x3F 			// i2c адрес LCD
#define LCD_H 16 					// количество горизонтальных ячеек
#define LCD_V 2	 					// количество вертикальных ячеек
#define SHT31_ADDRESS 0x44 			// i2c адрес SHT3X
#define ONE_WIRE_BUS 2 				// Шина onewire DT18B20
#define TEMPERATURE_PRECISION 10 	// Точность преобразования температуры DT18B20
#define MAX_COMMAND_LENGTH 32 		// Максимальная длина команды Zabbix
#define MEASUREMENTDELTA 10000 		// Интервал опроса датчиков
#define LCDINTERVAL 5000 			// Интервал обновления дисплея

GTimer myTimer1(MS, LCDINTERVAL);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensdt(&oneWire);
SHT31 sht;
uint32_t start;
uint32_t stop;

LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_H, LCD_V); // i2c адрес LCD

byte mac[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
IPAddress ip(192, 168, 0, 100);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
EthernetServer server(10050);
EthernetClient client;

boolean alreadyConnected = false;
String command; // Буфер команд Zabbix
unsigned int temperature[] = { 0, 0, 0, 0, 0, 0 };
static unsigned long lcdcounter = 5000;
int lcdflag = 0;
boolean sensping[] = { 0, 0, 0, 0, 0, 0 };
unsigned int humidity = 0;
unsigned int voltage[] = { 0, 0, 0, 0 };
unsigned int flagLCD = 0;

DeviceAddress addrsensdt[] = { // Адреса датчиков DS18B20
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
  { 0x28, 0x66, 0x66, 0x83, 0x18, 0x20, 0x01, 0x43 },
  { 0x28, 0xC0, 0x69, 0x8C, 0x18, 0x20, 0x01, 0x58 },
  { 0x28, 0x2C, 0xBA, 0xC3, 0x18, 0x20, 0x01, 0x66 },
  { 0x28, 0x09, 0x0F, 0xA9, 0x18, 0x20, 0x01, 0x2D },
  { 0x28, 0x7E, 0x30, 0x83, 0x18, 0x20, 0x01, 0xB0 },
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

void readsensSHT3x() { // Получаем данные с SHT3X
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

void readsensdt(byte sensdtindex) { // Получаем данные с DS18B20
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

void readtcpstream(char character) { // Парсинг приходящих от заббикс данных
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

      command.remove(command.indexOf('Z'), (command.length() - command.indexOf('Z'))); // находим индекс символа Z в строке, вычитаем из длинны строки индекс Z, таким образом из строки command удаляем все, начиная с символа Z
      Serial.print("Receive Data from Zabbix: ");
      Serial.println(command);
      zbxexecutecommand();
    }
    command = "";
  }
}

void lcdprint() { // Вывод данных на экран
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

void zbxexecutecommand() { // Сбор и отправка пакета данных в Заббикс
  client.print("ZBXD\x01"); // Отправляем заголовок ZBXD\x01 первым пакетом
  
  if (command.equals("GetData")) { //Если приходящая от Заббикс команда равна GetData, выполнить цикл IF
    String StrSpacer = ",";
    String SendStrData = "";
    
    for (int i = 0; i <= 5; i++) { // Собираем в цикле FOR строку с данными всех датчиков температуры
      String TempStrData = temperature[i] + StrSpacer;
      SendStrData = SendStrData + TempStrData; 
    }
    SendStrData = SendStrData + humidity; // Дописываем в строку данные датчика влажности
    byte responseBytes [] = {(byte) SendStrData.length(), 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // Собираем второй пакет данный для отправки в Заббикс
    client.write(responseBytes, 8); // Отправляем второй пакет данных
    client.print(SendStrData); // Отправляем сами данные в Заббикс
    client.stop(); // Закрываем соединение
  } else { }
  alreadyConnected = false;
}

void setup() {
  Serial.begin(9600);
  Serial.println("Server Room Monitoring 1.0");
  Serial.println("Initialize sensors...");
  Wire.begin();
  sensdt.begin(); // Start DS18D20
  sht.begin(SHT31_ADDRESS);
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

  lcd.init();                      // инициализация дисплея
  lcd.backlight();                 // включение подсветки
  lcd.clear();                     // очистка экрана
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
