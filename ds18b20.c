#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#define CONV_DLY_MS_DS1820 210 // min 200mS
#define OWPIN 5
#define FALSE 0
#define TRUE 1
////////////////////////////////////////////////////////////////////////////
// GLOBAL VARIABLES
//
unsigned char ROM[8]; // ROM Bit
unsigned char lastDiscrep = 0; // last discrepancy
unsigned char doneFlag = 0; // Done flag
unsigned char FoundROM[5][8]; // table of found ROM codes
unsigned char numROMs;
unsigned char dowcrc;
unsigned char dscrc_table[] = {
0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
157,195, 33,127,252,162, 64, 30, 95, 1,227,189, 62, 96,130,220,
35,125,159,193, 66, 28,254,160,225,191, 93, 3,128,222, 60, 98,
190,224, 2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89, 7,
219,133,103, 57,186,228, 6, 88, 25, 71,165,251,120, 38,196,154,
101, 59,217,135, 4, 90,184,230,167,249, 27, 69,198,152,122, 36,
248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91, 5,231,185,
140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
202,148,118, 40,171,245, 23, 73, 8, 86,180,234,105, 55,213,139,
87, 9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53};


unsigned char First(void);
unsigned char Next(void);
unsigned char ow_crc(unsigned char x);


//////////////////////////////////////////////////////////////////////////////
// OW_RESET - performs a reset on the one-wire bus and
// returns the presence detect. Reset is 480us, so delay
// value is (480-24)/16 = 28.5 - we use 29. Presence checked
// another 70us later, so delay is (70-24)/16 = 2.875 - we use 3.
//
unsigned char ow_reset(void)
{
unsigned char presence;
//DQ = 0; 				  //pull DQ line low
pinMode(OWPIN, OUTPUT);
digitalWrite(OWPIN, LOW);
delayMicroseconds(480);   // leave it low for 480us
//DQ = 1; 				  // allow line to return high
digitalWrite(OWPIN, HIGH);
//delay(3); // wait for presence
delayMicroseconds(70);
//presence = DQ; // get presence signal
pinMode(OWPIN, INPUT);
presence = digitalRead(OWPIN);
//delay(25); // wait for end of timeslot
delayMicroseconds(424); // 24µS + 25 * 16µS
return(presence); // presence signal returned
} // 0=presence, 1 = no part



//////////////////////////////////////////////////////////////////////////////
// READ_BIT - reads a bit from the one-wire bus. The delay
// required for a read is 15us, so the DELAY routine won't work.
// We put our own delay function in this routine in the form of a
// for() loop.
//
unsigned char read_bit(void)
{
unsigned char i;
//DQ = 0; // pull DQ low to start timeslot
pinMode(OWPIN, OUTPUT);
digitalWrite(OWPIN, LOW);
//DQ = 1; // then return high
digitalWrite(OWPIN, HIGH);
//for (i=0; i<3; i++); // delay 15us from start of timeslot
delayMicroseconds(15);
//return(DQ); // return value of DQ line
pinMode(OWPIN, INPUT);
return(digitalRead(OWPIN));
}


//////////////////////////////////////////////////////////////////////////////
// WRITE_BIT - writes a bit to the one-wire bus, passed in bitval.
//
void write_bit(char bitval)
{
//DQ = 0; // pull DQ low to start timeslot
pinMode(OWPIN, OUTPUT);
digitalWrite(OWPIN, LOW);
//if(bitval==1) DQ =1; // return DQ high if write 1
if(bitval==1) digitalWrite(OWPIN, HIGH);
//delay(5); // hold value for remainder of timeslot
delayMicroseconds(104);
//DQ = 1;
digitalWrite(OWPIN, HIGH);
}// Delay provides 16us per loop, plus 24us. Therefore delay(5) = 104us


//////////////////////////////////////////////////////////////////////////////
// READ_BYTE - reads a byte from the one-wire bus.
//
unsigned char read_byte(void)
{
unsigned char i;
unsigned char value = 0;
for (i=0;i<8;i++)
{
if(read_bit()) value|=0x01<<i; // reads byte in, one byte at a time and then
// shifts it left
//delay(6); // wait for rest of timeslot
delayMicroseconds(120);  // 24µS + 6 * 16µS
}
return(value);
}

