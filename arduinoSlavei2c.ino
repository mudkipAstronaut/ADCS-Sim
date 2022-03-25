#include <Wire.h>
#include <TimeLib.h>
#include <SoftWire.h>
#include <AsyncDelay.h>

//i2c ADCS Stuff
int frameLen = 0;
bool unixTime = false;

void setup() {
  Serial.begin(9600);
  // Join I2C bus as slave with address 57
  //Bat2 addy is 0x2D
  Wire.begin(0x57);
  
  // Call receiveEvent when data received                
  Wire.onReceive(receiveEvent);

  Wire.onRequest(sendData);

  //setup the RTC connection
  /*
  setSyncProvider(RTC.get);
  if(timeStatus()!=timeSet)
    Serial.println("Unable to sync w the RTC");
  else
    Serial.println("RTC has set system time");
    */
}

void receiveEvent(int howMany) {
  Serial.println("Received " + String(howMany) + " bytes of data");
  int count = 0;
  byte x[7];
  
  while (Wire.available()) { // loop through all but the last
    byte b = Wire.read(); // receive byte as a character

    Serial.print("Byte in Hex: ");
    Serial.print(b, HEX);
    Serial.println();

    if(count == 0){
      if(b < 128){
        Serial.println("Telecommand: " + String(b));
      }
      else if(b == 140){
        //get command for current Unix time
          Serial.println("setting return unix time to true");
          unixTime = true;
          break;
      }
      else{
        Serial.println("Telemetry: " + String(b));      
      }
    }

    if(count < 7){
      x[count] = b;
    }
    count++;
  }
  
  if(x[0] == 2){
    //set command for current Unix time
    //time is x[1-6]

    //seconds since 01/01/1970
    unsigned long sec =0;
    sec = (long(x[1]) << 24) | (long(x[2]) << 16) | (long(x[3]) << 8) | (long(x[4]));

    //current millisecond count
    unsigned int milli = 0;
    milli = (x[5]<< 8) | milli;
    milli = (x[6]) | milli;

    Serial.println("Setting time to " + String(sec) + "(s) and " + String(milli) + "ms");
    setTime(sec);
  }
}

void sendData(){
  if(unixTime){
    //Serial.println("sending time");
    //send the current time to the obc
    time_t t = now();
    byte y[6];
  
    unsigned long sec = t;
    y[0] = (sec >> 24) & 0xFF;
    y[1] = (sec >> 16) & 0xFF;
    y[2] = (sec >> 8) & 0xFF;
    y[3] = (sec) & 0xFF;

    //milliseconds
    y[4] = 0;
    y[5] = 0;
    for(int i=0; i<6; i++){
      Wire.write(y[i]);
      //Serial.println("Wrote: " + String(y[i]));
    }
    unixTime = false;
  }
  else{
    Serial.println("About to write data");
    Wire.write("Q");
  }
}

void loop() {
  //Serial.println(now());
  delay(100);
}
