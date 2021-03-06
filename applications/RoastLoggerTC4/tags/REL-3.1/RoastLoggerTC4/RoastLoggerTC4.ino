
/********************************************************************************************
 *  RoastLoggerTC4.ino  by GreenBean and Jim Gallt (see revision history below)
 *  
 *  TC4 Arduino sketch for use with Roast Logger by GreenBean http://www.toomuchcoffee.com/
 *  For information on the RoastLogger see:
 * 
 *        http://homepage.ntlworld.com/green_bean/coffee/roastlogger/roastlogger.htm
 *
 *  This version modified to compile/verify on Arduino IDE version 1.0.
 *  Compiles on Arduino Duemilanove 328 and Uno boards.
 *
 *  Based on aBourbon.pde:
 *  Bourbon-REL-300  downloaded from http://code.google.com/p/tc4-shield/downloads/list
 *  Quote
 *  aBourbon, as a standalone application, displays elapsed roast time, ET, BT, and BT-RoR on 
 *  LCD.  When connected to LCDapter board, buttons provide ability to identify 4 roast 
 *  markers (start, first crack, second crack, eject).
 *  Unquote
 *
 *  Modified to be compatible with the Roast Logger and renamed RoastLoggerTC4.pde
 *  Changed baud rate in user.h to 115200
 *  Default output is Celsius - add jumper on ANLG2 to output in Fahrenheit.
 *  Added communication for heater power control for OT1 
 *  Changed serial output to report T1, T2, rorT1, rorT2 and power level.
 *  
 *
 ********************************************************************************************/
/********************************************************************************************

 Please note that you must install the included libraries (in the libraries folder) before 
  the sketch will verify/compile.

  See the "Contributed Libraries" section of http://www.arduino.cc/en/Reference/Libraries
  for details of how to install them.

 ********************************************************************************************/

// *** BSD License ***
// ------------------------------------------------------------------------------------------
// Copyright (c) 2011, 2012, 2014 MLG Properties, LLC
// All rights reserved.
//
// Contributor:  Jim Gallt
//
// Redistribution and use in source and binary forms, with or without modification, are 
// permitted provided that the following conditions are met:
//
//   Redistributions of source code must retain the above copyright notice, this list of 
//   conditions and the following disclaimer.
//
//   Redistributions in binary form must reproduce the above copyright notice, this list 
//   of conditions and the following disclaimer in the documentation and/or other materials 
//   provided with the distribution.
//
//   Neither the name of the MLG Properties, LLC nor the names of its contributors may be 
//   used to endorse or promote products derived from this software without specific prior 
//   written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS 
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL 
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// ------------------------------------------------------------------------------------------

//#define LOGIC_ANALYZER 

#define BANNER_RL1 "RoastLoggerTC4"
#define BANNER_RL2 "version 3.1"

// Revision history: - of RoastLoggerTC4
//  20120112:  Version 0.3 - Released for testing
//  20120127:  Version 0.4 - Modified to compile under Arduino IDE 1.0
//  20120312:  Version 0.5 - Modified by Jim Gallt to use standard PWM outputs on OT1 and IO3
//                           Added fan output command capability
//  20120403:  Version 0.6 - Minor modification to logger method to change order of output and clean up RoR output
//  20120425:  Version 0.7 - Select F units using jumper on ANLG2 port (IN -- GND)
//  20120511:  Version 0.8 - Turn fan, heater off by default in setup; 
//                           use 4 sec timebase for heater PWM;
//                           allow mapping of physical input channels;
//                           round heater, fan output levels to nearest 5%;
//                           enable use of LCDapter, if present;
//                           set default filter level to 90% for ambient sensor;
//                           enabled button functions (in standalone mode)
//  20120513   Version 0.9x  Standalone user interface
//  20120516   Version 0.9y  Respond to LOAD command from host (resets timer)
//                           Split LCD display refreshes into two cycles
//  20120517                 Eliminated the banner display delay
//                           Moved hid.refresh() out of logger() and into loop()
//  20120518                 Optimizations for display
//  20120518   Version 1.0b  beta release for firmware with standalone option
//  20120607                 Changed PWM frequency for fan to 7808Hz
//  20120710   Version 2.0   Release supports standalone operation using LCDapter buttons
//                           Set default fan PWM frequency to 61Hz (7808 is too fast for HTC controller)
//  20130117   Version 2.1   Made the choice of temperature scale selectable in user.h
//  20140408   Version 3RC   Added slew rate limitations on IO3 (fan)
//                           Added support for runtime selectable temperature scale
//  20140409   Version 3RC2  ANLG2 port (temp scale select) checked only at start
//  20140410   Version 3.0   Added fan variable initializations
//  20141122   Version 3.1RC Do all computations in F, convert units only to display
//  20141124   Version 3.1   Released