//////////////////////////////////////////////////////////////////////////////
// WRITE_BYTE - writes a byte to the one-wire bus.
//
void write_byte(char val)
{
unsigned char i;
unsigned char temp;
for (i=0; i<8; i++) // writes byte, one bit at a time
{
temp = val>>i; // shifts val right 'i' spaces
temp &= 0x01; // copy that bit to temp
write_bit(temp); // write bit in temp into
}
//delay(5);
delayMicroseconds(104); 
}

//////////////////////////////////////////////////////////////////////////////
// ONE WIRE CRC
//
unsigned char ow_crc( unsigned char x)
{
dowcrc = dscrc_table[dowcrc^x];
return dowcrc;
}


// FIRST
// The First function resets the current state of a ROM search and calls
// Next to find the first device on the 1-Wire bus.
//
unsigned char First(void)
{
lastDiscrep = 0; // reset the rom search last discrepancy global
doneFlag = FALSE;
return Next(); // call Next and return its return value
}

// NEXT
// The Next function searches for the next device on the 1-Wire bus. If
// there are no more devices on the 1-Wire then false is returned.
//
unsigned char Next(void)
{
unsigned char m = 1; // ROM Bit index
unsigned char n = 0; // ROM Byte index
unsigned char k = 1; // bit mask
unsigned char x = 0;
unsigned char discrepMarker = 0; // discrepancy marker
unsigned char g; // Output bit
unsigned char nxt; // return value
int flag;
nxt = FALSE; // set the next flag to false
dowcrc = 0; // reset the dowcrc
flag = ow_reset(); // reset the 1-Wire
if(flag||doneFlag) // no parts -> return false
{
lastDiscrep = 0; // reset the search
return FALSE;
}
write_byte(0xF0); // send SearchROM command
do
// for all eight bytes
{
x = 0;
if(read_bit()==1) x = 2;
//delay(6);
delayMicroseconds(120);
if(read_bit()==1) x |= 1; // and its complement
if(x ==3) // there are no devices on the 1-Wire
break;

else
{
if(x>0) // all devices coupled have 0 or 1
g = x>>1; // bit write value for search
else
{
// if this discrepancy is before the last
// discrepancy on a previous Next then pick
// the same as last time
if(m<lastDiscrep)
g = ((ROM[n]&k)>0);
else // if equal to last pick 1
g = (m==lastDiscrep); // if not then pick 0
// if 0 was picked then record
// position with mask k
if (g==0) discrepMarker = m;
}
if(g==1) // isolate bit in ROM[n] with mask k
ROM[n] |= k;
else
ROM[n] &= ~k;
write_bit(g); // ROM search write
m++; // increment bit counter m
k = k<<1; // and shift the bit mask k
if(k==0) // if the mask is 0 then go to new ROM
{ // byte n and reset mask
ow_crc(ROM[n]); // accumulate the CRC
n++; k++;
}
}
}while(n<8); //loop until through all ROM bytes 0-7
if(m<65||dowcrc) // if search was unsuccessful then
lastDiscrep=0; // reset the last discrepancy to 0
else
{
// search was successful, so set lastDiscrep,
// lastOne, nxt
lastDiscrep = discrepMarker;
doneFlag = (lastDiscrep==0);
nxt = TRUE; // indicates search is not complete yet, more
// parts remain
}
return nxt;
}

// FIND DEVICES
void FindDevices(void)
{
unsigned char m;
if(!ow_reset()) //Begins when a presence is detected
{
if(First()) //Begins when at least one part is found
{
numROMs=0;
do
{
numROMs++;
for(m=0;m<8;m++)
{
FoundROM[numROMs][m]=ROM[m]; //Identifies ROM number on found device
} printf("\nROM CODE =%02X%02X%02X%02X\n",
FoundROM[5][7],FoundROM[5][6],FoundROM[5][5],FoundROM[5][4],
FoundROM[5][3],FoundROM[5][2],FoundROM[5][1],FoundROM[5][0]);
}while (Next()&&(numROMs<10)); //Continues until no additional devices are found
}
}
}

void Read_ScratchPad(void)
{
int j;
char pad[10];
printf("\nReading ScratchPad Data\n");
write_byte(0xBE);
for (j=0;j<9;j++){pad[j]=read_byte();}
printf("\n ScratchPAD DATA = %X%X%X%X%X%X\n",pad[8],pad[7],pad[6],pad[5],pad[4],pad[3],pad[2],pad[1],pad[0]);
}

