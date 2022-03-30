#include <Wire.h>
#include <TimeLib.h>
#include <SoftWire.h>
#include <AsyncDelay.h>

//i2c ADCS Stuff
int frameLen = 0;
bool unixTime = false;
//end of ADCS stuff

/* Start of SoftWire i2c */

#define PIN_WIRE_SDA 5
#define PIN_WIRE_SCL 6

int sdaPin = PIN_WIRE_SDA;
int sclPin = PIN_WIRE_SCL;


// I2C address of DS1307
const uint8_t I2C_ADDRESS = 0x68;

SoftWire sw(sdaPin, sclPin);
// These buffers must be at least as large as the largest read or write you perform.
char swTxBuffer[16];
char swRxBuffer[16];

AsyncDelay readInterval;
/* end of softwire i2c */

//helper function to decode decimal to binary coded decimal
//used to help write the time to the rtc
uint8_t dec2bcd(uint8_t num){
  return( (num/10 * 16) + (num%10));
}

//helper function to decode binary decoded decimal to decimal
//used to help read the time to the rtc
uint8_t bcd2dec(uint8_t num){
  return ( (num/16 * 10) + (num%16) );
}

static uint8_t bcd2bin(uint8_t val) { return val - 6 * (val >> 4); }
static uint8_t bin2bcd(uint8_t val) { return val + 6 * (val / 10); }

static uint8_t dowToDS3231(uint8_t d) { return d == 0 ? 7 : d; }

const uint8_t daysInMonth[] PROGMEM = {31, 28, 31, 30, 31, 30,
                                       31, 31, 30, 31, 30};

uint8_t dayWeek(uint16_t y, uint8_t m, uint8_t d){
   if (y >= 2000U)
    y -= 2000U;
  uint16_t days = d;
  for (uint8_t i = 1; i < m; ++i)
    days += pgm_read_byte(daysInMonth + i - 1);
  if (m > 2 && y % 4 == 0)
    ++days;
  uint16_t day = days + 365 * y + (y+3) / 4 - 1;
  return (day + 6) % 7;
}

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

// Print with leading zero, as expected for time
void printTwoDigit(int n)
{
  if (n < 10) {
    Serial.print('0');
  }
  Serial.print(n);
}

//function that reads the virtual i2c bus for the rtc and returns the current time
time_t readTime()
{
  // Ensure register address is valid
  sw.beginTransmission(I2C_ADDRESS);
  sw.write(uint8_t(0)); // Access the first register
  sw.endTransmission();

  uint8_t registers[7]; // There are 7 registers we need to read from to get the date and time.
  int numBytes = sw.requestFrom(I2C_ADDRESS, (uint8_t)7);
  for (int i = 0; i < numBytes; ++i) {
    registers[i] = sw.read();
  }
  if (numBytes != 7) {
    Serial.print("Read wrong number of bytes: ");
    Serial.println((int)numBytes);
    return now();
  }

  tmElements_t  tmSet;
  tmSet.Second = bcd2dec(registers[0] & 0x7f);
  tmSet.Minute = bcd2dec(registers[1]);
  tmSet.Hour = bcd2dec(registers[2]);
  tmSet.Day = bcd2dec(registers[4]);
  tmSet.Month = bcd2dec(registers[5] & 0x7F);
  tmSet.Year = (bcd2dec(registers[6]));

  tmSet.Wday = dayWeek(tmSet.Year, tmSet.Month, tmSet.Day);

  Serial.print("read ");
  for (int i = 7; i >= 0; i--) {
    Serial.print(registers[i], HEX);
  }
  Serial.println();

  // ISO8601 is the only sensible time format
  /*
  Serial.print("Time: ");
  Serial.print(tmSet.Year);
  Serial.print('-');
  printTwoDigit(tmSet.Month);
  Serial.print('-');
  printTwoDigit(tmSet.Day);
  Serial.print(" (");
  Serial.print(daysOfTheWeek[tmSet.Wday]);
  Serial.print(") ");
  Serial.print('T');
  printTwoDigit(tmSet.Hour);
  Serial.print(':');
  printTwoDigit(tmSet.Minute);
  Serial.print(':');
  printTwoDigit(tmSet.Second);
  Serial.println();
  */

  return makeTime(tmSet);
}
void writeTime(time_t t){
  uint8_t registers[7];

  registers[0] = bin2bcd(second(t));
  registers[1] = bin2bcd(minute(t));
  registers[2] = bin2bcd(hour(t));
  registers[3] = bin2bcd(dowToDS3231(weekday(t)));
  registers[4] = bin2bcd(day(t));
  registers[5] = bin2bcd(month(t));
  registers[6] = bin2bcd((year(t)-1970));

  sw.beginTransmission(I2C_ADDRESS); 
  sw.write((uint8_t)0x00);  //ds3231 time register
  Serial.print("writ ");
  for (int i = 0; i < 7; i++) {
    Serial.print(registers[i], HEX);
    registers[i] = sw.write(registers[i]);
  }
  sw.endTransmission();
  Serial.println();

  //flip the Oscillatior Stop Flag bit
  sw.beginTransmission(I2C_ADDRESS); 
  sw.write((uint8_t)0x0F);  //ds3231 status register
  sw.endTransmission();
  uint8_t statreg = sw.read();
  statreg &= ~0x80;
  sw.beginTransmission(I2C_ADDRESS); 
  sw.write(statreg);  //ds3231 status register
  sw.endTransmission();
}

void setup() {
  Serial.begin(115200);
  sw.setTxBuffer(swTxBuffer, sizeof(swTxBuffer));
  sw.setRxBuffer(swRxBuffer, sizeof(swRxBuffer));
  sw.setDelay_us(5);
  sw.setTimeout(1000);
  sw.begin();
  readInterval.start(2000, AsyncDelay::MILLIS);

  // Join I2C bus as slave with address 57
  //Bat2 addy is 0x2D
  Wire.begin(0x57);
  
  // Call receiveEvent when data received                
  Wire.onReceive(receiveEvent);

  Wire.onRequest(sendData);

  //setup the RTC connection
  setSyncProvider(readTime);
  if(timeStatus()!=timeSet)
    Serial.println("Unable to sync w the RTC");
  else
    Serial.println("RTC has set system time");
}

//i2c wire receiveEvent
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
    time_t t = sec;
    writeTime(t);
  }
}

//i2c wire onrequest event
void sendData(){
  if(unixTime){
    //Serial.println("sending time");
    //send the current time to the obc
    time_t t = readTime();
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
    for(int i=0; i<6; i++){
      Serial.println("Wrote: " + String(y[i]));
    }
  }
  else{
    Serial.println("About to write data");
    Wire.write("Q");
  }
}

void loop() {
  if(readInterval.isExpired()){
    time_t t = readTime();
    Serial.print("Time: ");
    Serial.print(year(t));
    Serial.print('-');
    printTwoDigit(month(t));
    Serial.print('-');
    printTwoDigit(day(t));
    Serial.print(" (");
    Serial.print(daysOfTheWeek[weekday(t)]);
    Serial.print(") ");
    Serial.print('T');
    printTwoDigit(hour(t));
    Serial.print(':');
    printTwoDigit(minute(t));
    Serial.print(':');
    printTwoDigit(second(t));
    Serial.println();
    Serial.print("TIME: ");
    Serial.print(t);
    Serial.println();
    readInterval.restart();
  }
}
