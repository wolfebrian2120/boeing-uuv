/*
 * A simple sketch that uses WiServer to serve a web page
 */


#include <WiServer.h>
#include <Wire.h>

#define WIRELESS_MODE_INFRA	1
#define WIRELESS_MODE_ADHOC	2

// Wireless configuration parameters ----------------------------------------
unsigned char local_ip[] = {192,168,1,122};	// IP address of WiShield
unsigned char gateway_ip[] = {192,168,1,1};	// router or gateway IP address
unsigned char subnet_mask[] = {255,255,255,0};	// subnet mask for the local network
const prog_char ssid[] PROGMEM = {"wolfe"};		// max 32 bytes

unsigned char security_type = 2;	// 0 - open; 1 - WEP; 2 - WPA; 3 - WPA2

// WPA/WPA2 passphrase
const prog_char security_passphrase[] PROGMEM = {"wolfe2120"};	// max 64 characters

// WEP 128-bit keys
// sample HEX keys
prog_uchar wep_keys[] PROGMEM = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,	// Key 0
				  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// Key 1
				  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// Key 2
				  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00	// Key 3
				};

// setup the wireless mode
// infrastructure - connect to AP
// adhoc - connect to another WiFi device
unsigned char wireless_mode = WIRELESS_MODE_INFRA;

unsigned char ssid_len;
unsigned char security_passphrase_len;
// End of wireless configuration parameters ----------------------------------------


//Preassure Sensor Stuff

int I2C_ADDRESS = 0x77;  // sensor address

// oversampling setting
// 0 = ultra low power
// 1 = standard
// 2 = high
// 3 = ultra high resolution
const unsigned char oversampling_setting = 3; //oversampling for measurement
const unsigned char pressure_conversiontime[4] = { 
  5, 8, 14, 26 };  // delays for oversampling settings 0, 1, 2 and 3   

// sensor registers from the BOSCH BMP085 datasheet
int ac1;
int ac2; 
int ac3; 
unsigned int ac4;
unsigned int ac5;
unsigned int ac6;
int b1; 
int b2;
int mb;
int mc;
int md;

// variables to keep the values
int temperature = 0;
long pressure = 0;


//WiServer Stuff
const prog_char httpOK[]={"Request is Good\n"};
const prog_char httpNotFound[]={"HTTP request not found\n"};


// This is our page serving function that generates web pages
boolean sendMyPage(char* URL) {
  
    // Check if the requested URL matches "/"
    if (strcmp(URL, "/") == 0) {
        // Use WiServer's print and println functions to write out the page content
        WiServer.print("<html>");
        WiServer.print("Hello World!");
        WiServer.print((int)local_ip[0]);
        WiServer.print(".");
        WiServer.print((int)local_ip[1]);
        WiServer.print(".");
        WiServer.print((int)local_ip[2]);
        WiServer.print(".");
        WiServer.print((int)local_ip[3]);
        WiServer.print(" is my IP");
        WiServer.print("<br/>");
        WiServer.print("Temperature: ");
        WiServer.print(temperature,DEC);
        WiServer.print(" Pressure: ");
        WiServer.println(pressure,DEC);
        WiServer.print("</html>");
        
        // URL was recognized
        return true;
    }
    // URL not found
    return false;
}


void setup() {
  
  Serial.begin(9600);  
  Wire.begin();
  getCalibrationData();
  // Initialize WiServer and have it use the sendMyPage function to serve pages
  WiServer.init(sendMyPage);
  
  // Enable Serial output and ask WiServer to generate log messages (optional)
  Serial.begin(57600);
  WiServer.enableVerboseMode(true);
}

void loop(){

  readSensor();
  //delay(100);
  // Run WiServer
  WiServer.server_task();
  //WiServer.write();
 
  delay(10);
}


// Below there are the utility functions to get data from the preassure sensor.

