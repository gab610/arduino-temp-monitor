#include <SPI.h>


#include <Ethernet.h> // Used for Ethernet
#include <OneWire.h>     // Used for temperature sensor(s)

// **** ETHERNET SETTING ****
// Ethernet MAC address - must be unique on your network
byte mac[] = { 0x54, 0x34, 0x41, 0x30, 0x30, 0x31 };                                      
// ethernet interface IP address (unique in your network)
IPAddress ip(192, 168, 168, 246);                        
// ethernet interface IP port (80 = http)
EthernetServer server(80);

EthernetClient client;

// **** TEMPERATURE SETTINGS ****
// Sensor(s) data pin is connected to Arduino pin 2 in non-parasite mode!
OneWire  ds(2);

void setup() {
  // Open serial communications:
  Serial.begin(9600);

  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  server.begin();

  Serial.println("Tweaking4All.com - Temperature Drone - v1.0");
  Serial.println("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
  Serial.print("IP Address: ");
  Serial.println(Ethernet.localIP());
  Serial.println();
}

void loop() {
  // listen for incoming clients
  client = server.available();

  if (client)
  {  
    Serial.println("-> New Connection\n");

    // a http request ends with a blank line
    boolean currentLineIsBlank = true;

    while (client.connected())
    {
      if (client.available()) {
        char c = client.read();

        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank)
        {
          client.println("<?xml version='1.0'?>\n<sensordata>");
          Serial.println("   Collecting Sensor Data:");
          TemperaturesToXML();
          client.println("</sensordata>");
          Serial.println("\n   Done Collecting Sensor Data");
          break;
        }

        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        }
        else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }

    // give the web browser time to receive the data
    delay(10);

    // close the connection:
    client.stop();
    Serial.println("   Disconnected\n");
  }
}

void TemperaturesToXML(void) {
  byte counter;
  byte present = 0;
  byte sensor_type;
  byte data[12];
  byte addr[8];
  float celsius, fahrenheit;

  ds.reset_search();

  while ( ds.search(addr)) {
    client.println("<sensor>");

    // Get Serial number
    client.print("  <serial>");
    Serial.print("\n   Sensor     : ");

    for( counter = 0; counter < 8; counter++)
    {
      if (addr[counter]<10) client.print("0");
      client.print(String(addr[counter], HEX));
      client.print(" ");

      if (addr[counter]<10) Serial.print("0");
      Serial.print(String(addr[counter], HEX));
      Serial.print(" ");
    }
    Serial.println();

    client.println("</serial>");

    // Check CRC
    if (OneWire::crc8(addr, 7) != addr[7])
    {
        client.println("  <status>Invalid CRC</status>\n</sensor>");
        Serial.println("   ERROR\n");
        return;
    }

    client.println("  <status>OK</status>");

    // Get Chip type (the first ROM byte indicates which chip)
    client.print("  <chip>");

    switch (addr[0])
    {
      case 0x10:
        client.println("DS18S20");
        sensor_type = 1;
        break;
      case 0x28:
        client.println("DS18B20");
        sensor_type = 0;
        break;
      case 0x22:
        client.println("DS1822");
        sensor_type = 0;
        break;
      default:
        client.println("undefined</chip>");
        return;
    }
    client.println("</chip>");

    ds.reset();
    ds.select(addr);
    ds.write(0x44);  // start conversion, with regular (non-parasite!) power

    delay(1000);     // maybe 750ms is enough, maybe not

    present = ds.reset();
    ds.select(addr);    
    ds.write(0xBE);  // Read Scratchpad

    // Get Raw Temp Data, we need 9 bytes
    for ( counter = 0; counter < 9; counter++)
    {          
      data[counter] = ds.read();
    }

    // Convert the data to actual temperature
    // because the result is a 16 bit signed integer, it should
    // be stored to an "int16_t" type
    int16_t raw = (data[1] << 8) | data[0];

    if (sensor_type)
    {
      raw = raw << 3; // 9 bit resolution default
      if (data[7] == 0x10) {
        // "count remain" gives full 12 bit resolution
        raw = (raw & 0xFFF0) + 12 - data[6];
      }
    }
    else
    {
      // at lower res, the low bits are undefined, so let's zero them
      byte cfg = (data[4] & 0x60);

      //// default is 12 bit resolution, 750 ms conversion time
      if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
      else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
      else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    }

    celsius = (float)raw / 16.0;
    fahrenheit = celsius * 1.8 + 32.0;

    client.print("  <celsius>");
    client.print(celsius);
    client.print("</celsius>\n  <fahrenheit>");
    client.print(fahrenheit);
    client.println("</fahrenheit>");

    Serial.print("   Temperature: ");
    Serial.print(celsius);
    Serial.print(" C  (");
    Serial.print(fahrenheit);
    Serial.println(" F)");

    client.println("</sensor>");
  }

  return;
}