// This code was adapted from the a_logger.pde file provided
// by Bill Welch.

// The user.h file contains user-definable compiler options
#include "user.h"
#include "basicHID.h" // standard interface

// this library included with the arduino distribution
#include <Wire.h>

// these "contributed" libraries must be installed in your sketchbook's arduino/libraries folder
#include <thermocouple.h>
#include <cADC.h>
#include <PWM16.h>
#include <mcEEPROM.h>
#include <cLCD.h>
#include <cButton.h>

// ------------------------ other compile directives
#define MIN_DELAY 300   // ms between ADC samples (tested OK at 270)
#define NCHAN 2   // number of TC input channels
#define DP 1  // decimal places for output on serial port
#define D_MULT 0.001 // multiplier to convert temperatures from int to float
#define MAX_COMMAND 80 // max length of a command string
#define LOOPTIME 1000 // cycle time, in ms

// hardware override of default temperature scale
#define ANLG2 A1 // arduino pin A1
#define UNIT_SEL ANLG2 // select temperature scale

#define CMD_POWER "POWER"
#define CMD_FAN "FAN"
#define CMD_PCCONTROL "PCCONTROL"
#define CMD_ARDUINOCONTROL "ARDUINOCONTROL"
#define CMD_LOAD "LOAD"
#define CMD_TEMPF "TEMPF"
#define CMD_TEMPC "TEMPC"

// fan slew rate limitations (on IO3)
#define MAX_SLEW 25 // percent per second
#define SLEW_STEP 5 // increase in steps of 5% for smooth transition
#define SLEW_STEP_TIME (uint32_t)(SLEW_STEP * 1000 / MAX_SLEW) // min ms delay between steps

#ifdef LOGIC_ANALYZER
#define BLIP_PIN 2  
#endif

// --------------------------------------------------------------
// global variables

mcEEPROM eeprom;
calBlock caldata;

// class objects
cADC adc( A_ADC ); // MCP3424
ambSensor amb( A_AMB ); // MCP9800
filterRC fT[NCHAN]; // filter for displayed/logged ET, BT
filterRC fRise[NCHAN]; // heavily filtered for calculating RoR
filterRC fRoR[NCHAN]; // post-filtering on RoR values
HIDbase hid; // interface

tcBase* tc[NCHAN]; // array of pointers to thermocouples

TC_TYPE1 tc1; // input 1
TC_TYPE2 tc2; // input 2

// arrays to store temperatures, times for each channel
int32_t temps[NCHAN]; //  stored temperatures are divided by D_MULT
int32_t ftemps[NCHAN]; // heavily filtered temps
int32_t ftimes[NCHAN]; // filtered sample timestamps
int32_t flast[NCHAN]; // for calculating derivative
int32_t lasttimes[NCHAN]; // for calculating derivative

// allows choice of TC4 input channels (see user.h for mapping)
uint8_t chan_map[NCHAN] = { LOGCHAN1, LOGCHAN2 };

//RoastLogger global variables for heater, fan power %
int8_t heater; // power for 1st output (heater)
int8_t current_fan; // power for 2nd output (fan) - instantaneous value
int8_t target_fan; // user-selected fan output
uint32_t last_fan_change; // ms time value when fan output was last increased
PWM16 output1; // 16-bit timer for SSR output on Ot1 and Ot2
PWM_IO3 io3; // 8-bit timer for fan control on IO3


