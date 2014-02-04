//
// Sharp BoosterPackLCD SPI
// Example for library for Sharp BoosterPack LCD with hardware SPI
//
//
// author:  Stefan Schauer
// date:    Jan 29, 2014
// version:	1.00
//
// file:    LCD_SharpBoosterPack_SPI_main.cpp
//
// see	ReadMe.txt for references
//

#include <msp430.h>
#include "LCD_SharpBoosterPack_SPI.h"
#include "spi.h"

uint8_t _pinReset;
uint8_t _pinSerialData;
uint8_t _pinDISP;
uint8_t _pinVCC;
uint8_t _pinChipSelect;
uint8_t _pinSerialClock;

// Booster Pack Pins
    //  7 - P2.2 for SPI_CLK mode
    // 15 - P1.6 for SPI_SIMO mode
    //  6 - P2.4 output pin for SPI_CS
    //  2 - P4.2 as output to supply the LCD
    //  5 - P4.3 as output for DISP
    // Set display's VCC and DISP pins to high


static const uint8_t P_CS   = 6;
static const uint8_t P_VCC  = 2;
static const uint8_t P_DISP = 5;

#define LCD_VERTICAL_MAX    96
#define LCD_HORIZONTAL_MAX  96

#define SHARP_SEND_TOGGLE_VCOM_COMMAND      0x01
#define SHARP_SKIP_TOGGLE_VCOM_COMMAND      0x00

#define SHARP_LCD_TRAILER_BYTE              0x00

#define SHARP_VCOM_TOGGLE_BIT               0x40

#define SHARP_LCD_CMD_CHANGE_VCOM           0x00
#define SHARP_LCD_CMD_CLEAR_SCREEN          0x20
#define SHARP_LCD_CMD_WRITE_LINE            0x80

unsigned char DisplayBuffer[LCD_VERTICAL_MAX][LCD_HORIZONTAL_MAX/8];
unsigned char VCOMbit= 0x40;
unsigned char flagSendToggleVCOMCommand = 0;

static void SendToggleVCOMCommand(void);
	
LCD_SharpBoosterPack_SPI::LCD_SharpBoosterPack_SPI() {
    LCD_SharpBoosterPack_SPI(P_CS,    // Chip Select
                 P_VCC,   // Vcc display
                 P_DISP   // DISP
	);
}


LCD_SharpBoosterPack_SPI::LCD_SharpBoosterPack_SPI(uint8_t pinChipSelect, uint8_t pinDISP, uint8_t pinVCC) {
    _pinChipSelect  = pinChipSelect;
    _pinDISP = pinDISP;
    _pinVCC  = pinVCC;
}

void LCD_SharpBoosterPack_SPI::setXY(uint8_t x, uint8_t y, uint8_t  ulValue) {
    if( ulValue != 0)
        DisplayBuffer[y][x>>3] &= ~(0x80 >> (x & 0x7));
    else
        DisplayBuffer[y][x>>3] |= (0x80 >> (x & 0x7));
}

void LCD_SharpBoosterPack_SPI::begin() {
    pinMode(_pinChipSelect, OUTPUT);
    pinMode(_pinDISP, OUTPUT);
    pinMode(_pinVCC, OUTPUT);

    digitalWrite(_pinChipSelect, LOW);
    digitalWrite(_pinVCC, HIGH);
    digitalWrite(_pinDISP, HIGH);
    
	TA0_enableVCOMToggle();
	
    clear();
    _font = 0;
}

String LCD_SharpBoosterPack_SPI::WhoAmI() {
    return "Sharp LCD BoosterPack";
}

void LCD_SharpBoosterPack_SPI::clear() {
	
  unsigned char command = SHARP_LCD_CMD_CLEAR_SCREEN;                            //clear screen mode(0X100000b)
  command = command^VCOMbit;                    //COM inversion bit

  // Set P2.4 High for CS
  digitalWrite(_pinChipSelect, HIGH);

  SPI.transfer(command);
  flagSendToggleVCOMCommand = SHARP_SKIP_TOGGLE_VCOM_COMMAND;
  SPI.transfer(SHARP_LCD_TRAILER_BYTE);

  // Wait for last byte to be sent, then drop SCS
  __delay_cycles(100);

  // Set P2.4 High for CS
  digitalWrite(_pinChipSelect, LOW);

  clearBuffer();
  
}

