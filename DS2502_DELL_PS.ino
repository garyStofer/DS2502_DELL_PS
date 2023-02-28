/*

Programmer for DS2502 device used in  DELL 90Watt power supply to identify the power adapter as a 90W, 19.5V 4.6A DELL AC adapter

This sketch needs a DS2502 programmer interface board attached to arduino digital pins 6 and 7, fed with a 12V PS capable of 100ma
This sketch is running on a Pro-mini running at 16mhz, 5V from the on board 12 to 5V regulator and has been verified under Arduino 1.6.1 IDE, 

Eagle CAD files for programmer board can be downloaded from Github here: https://github.com/garyStofer/DS2502_DELL_PS
PCB boards can be ordered from OSH PCB here: https://oshpark.com/shared_projects/l4VC80Px

Sketch requires library  PJRC OneWire 2.0 library http://www.pjrc.com/teensy/td_libs_OneWire.html

Sketch is roughly based on the One-Wire example code provided  by Guillermo Lovato <glovato@gmail.com>

 The DS250x is a 512/1024bit add-only PROM(you can add data but cannot change the old one) that's used mainly for device identification purposes
 like serial number, mfgr data, unique identifiers, etc. It uses the Maxim 1-wire bus.

Use: Upon power-up of the Arduino the code is looking for an attached DS2502 device in a 3 second loop. if the device is found it will be programmed with the 
text in progStr and after wards the programmed block will be read back, bot in hex and as a string. The program then stops executing until reset is pressed.
Programming a device multiple times will not hurt or corrupt the device unless a different programming string or address is chosen.

Progress can be monitored via the Serial terminal in the IDE

 
 Sept. 2015 , Gary Stofer, GaryStofer@gmail.com
 This file archived at https://github.com/garyStofer/DS2502_DELL_PS

*/

#include <OneWire.h>

#define DOPROGRAM

#define LED_PIN 10
#define PROG_PIN 7
#define ONE_WIRE_IO_PIN 6
#define DS2502DevID 0x09
// DS250x ROM commands
#define READ_ROM   0x33
#define MATCH_ROM  0x55
#define SKIP_ROM   0xCC
#define SEARCH_ROM 0xF0
#define ROM_SZ 8  // 8 bytes of laser etched ROM data , LSB = 0x09 == DS2502

// DS250x Memory commands -- can only be executed after one of the above ROM commands has been executed properly
#define READ_MEMORY  0xF0
#define READ_STATUS  0xAA
#define WRITE_STATUS 0x55
#define WRITE_MEMORY 0x0F

#define ProgLocation 0x00

#define ProgPulseUs 480    // length of time 12V prog pulse needs to be applied for 
OneWire ds(ONE_WIRE_IO_PIN);             // OneWire bus on digital pin 6
void setup()
{
     Serial.begin (115200);
    // Pin 13 has an LED connected on most Arduino boards:
    pinMode(LED_PIN, OUTPUT); 
     
    // Pin 7 is the PRGM/ output, sending a low enables the programming voltage +12V onto the wire
    digitalWrite(PROG_PIN, HIGH);   // set Non Programming mode
    pinMode(PROG_PIN,OUTPUT);
}

// This is the magic string Dell laptops need to see from the device in order to not go into limp home mode 
// The interesting bit is the wattage 090 followed by the voltage 1950 and the amperage 4.6 
char *progStr ="DELL00AC090195046CN09T";

// This function parks the MCU in an infinite while loop 
void 
lockup( void)
{
  Serial.println("Stopped!!");
  while (1) 
  {
    delay (10);
  }
  
}

// This function switches the 12V programming signal on for the 480us duration and then off again
void 
ProgPulse( void)
{    
  digitalWrite(PROG_PIN, LOW);  
  delayMicroseconds(ProgPulseUs);
  digitalWrite(PROG_PIN, HIGH); 
  delayMicroseconds(100); 
}

