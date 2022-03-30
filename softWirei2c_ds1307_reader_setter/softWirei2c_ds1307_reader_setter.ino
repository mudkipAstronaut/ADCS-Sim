#include <SoftWire.h>
#include <AsyncDelay.h>
#include <TimeLib.h>

#define PIN_WIRE_SDA 5
#define PIN_WIRE_SCL 6

#if defined(PIN_WIRE_SDA) && defined(PIN_WIRE_SCL)
int sdaPin = PIN_WIRE_SDA;
int sclPin = PIN_WIRE_SCL;

#else
int sdaPin = SDA;
int sclPin = SCL;
#endif

// I2C address of DS1307
const uint8_t I2C_ADDRESS = 0x68;

SoftWire sw(sdaPin, sclPin);
// These buffers must be at least as large as the largest read or write you perform.
char swTxBuffer[16];
char swRxBuffer[16];

AsyncDelay readInterval;


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

  int tenYear = (registers[6] & 0xf0) >> 4;
  int unitYear = registers[6] & 0x0f;
  int year = (10 * tenYear) + unitYear;

  int tenMonth = (registers[5] & 0x10) >> 4;
  int unitMonth = registers[5] & 0x0f;
  int month = (10 * tenMonth) + unitMonth;

  int tenDateOfMonth = (registers[4] & 0x30) >> 4;
  int unitDateOfMonth = registers[4] & 0x0f;
  int dateOfMonth = (10 * tenDateOfMonth) + unitDateOfMonth;

  // Reading the hour is messy. See the datasheet for register details!
  bool twelveHour = registers[2] & 0x40;
  bool pm = false;
  int unitHour;
  int tenHour;
  if (twelveHour) {
    pm = registers[2] & 0x20;
    tenHour = (registers[2] & 0x10) >> 4;
  } else {
    tenHour = (registers[2] & 0x30) >> 4;
  }
  unitHour = registers[2] & 0x0f;
  int hour = (10 * tenHour) + unitHour;
  if (twelveHour) {
    // 12h clock? Convert to 24h.
    hour += 12;
  }

  int tenMinute = (registers[1] & 0xf0) >> 4;
  int unitMinute = registers[1] & 0x0f;
  int minute = (10 * tenMinute) + unitMinute;

  int tenSecond = (registers[0] & 0xf0) >> 4;
  int unitSecond = registers[0] & 0x0f;
  int second = (10 * tenSecond) + unitSecond;

  tmElements_t  tmSet;
  tmSet.Year = year;
  tmSet.Month = month;
  tmSet.Day = dateOfMonth;
  tmSet.Hour = hour;
  tmSet.Minute = minute;
  tmSet.Second = second;

  Serial.print("read ");
  for (int i = 0; i < 7; i++) {
    Serial.print(registers[i], HEX);
  }
  Serial.println();
  
  return makeTime(tmSet);
}

void writeTime(time_t t){
  // Ensure register address is valid
  sw.beginTransmission(I2C_ADDRESS);
  sw.write(uint8_t(0)); // Access the first register
  sw.endTransmission();

  uint8_t registers[7]; // There are 7 registers we need to read from to get the date and time.

  int y = year(t)-1970;
  uint8_t unitY = y%10;
  uint8_t tenY = y/10;
  registers[6] = (tenY << 4 ) & 0xf0;
  registers[6] +=  (unitY & 0x0f);

  int mo = month(t);
  uint8_t unitMo = mo%10;
  uint8_t tenMo = mo/10;
  registers[5] = (tenMo << 4 ) & 0x10;
  registers[5] +=  (unitMo & 0x0f);

  int dateofMonth = day(t);
  uint8_t unitDoM = dateofMonth%10;
  uint8_t tenDoM = dateofMonth/10;
  registers[4] = (tenDoM << 4 ) & 0x30;
  registers[4] +=  (unitDoM & 0x0f);

  int h = hour(t);
  uint8_t unitH = h%10;
  uint8_t tenH = h/10;
  registers[2] = (tenH << 4 ) & 0x10;
  registers[2] +=  (unitH & 0x0f);

  int m = minute(t);
  uint8_t unitM = m%10;
  uint8_t tenM = m/10;
  registers[1] = (tenM << 4 ) & 0xf0;
  registers[1] +=  (unitM & 0x0f);

  int s = second(t);
  uint8_t unitS = s%10;
  uint8_t tenS = s/10;
  registers[0] = (tenS << 4 ) & 0xf0;
  registers[0] +=  (unitS & 0x0f);

  registers[3] = 1;
  

  sw.beginTransmission(I2C_ADDRESS); 
  sw.write(uint8_t(0));
  Serial.print("writ ");
  for (int i = 7; i >=0; i--) {
    Serial.print(registers[i], HEX);
    registers[i] = sw.write(registers[i]);
  }
  sw.endTransmission();
  Serial.println();
}

void setup(void)
{
  Serial.begin(115200);
  Serial.println("Read DS1307 example");
  Serial.print("    SDA pin: ");
  Serial.println(int(sdaPin));
  Serial.print("    SCL pin: ");
  Serial.println(int(sclPin));
  Serial.print("    I2C address: ");
  Serial.println(int(I2C_ADDRESS), HEX);
  sw.setTxBuffer(swTxBuffer, sizeof(swTxBuffer));
  sw.setRxBuffer(swRxBuffer, sizeof(swRxBuffer));
  sw.setDelay_us(5);
  sw.setTimeout(1000);
  sw.begin();
  readInterval.start(2000, AsyncDelay::MILLIS);

  writeTime(now());
}

void loop(void)
{
  if (readInterval.isExpired()) {
    time_t t = readTime();
    Serial.print("ReadTime: ");
    Serial.print(t);
    Serial.println();
    Serial.print("Arduino Time: ");
    Serial.print(now());
    Serial.println();
    readInterval.restart();
  }
}