// used in main loop
float timestamp;
boolean first;
uint32_t lastLoop;
uint32_t thisLoop;
float reftime; // reference for measuring elapsed time

// current values are needed globally
float RoR_cur,t1_cur,t2_cur;

// temperature units selection
boolean celsius = CELSIUS;  // select the default units in user.h

char command[MAX_COMMAND+1]; // input buffer for commands from the serial port

#ifdef LOGIC_ANALYZER
uint8_t blip_state;
void blip() {
  digitalWrite( BLIP_PIN, blip_state );
  blip_state = !blip_state;
}
#endif

// T1, T2 = temperatures x 1000
// t1, t2 = time marks, milliseconds
// ---------------------------------------------------
float calcRise( int32_t T1, int32_t T2, int32_t t1, int32_t t2 ) {
  int32_t dt = t2 - t1;
  if( dt == 0 ) return 0.0;  // fixme -- throw an exception here?
  float dT = (T2 - T1) * D_MULT;
  float dS = dt * 0.001; // convert from milli-seconds to seconds
  return ( dT / dS ) * 60.0; // rise per minute
}

// ------------------------------------------------------------------
void logger()
{
  uint8_t i;
  float rx;
  float cf,co;  // temp scale units conversion factor and offset

  String rorT1,rorT2;

  // temperature scale conversion
  if( celsius ) {
    cf = 1.8;
    co = 32.0;
  }
  else {
    cf = 1.0;
    co = 0.0;
  }
  
  t1_cur = D_MULT * temps[0];
  t2_cur = D_MULT * temps[1];

  // print temperature, rate for each channel
  i = 0;
  if( NCHAN >= 1 ) {
    Serial.print("rorT1=");
    RoR_cur = calcRise( flast[i], ftemps[i], lasttimes[i], ftimes[i] );
    RoR_cur = fRoR[i].doFilter( RoR_cur /  D_MULT ) * D_MULT; // perform post-filtering on RoR values
    Serial.println( RoR_cur / cf , DP );
    Serial.print("T1=");
    Serial.println( ( t1_cur - co ) / cf, DP );
    i++;
  };

  if( NCHAN >= 2 ) {
    Serial.print("rorT2=");
    rx = calcRise( flast[i], ftemps[i], lasttimes[i], ftimes[i] );
    rx = fRoR[i].doFilter( rx / D_MULT ) * D_MULT; // perform post-filtering on RoR values
    Serial.println( rx / cf, DP );
    Serial.print("T2=");
    Serial.println( ( t2_cur - co ) / cf, DP );
    i++;
  };

  //print heater power and fan output level 
  Serial.print("Power%=");
  Serial.println(heater);
  Serial.print("Fan=");
  Serial.println(current_fan);
}

// -------------------------------------
void append( char* str, char c ) { // reinventing the wheel
  int len = strlen( str );
  str[len] = toupper(c);
  str[len+1] = '\0';
}

// -------------------------------------
void set_fan_level( uint8_t new_fan ) {
  if( new_fan >= 0 && new_fan < 101 ) {  // screen out bogus values
    float pow = 2.55 * new_fan;  // output values are 0 to 255
    io3.Out( round( pow ) ); // set the output level
    current_fan = new_fan; // record the present output level
    last_fan_change = millis(); // record the time of the last level change
  }
}

// -------------------------------------
// limits the rate of increase of fan output
void slew_fan() {
  if( target_fan < current_fan ) {
    set_fan_level( target_fan ); // no limitation if decreasing
  }
  else if( target_fan > current_fan ) { // limit the skew rate if increasing level
    uint8_t delta_fan = target_fan - current_fan; // remaining requested increase in fan level
    if( delta_fan > SLEW_STEP )  // place a limitation on the step size
      delta_fan = SLEW_STEP;
    uint32_t delta_fan_ms = millis() - last_fan_change; // ms since last step increase
    if( delta_fan_ms >= SLEW_STEP_TIME ) { // only if enough time has gone by
      set_fan_level( current_fan + delta_fan ); // increase the fan level
    }
  }
}