// The Arduino never ending loop 
void loop()
{
    byte i;                     
   
    byte data[32];                  // container for the data from device
    byte leemem[12];                // array with the commands to initiate a read, DS250x devices expect 3 bytes to start a read: command,LSB&MSB adresses

    byte crc;                      // Variable to store the command CRC
    byte crc_calc;

    digitalWrite(LED_PIN, LOW);   // set the LED off
    
    if (ds.reset())            // We only try to read the data if there's a device present
    {
        digitalWrite(LED_PIN, HIGH);   // LED on
        Serial.println("device present");
        
        // Read and display ROM data of device attached and check CRC 
        ds.write( READ_ROM,1);
        Serial.println("ROM  Data is: ");  
        for ( i = 0; i < 8; i++)     
        {
            data[i] = ds.read();        
            Serial.print(data[i],HEX); 
            Serial.print(" ");          
        }
        
        Serial.println(".");
        crc_calc = OneWire::crc8(data, 7);
        
        if (crc_calc == data[7] )
        {
          Serial.println("ROM CRC matches ");
          if (data[0] != DS2502DevID)
          {
             Serial.print( data[0],HEX);
             Serial.println(" -- Device ID does not match DS2502"); 
   //          lockup();
          }
          else
            Serial.println("Device ID  DS2502 found"); 
        }
        else
        {
             Serial.println("ROM CRC failed"); 
             lockup();
        }
#ifdef DOPROGRAM        
        Serial.println("PROG Memory");        
        ds.reset(); 
        ds.skip();  
        leemem[0] = WRITE_MEMORY;       // command to initiate a write seq using array for later CRC calc
        leemem[1] = ProgLocation;       // low address byte
        leemem[2] = 0;                  // high address is always 0
        leemem[3] = progStr[0];         // first byte of data
        ds.write_bytes(leemem,4,1);        // write command,address and data, leave ghost power on

        crc = ds.read();            // DS250x responds with crc over command and address and data
        crc_calc = OneWire::crc8(leemem, 4);
        if ( crc_calc != crc)   
        {
            Serial.println("Invalid command CRC on write!");
            Serial.print("Calculated CRC:");
            Serial.println(crc_calc,HEX);  
            Serial.print("DS250x readback CRC:");
            Serial.println(crc,HEX);
            lockup();
        }
        else
        {
          ProgPulse();
        }
          
        data[0] = ds.read();
        Serial.print("programmed data:  ");
        Serial.print(progStr[0],HEX); 
        Serial.print(" read back: ");
        Serial.println(data[0],HEX);        
          
       // loop for subsequent bytes -- CRC calculation is based on incremented LSB of address and data only now
       for (i = 1; i < strlen(progStr); i++)
       {
          leemem[1]++;    // increment address for parallel crc calculation 
          leemem[2]=progStr[i];  // next data byte
          ds.write(leemem[2],1);  // write data byte only
          crc = ds.read();            // DS250x responds with crc over command and address and data
          /*
          crc_calc = OneWire::crc8(&leemem[1], 2); // calc crc over address byte and data 
          
          CRC calculation doesn't match what I get back from device, mismatch most likely because I don't know how to calc subsequent CRCs
             Programming however works correctly
          if ( crc_calc != crc)   
            {
                Serial.println("Invalid command CRC on subsequent write!");
                Serial.print("Calculated CRC:");
                Serial.println(ccrc_calc,HEX);  
                Serial.print("DS250x readback CRC:");
                Serial.println(ccrc,HEX);
                lockup();
            }
          else
          */
        {
            ProgPulse();
        }
        
          data[0] = ds.read();
          Serial.print("programmed data:  ");
          Serial.print(progStr[i],HEX); 
          Serial.print(" read back: ");
          Serial.println(data[0],HEX);   
             
       }
#endif          
                  
   
        Serial.println("Reading Memory");        
        ds.reset(); 
        ds.skip();                      // Skip ROM search, address the device no matter what
        leemem[0] = READ_MEMORY;        // command 
        leemem[1] = ProgLocation;       // low address byte
        leemem[2] = 0;                  // High address byte, always 0 for 2502
        ds.write(leemem[0],1);        // Read data command, leave ghost power on
        ds.write(leemem[1],1);        // LSB starting address, leave ghost power on
        ds.write(leemem[2],1);        // MSB starting address, leave ghost power on

        crc = ds.read();             // DS250x generates a CRC for the command we sent, we assign a read slot and store it's value
        crc_calc = OneWire::crc8(leemem, 3);  // We calculate the CRC of the commands we sent using the library function and store it
 
        if ( crc_calc != crc)        // Then we compare it to the value the ds250x calculated, if it fails, we print debug messages and abort
        {
            Serial.println("Invalid command CRC!");
            Serial.print("Calculated CRC:");
            Serial.println(crc_calc,HEX);    // HEX makes it easier to observe and compare
            Serial.print("DS250x readback CRC:");
            Serial.println(crc,HEX);
            lockup();                      // Since CRC failed, we abort 
        }
        
        Serial.println("Data is: "); 
        for ( i = 0; i < 32; i++)     
        {
            data[i] = ds.read();        
            Serial.print(data[i],HEX);       
            Serial.print(" ");           
        }
        Serial.println();
        data[strlen(progStr)]=0;
        Serial.println((char *) data);  // string
        lockup();
    }
    else                             // Nothing is connected in the bus
    {
        Serial.println("Nothing connected");
        delay(3000);
    }
}


















