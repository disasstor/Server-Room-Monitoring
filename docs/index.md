# What is this?
###### Server Room Monitoring - a device for monitoring the temperature and humidity of servers, server rooms and just for rooms where environmental control is needed.


------------

##### The differences between this system and Zabbuino in open source for the Arduino IDE. Anyone can customize the device to suit their needs. 
##### You can remove the display if display control is not needed, or change it to any other display by rewriting the code a bit. 
##### You can add or remove any sensor, for example, you can control the content of carbon dioxide or dust particles in the air, everything that you can implement on Arduino can be done here.

------------


# Electronic circuit:
![Shema](circuit.png)

# Components that I used:
###### ARDUINO UNO R3 - BASE

###### ETHERNET SHIELD W5100 - EXTEND SHIELD FOR ARDUINO

###### SHT31 - TEMPERATURE AND HUMIDITY SENSOR

###### DS18B20 - TEMPERATURE SENSOR

###### LCD 1602 I2C - DYSPLAY

###### RESISTOR 4.7K - PULL-UP RESISTOR

###### D6MG DIN RAIL MOUNTING ENCLOSURE - BOX FOR ARDUINO

###### RJ45 CAT5 DUAL PORT SURFACE MOUNT BOX - BOX FOR SENSORS

# Based Settings:

------------

##### You'll need a scanner i2c to find the display and sht31 addresses
##### and you will need scanner onewire to find ds18b20 addresses 

------------