// -------------------------------------
void processCommand() {  // a newline character has been received, so process the command
  char *val, *c;
  String key = strtok_r(command, "=", &c);
  hid.setSlaveMode(); // disable local user input after first newline character received from serial
  if (key != NULL && key.equals(CMD_POWER)) {
    val = strtok_r(NULL, "=", &c);
    if (val != NULL) {
      heater = atoi(val);   
      // heater = roundOutput( heater );   
      if (heater >= 0 && heater <101) {  
        output1.Out( heater, 0 ); // update the power output on the SSR drive Ot1
      }
    }
  }
  else if (key != NULL && key.equals(CMD_FAN)) {
    val = strtok_r(NULL, "=", &c);
    if (val != NULL) {
      target_fan = atoi(val);  // set new target fan output level
    }
  }
  else if (key != NULL && key.equals(CMD_PCCONTROL)) { // placeholder
  }
  else if (key != NULL && key.equals(CMD_ARDUINOCONTROL)) { // placeholder
  }
  else if (key != NULL && key.equals(CMD_LOAD)) {
    resetTimer();  // reset the timer in response to message from host
  }
  else if (key != NULL && key.equals(CMD_TEMPF)) {
    //select Fahrenheit
    celsius = false;
  }
  else if (key != NULL && key.equals(CMD_TEMPC)) {
    // select Celsius
    celsius = true;
  }
  
}

// -------------------------------------
void checkSerial() {  // buffer the input from the serial port
  char c;

#ifdef LOGIC_ANALYZER
  blip();
#endif
  
  while( Serial.available() > 0 ) {
    c = Serial.read();
    if( ( c == '\n' ) || ( strlen( command ) == MAX_COMMAND ) ) { // check for newline, or buffer overflow
      processCommand();
      strcpy( command, "" ); // empty the buffer
    } // end if
    else {
      append( command, c );
    } // end else
  } // end while
}

// --------------------------------------------------------------------------
void get_samples() // this function talks to the amb sensor and ADC via I2C
{
  int32_t v;
  float tempF;
  uint32_t tod;

  for( int j = 0; j < NCHAN; j++ ) { // one-shot conversions on both chips
    adc.nextConversion( chan_map[j] ); // start ADC conversion on channel j
    amb.nextConversion(); // start ambient sensor conversion

    // wait for conversions to take place
    tod = millis();
    while( millis() - tod < MIN_DELAY ) {
      checkSerial(); // check for serial input
      slew_fan(); // keep the fan smoothly increasing in speed
      HIDevents(); // check for interface events
    }

    ftimes[j] = millis(); // record timestamp for RoR calculations
    amb.readSensor(); // retrieve value from ambient temp register
    v = adc.readuV(); // retrieve microvolt sample from MCP3424
    tempF = tc[j]->Temp_F( 0.001 * v, amb.getAmbF() ); // basic unit is F
    v = round( tempF / D_MULT ); // store results as integers
    temps[j] = fT[j].doFilter( v ); // apply digital filtering for display/logging
    ftemps[j] =fRise[j].doFilter( v ); // heavier filtering for RoR
  }
};

// ----------------------------
void resetTimer() {
  reftime = 0.001 * thisLoop; // reset the reference point for timestamp
  return;
}

// ------------------- process interface events (not active when connected to PC)
void HIDevents() {
  if( hid.processEvents() ) { // something changed
    if( hid.resetTimer() )
      resetTimer();
    if( hid.chgLevel_1() ) {
      heater = hid.getLevel_1();
      output1.Out( heater, 0 );
    }
    if( hid.chgLevel_2() ) {
      target_fan = hid.getLevel_2();
    }
  }
}