// read temperature and pressure from sensor
void readSensor() {
  int  ut= readUT();
  long up = readUP();
  long x1, x2, x3, b3, b5, b6, p;
  unsigned long b4, b7;

  //calculate true temperature
  x1 = ((long)ut - ac6) * ac5 >> 15;
  x2 = ((long) mc << 11) / (x1 + md);
  b5 = x1 + x2;
  temperature = (b5 + 8) >> 4;

  //calculate true pressure
  b6 = b5 - 4000;
  x1 = (b2 * (b6 * b6 >> 12)) >> 11; 
  x2 = ac2 * b6 >> 11;
  x3 = x1 + x2;
  b3 = (((int32_t) ac1 * 4 + x3)<<oversampling_setting + 2) >> 2;
  x1 = ac3 * b6 >> 13;
  x2 = (b1 * (b6 * b6 >> 12)) >> 16;
  x3 = ((x1 + x2) + 2) >> 2;
  b4 = (ac4 * (uint32_t) (x3 + 32768)) >> 15;
  b7 = ((uint32_t) up - b3) * (50000 >> oversampling_setting);
  p = b7 < 0x80000000 ? (b7 * 2) / b4 : (b7 / b4) * 2;

  x1 = (p >> 8) * (p >> 8);
  x1 = (x1 * 3038) >> 16;
  x2 = (-7357 * p) >> 16;
  pressure = p + ((x1 + x2 + 3791) >> 4);
}

// read uncompensated temperature value
unsigned int readUT() {
  writeRegister(0xf4,0x2e);
  delay(5); // the datasheet suggests 4.5 ms
  return readIntRegister(0xf6);
}

// read uncompensated pressure value
long readUP() {
  writeRegister(0xf4,0x34+(oversampling_setting<<6));
  delay(pressure_conversiontime[oversampling_setting]);

  unsigned char msb, lsb, xlsb;
  Wire.beginTransmission(I2C_ADDRESS);
  Wire.write(0xf6);  // register to read
  Wire.endTransmission();

  Wire.requestFrom(I2C_ADDRESS, 3); // request three bytes
  while(!Wire.available()); // wait until data available
  msb = Wire.read();
  while(!Wire.available()); // wait until data available
  lsb |= Wire.read();
  while(!Wire.available()); // wait until data available
  xlsb |= Wire.read();
  return (((long)msb<<16) | ((long)lsb<<8) | ((long)xlsb)) >>(8-oversampling_setting);
}

void  getCalibrationData() {
  Serial.println("Reading Calibration Data");
  ac1 = readIntRegister(0xAA);
  Serial.print("AC1: ");
  Serial.println(ac1,DEC);
  ac2 = readIntRegister(0xAC);
  Serial.print("AC2: ");
  Serial.println(ac2,DEC);
  ac3 = readIntRegister(0xAE);
  Serial.print("AC3: ");
  Serial.println(ac3,DEC);
  ac4 = readIntRegister(0xB0);
  Serial.print("AC4: ");
  Serial.println(ac4,DEC);
  ac5 = readIntRegister(0xB2);
  Serial.print("AC5: ");
  Serial.println(ac5,DEC);
  ac6 = readIntRegister(0xB4);
  Serial.print("AC6: ");
  Serial.println(ac6,DEC);
  b1 = readIntRegister(0xB6);
  Serial.print("B1: ");
  Serial.println(b1,DEC);
  b2 = readIntRegister(0xB8);
  Serial.print("B2: ");
  Serial.println(b1,DEC);
  mb = readIntRegister(0xBA);
  Serial.print("MB: ");
  Serial.println(mb,DEC);
  mc = readIntRegister(0xBC);
  Serial.print("MC: ");
  Serial.println(mc,DEC);
  md = readIntRegister(0xBE);
  Serial.print("MD: ");
  Serial.println(md,DEC);
}

void writeRegister(unsigned char r, unsigned char v)
{
  Wire.beginTransmission(I2C_ADDRESS);
  Wire.write(r);
  Wire.write(v);
  Wire.endTransmission();
}

// read a 16 bit register
int readIntRegister(unsigned char r)
{
  unsigned char msb, lsb;
  Wire.beginTransmission(I2C_ADDRESS);
  Wire.write(r);  // register to read
  Wire.endTransmission();

  Wire.requestFrom(I2C_ADDRESS, 2); // request two bytes
  while(!Wire.available()); // wait until data available
  msb = Wire.read();
  while(!Wire.available()); // wait until data available
  lsb = Wire.read();
  return (((int)msb<<8) | ((int)lsb));
}

// read an 8 bit register
/*
unsigned char readRegister(unsigned char r)
{
  unsigned char v;
  Wire.beginTransmission(I2C_ADDRESS);
  Wire.send(r);  // register to read
  Wire.endTransmission();

  Wire.requestFrom(I2C_ADDRESS, 1); // request a byte
  while(!Wire.available()); // wait until data available
  v = Wire.receive();
  return v;
}
*/