void LCD_SharpBoosterPack_SPI::clearBuffer() {
    unsigned int i=0,j=0;
    for(i =0; i< LCD_VERTICAL_MAX; i++)
    for(j =0; j< (LCD_HORIZONTAL_MAX>>3); j++)
       DisplayBuffer[i][j] = 0xff;
}

void LCD_SharpBoosterPack_SPI::setFont(uint8_t font) {
    _font = font;
}

void LCD_SharpBoosterPack_SPI::text(uint8_t x, uint8_t y, String s) {
    uint8_t i;
    uint8_t j;
    int8_t k;
    uint8_t offset = 0;;
    
    if (_font==0) {
        for (j=0; j<s.length(); j++) {
            for (i=0; i<5; i++){ 
				for (k=7; k>=0; k--) setXY(x+offset, y+k, Terminal6x8[s.charAt(j)-' '][i]&(1<<k));
				offset += 1;
			}
			offset += 1;  // spacing
        }
    }
    else if (_font==1) {
        for (j=0; j<s.length(); j++) {
            for (i=0; i<11; i++){
				for (k=7; k>=0; k--){ 
					setXY(x+offset, y+k,   Terminal11x16[s.charAt(j)-' '][2*i]&(1<<k));
					setXY(x+offset, y+k+8, Terminal11x16[s.charAt(j)-' '][2*i+1]&(1<<k));
				}
				offset += 1;
			}
			offset += 1;  // spacing
        }
    }
}

uint8_t reverse(uint8_t x)
{
  const uint8_t a[] = {0x0, 0x8, 0x4, 0xC, 0x2, 0xA, 0x6, 0xE, 0x1, 0x9, 0x5, 0xD, 0x3, 0xB, 0x7, 0xF};
  uint8_t b = 0;
  
  b  = a[x & 0xF]<<4;
  b |= a[(x & 0xF0)>>4];
  return b;
}


void LCD_SharpBoosterPack_SPI::flush (void)
{
	unsigned char *pucData = &DisplayBuffer[0][0];
    long xi =0;
    long xj = 0;
    //image update mode(1X000000b)
    unsigned char command = SHARP_LCD_CMD_WRITE_LINE;

    command |= BIT7;
    //COM inversion bit
    command = command^VCOMbit;
    // Set P2.4 High for CS
    digitalWrite(_pinChipSelect, HIGH);

    SPI.transfer((char)command);
    flagSendToggleVCOMCommand = SHARP_SKIP_TOGGLE_VCOM_COMMAND;
    for(xj=0; xj<LCD_VERTICAL_MAX; xj++)
    {
		SPI.transfer((char)reverse(xj + 1));

        for(xi=0; xi<(LCD_HORIZONTAL_MAX>>3); xi++)
        {
			SPI.transfer((char)*(pucData++));
        }
        SPI.transfer(SHARP_LCD_TRAILER_BYTE);
    }

    SPI.transfer((char)SHARP_LCD_TRAILER_BYTE);

    __delay_cycles(100);

    // Set P2.4 Low for CS
    digitalWrite(_pinChipSelect, LOW);
}

static void SendToggleVCOMCommand(void)
{
    VCOMbit ^= SHARP_VCOM_TOGGLE_BIT;

    if(SHARP_SEND_TOGGLE_VCOM_COMMAND == flagSendToggleVCOMCommand)
    {
        unsigned char command = SHARP_LCD_CMD_CHANGE_VCOM;                            //clear screen mode(0X100000b)
        command = command^VCOMbit;                    //COM inversion bit

		// Set P2.4 High for CS
		digitalWrite(_pinChipSelect, HIGH);

        SPI.transfer((char)command);
        SPI.transfer((char)SHARP_LCD_TRAILER_BYTE);

        // Wait for last byte to be sent, then drop SCS
        __delay_cycles(100);
		// Set P2.4 High for CS
		digitalWrite(_pinChipSelect, LOW);
    }
	
    flagSendToggleVCOMCommand = SHARP_SEND_TOGGLE_VCOM_COMMAND;
}

void LCD_SharpBoosterPack_SPI::TA0_enableVCOMToggle()
{
	TA0CTL = TASSEL__ACLK | ID__8 | TACLR | MC__UP;
    TA0CCTL0 = CCIE;
	TA0CCR0 = 4096;
}


void LCD_SharpBoosterPack_SPI::TA0_turnOff()
{
	TA0CTL = 0;
    TA0CCTL0 = 0;
}

#pragma vector=TIMER0_A0_VECTOR
__interrupt void TA0_ISR(void)
{
    SendToggleVCOMCommand();
}