// ------------------------------------------------------------------------
// MAIN
//
void setup()
{
  delay(10); // short delay for startup
  
  // use ANLG2 input pin for temperature units selection
  pinMode( UNIT_SEL, INPUT );
  digitalWrite( UNIT_SEL, HIGH ); // enable pullup
  boolean override = digitalRead( UNIT_SEL ) == LOW;  // use jumper to drive low and select non-default
  if( override ) celsius = !CELSIUS;
  else celsius = CELSIUS;

  Serial.begin(BAUD); // enable serial right away to avoid buffer overflows

#ifdef LOGIC_ANALYZER
  pinMode( BLIP_PIN, OUTPUT );
#endif

  lastLoop = millis();
  Wire.begin(); 

  // initialize values
  heater = 0;  // heater is off by default
  target_fan = 0; // fan is off by default
  current_fan = 0;
  last_fan_change = millis();
  t1_cur = 0.0;
  t2_cur = 0.0;
  RoR_cur = 0.0;
  
// initialize the display/input device
  hid.begin( 16, 2, 4 ); // default is 16 x 2 LCD with 4 buttons
  hid.backlight();
  hid.setCursor( 0, 0 );
  hid.print( BANNER_RL1 ); // program name
  hid.setCursor( 0, 1 );
  hid.print( BANNER_RL2 ); // version
  hid.ledAllOff();

  amb.init( AMB_FILTER );  // initialize ambient temp filtering

  // read calibration and identification data from eeprom
  if( readCalBlock( eeprom, caldata ) ) {
    Serial.println("# EEPROM data read: ");
    Serial.print("# ");
    Serial.print( caldata.PCB); Serial.print("  ");
    Serial.println( caldata.version );
    Serial.print("# ");
    Serial.print( caldata.cal_gain, 4 ); Serial.print("  ");
    Serial.println( caldata.K_offset, 2 );
    adc.setCal( caldata.cal_gain, caldata.cal_offset );
    amb.setOffset( caldata.K_offset );
  }
  else { // if there was a problem with EEPROM read, then use default values
    Serial.println("# Failed to read EEPROM.  Using default calibration data. ");
    adc.setCal( CAL_GAIN, UV_OFFSET );
    amb.setOffset( AMB_OFFSET );
  }   

  fT[0].init( BT_FILTER ); // digital filtering on BT
  fT[1].init( ET_FILTER ); // digital filtering on ET
  fRise[0].init( RISE_FILTER ); // digital filtering for RoR calculation
  fRise[1].init( RISE_FILTER ); // digital filtering for RoR calculation
  fRoR[0].init( ROR_FILTER ); // post-filtering on RoR values
  fRoR[1].init( ROR_FILTER ); // post-filtering on RoR values

  tc[0] = &tc1; // allow different thermocouple types on outputs
  tc[1] = &tc2;
  
  output1.Setup( TIME_BASE );
  output1.Out( heater, 0 ); // heater is off by default
  
  io3.Setup( PWM_MODE, PWM_PRESCALE );
  set_fan_level( target_fan );  // fan is off by default
 
  first = true;
}

// -----------------------------------------------------------------
void loop() {
  float idletime;

  checkSerial(); // keep reading the incoming serial data
  slew_fan(); // ramp up the fan speed if slewing
  HIDevents(); // check the LCD keypad
  thisLoop = millis(); // timestamp for start of this loop
 

  if( thisLoop - lastLoop >= LOOPTIME ) { // time to take another sample
    if( first )
      resetTimer();
    lastLoop += LOOPTIME;
    timestamp = 0.001 * float( thisLoop ) - reftime; // system time, seconds, for this set of samples
    get_samples(); // retrieve values from MCP9800 and MCP3424
    if( first ) { // use first samples for RoR base values only
      first = false;
    }
    else {
      logger(); // output results to serial port
    }
    if( celsius ) {
      hid.refresh( F_TO_C(t1_cur), F_TO_C(t2_cur), 
                   RoR_cur / 1.8, timestamp, heater, target_fan ); // updates values for display
    }
    else {
      hid.refresh( t1_cur, t2_cur,RoR_cur, timestamp, heater, target_fan ); // updates values for display
    }
    for( int j = 0; j < NCHAN; j++ ) {
      flast[j] = ftemps[j]; // use previous values for calculating RoR
      lasttimes[j] = ftimes[j];
    }

    idletime = LOOPTIME - ( millis() - thisLoop );
    // arbitrary: complain if we don't have at least 50mS left
    if (idletime < 50 ) {
      Serial.print("# idle: ");
      Serial.println(idletime);
    }
  }
}