void Read_ROMCode(void)
{
int n;
char dat[9];
printf("\nReading ROM Code\n");
ow_reset();
write_byte(0x33);
for (n=0;n<8;n++){dat[n]=read_byte();}
printf("\n ROM Code = %X%X%X%X\n",dat[7],dat[6],dat[5],dat[4],dat[3],dat[2],dat[1],dat[0]);
}

// Perform Match ROM
//
unsigned char Send_MatchRom(void)
{
unsigned char i;
if(ow_reset()) return FALSE;
write_byte(0x55); // match ROM
for(i=0;i<8;i++)
{
write_byte(FoundROM[numROMs][i]); //send ROM code
}
return TRUE;
}


float Read_Temperature(void)
{
char get[10];
char temp_lsb,temp_msb;
int k;
char temp_f,temp_c;
float temp_read, temp_high_res;

ow_reset();
write_byte(0xCC); //Skip ROM
write_byte(0x44); // Start Conversion
//delay(5);
delayMicroseconds(104);
ow_reset();
write_byte(0xCC); // Skip ROM
write_byte(0xBE); // Read Scratch Pad
for (k=0;k<9;k++){get[k]=read_byte();}
//printf("\n ScratchPAD DATA = %X%X%X%X%X\n",get[8],get[7],get[6],get[5],get[4],get[3],get[2],get[1],get[0]);
temp_msb = get[1]; // Sign byte + lsbit
temp_lsb = get[0]; // Temp data plus lsb
if (temp_msb <= 0x80){temp_lsb = (temp_lsb/2);} // shift to get whole degree
temp_msb = temp_msb & 0x80; // mask all but the sign bit
if (temp_msb >= 0x80) {temp_lsb = (~temp_lsb)+1;} // twos complement
if (temp_msb >= 0x80) {temp_lsb = (temp_lsb/2);}// shift to get whole degree
if (temp_msb >= 0x80) {temp_lsb = ((-1)*temp_lsb);} // add sign bit
if (temp_lsb & 0x01) {  // if last bit is set, decimal is 0.5°C 
	return((float)temp_lsb + 0.5); // print temp. C
}
else 
	return((float)temp_lsb); // print temp. C
}

void Print_Temperature(void)
{
char get[10];
char temp_lsb,temp_msb;
int k;
char temp_f,temp_c;
float temp_read, temp_high_res;

ow_reset();
write_byte(0xCC); //Skip ROM
write_byte(0x44); // Start Conversion
//delay(5);
delayMicroseconds(104);
ow_reset();
write_byte(0xCC); // Skip ROM
write_byte(0xBE); // Read Scratch Pad
for (k=0;k<9;k++){get[k]=read_byte();}
printf("\n ScratchPAD DATA = %X%X%X%X%X\n",get[8],get[7],get[6],get[5],get[4],get[3],get[2],get[1],get[0]);
temp_msb = get[1]; // Sign byte + lsbit
temp_lsb = get[0]; // Temp data plus lsb
if (temp_msb <= 0x80){temp_lsb = (temp_lsb/2);} // shift to get whole degree
temp_msb = temp_msb & 0x80; // mask all but the sign bit
if (temp_msb >= 0x80) {temp_lsb = (~temp_lsb)+1;} // twos complement
if (temp_msb >= 0x80) {temp_lsb = (temp_lsb/2);}// shift to get whole degree
if (temp_msb >= 0x80) {temp_lsb = ((-1)*temp_lsb);} // add sign bit
if (temp_lsb & 0x01) {  // include 1/2°C LSB bit 
	printf( "\nDS1820 = %d.5 °C\n", (int)temp_lsb ); // print temp. C
}
else 
	printf( "\nDS1820 = %d °C\n", (int)temp_lsb ); // print temp. C
temp_c = temp_lsb; // ready for conversion to Fahrenheit or high resolution

//temp_f = (((int)temp_c)* 9)/5 + 32;
//printf( "\nTempF= %d degrees F\n", (int)temp_f ); // print temp. F
}


int main(void)
{
  int i = 0;   
  //printf("Driver for MAXIM DS18B20\n");
  if(wiringPiSetup()==-1)
    exit(1);  // wiringPi library error
  if(ow_reset() == 0) {  // part is detected on the bus
	//printf("Part detected on OW Bus\n");
	printf("%.2f °C\n", Read_Temperature());
  }
  else
	printf("No parts detected on OW Bus\n");
  
  return (0);
}
