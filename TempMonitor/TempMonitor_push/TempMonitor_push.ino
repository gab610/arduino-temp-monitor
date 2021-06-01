#include <SPI.h>
#include <Ethernet.h> // Used for Ethernet
#include <OneWire.h>     // Used for temperature sensor(s)
#include <stdlib.h>


#define DEBUG

// **** ETHERNET SETTING ****
// Ethernet MAC address - must be unique on your network - MAC Reads T4A001 in hex (unique in your network)
byte mac[] = { 0x54, 0x34, 0x41, 0x30, 0x30, 0x31 };                                       
//IPAddress ip(192, 168, 168, 246);

EthernetClient client;
char server[] = "192.168.168.174"; // IP Adres (or name) of server to dump data to
unsigned long PreviousMillis = 0;// For when millis goes past app 49 days. 
//unsigned long interval = 10000;  // Wait between dumps (10000 = 10 seconds)
unsigned long interval = 60000;  // Wait between dumps (1 min)
unsigned long intervalTime;      // Global var tracking Interval Time

// **** TEMPERATURE SETTINGS ****
// Sensor(s) data pin is connected to Arduino pin 2 in non-parasite mode!
OneWire  ds(2);

void setup() {
  #ifdef DEBUG
    Serial.begin(9600); // only use serial when debugging
  #endif
  
  Ethernet.begin(mac); //, ip);
  
  intervalTime = millis();  // Set initial target trigger time (right NOW!)
  
  #ifdef DEBUG
    Serial.println("Tweaking4All.com - Temperature Drone - v2.3");
    Serial.println("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
    Serial.print("IP Address        : ");
    Serial.println(Ethernet.localIP());
    Serial.print("Subnet Mask       : ");
    Serial.println(Ethernet.subnetMask());
    Serial.print("Default Gateway IP: ");
    Serial.println(Ethernet.gatewayIP());
    Serial.print("DNS Server IP     : ");
    Serial.println(Ethernet.dnsServerIP());
  #endif
} 

void loop() {
  unsigned long CurrentMillis = millis();
  
  if ( CurrentMillis < PreviousMillis ) // millis reset to zero?
  {
    intervalTime = CurrentMillis+interval;
  }
  
  if ( CurrentMillis > intervalTime )  // Did we reach the target time yet?
  {
    intervalTime = CurrentMillis + interval;
    
    if (!client.connect(server, 8080)) {
      #ifdef DEBUG
        Serial.println("-> Connection failure detected: Resetting ENC!");  // only use serial when debugging
      #endif
      //Enc28J60.init(mac);
    } else {
      client.stop();
    } 
    
    // if you get a connection, report back via serial:
    if (client.connect(server, 8080)) 
    {
      #ifdef DEBUG
        Serial.println("-> Connected");  // only use serial when debugging
      #endif
      
      // Make a HTTP request:
      client.print( "POST /");
      
      char temp[40];
      char t[10];
      float x = GetTemperature();     
      dtostrf(x, 6, 2, t);
      sprintf(temp, "Temp=%s", t);
      
      #ifdef DEBUG
      Serial.println(temp);
      #endif
      
      client.println( " HTTP/1.1");
      //client.println( "Host: 192.168.1.100" );
      client.print(" Host: ");
      client.println(server);
      client.println("Content-Type: application/x-www-form-urlencoded");
      client.println( "Connection: close" );
      client.print("Content-Length: ");
      client.println(strlen(temp));
      client.println();
      client.print(temp);
      client.println();
      client.stop();
    }
    else 
    {
      // you didn't get a connection to the server:
      #ifdef DEBUG
        Serial.println("--> connection failed !!");  // only use serial when debugging
      #endif
      
      //Enc28J60.init(mac);
    }
  }
  else
  {
    Ethernet.maintain();
  }
}

float GetTemperature(void) {
  byte counter;
  byte present = 0;
  byte sensor_type;
  byte data[12];
  byte addr[8];
  float celsius, fahrenheit;

  ds.reset_search();

  while ( ds.search(addr)) {

    #ifdef DEBUG
    Serial.print("\n   Sensor     : ");
    for( counter = 0; counter < 8; counter++)
    {
      if (addr[counter]<10) Serial.print("0");
      Serial.print(String(addr[counter], HEX));
      Serial.print(" ");
    }
    Serial.println();
    #endif
    
    // Check CRC
    if (OneWire::crc8(addr, 7) != addr[7])
    {
        Serial.println("   ERROR\n");
        return 0;
    }

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

    #ifdef DEBUG
    Serial.print("   Temperature: ");
    Serial.print(celsius);
    Serial.print(" C  (");
    Serial.print(fahrenheit);
    Serial.println(" F)");
    #endif 
  }

  return fahrenheit;
}

char *float2s(float f, unsigned int digits=2)
{
  static char buf[16];
  return dtostre(f, buf, digits, 1);
}
