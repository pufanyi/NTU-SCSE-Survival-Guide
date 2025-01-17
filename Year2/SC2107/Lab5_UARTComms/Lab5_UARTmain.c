// RSLK Self Test via UART

/* This example accompanies the books
   "Embedded Systems: Introduction to the MSP432 Microcontroller",
       ISBN: 978-1512185676, Jonathan Valvano, copyright (c) 2017
   "Embedded Systems: Real-Time Interfacing to the MSP432 Microcontroller",
       ISBN: 978-1514676585, Jonathan Valvano, copyright (c) 2017
   "Embedded Systems: Real-Time Operating Systems for ARM Cortex-M Microcontrollers",
       ISBN: 978-1466468863, , Jonathan Valvano, copyright (c) 2017
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/

Simplified BSD License (FreeBSD License)
Copyright (c) 2017, Jonathan Valvano, All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are
those of the authors and should not be interpreted as representing official
policies, either expressed or implied, of the FreeBSD Project.
*/

#include "msp.h"
#include <stdint.h>
#include <string.h>
#include "..\inc\UART0.h"
#include "..\inc\EUSCIA0.h"
#include "..\inc\FIFO0.h"
#include "..\inc\Clock.h"
//#include "..\inc\SysTick.h"
#include "..\inc\SysTickInts.h"
#include "..\inc\CortexM.h"
#include "..\inc\TimerA1.h"
//#include "..\inc\Bump.h"
#include "..\inc\BumpInt.h"
#include "..\inc\LaunchPad.h"
#include "..\inc\Motor.h"
#include "../inc/IRDistance.h"
#include "../inc/ADC14.h"
#include "../inc/LPF.h"
#include "..\inc\Reflectance.h"
#include "../inc/TA3InputCapture.h"
#include "../inc/Tachometer.h"
#include "..\inc\TExaS.h"

#define P2_4 (*((volatile uint8_t *)(0x42098070)))
#define P2_3 (*((volatile uint8_t *)(0x4209806C)))
#define P2_2 (*((volatile uint8_t *)(0x42098068)))
#define P2_1 (*((volatile uint8_t *)(0x42098064)))
#define P2_0 (*((volatile uint8_t *)(0x42098060)))


void RSLK_Reset(void){
    DisableInterrupts();
    Clock_Init48MHz();

    LaunchPad_Init();
    //Initialise modules used e.g. Reflectance Sensor, Bump Switch, Motor, Tachometer etc
    Motor_Init();
    IRSensor_Init();


    EnableInterrupts();
}

//IR SENSOR--------------------------------------
volatile uint32_t ADCvalue;
volatile uint32_t ADCflag;
volatile uint32_t nr, nc, nl;

void SensorRead_ISR(void)
{  // runs at 2000 Hz
    uint32_t raw17, raw12, raw16;
    P1OUT ^= 0x01;         // profile
    P1OUT ^= 0x01;         // profile
    ADC_In17_12_16(&raw17, &raw12, &raw16);  // sample
    nr = LPF_Calc(raw17);  // right is channel 17 P9.0
    nc = LPF_Calc2(raw12);  // center is channel 12, P4.1
    nl = LPF_Calc3(raw16);  // left is channel 16, P9.1
    ADCflag = 1;           // semaphore
    P1OUT ^= 0x01;         // profile
}

void IRSensor_Init(void)
{
    uint32_t raw17, raw12, raw16;
    uint32_t s;
    ADCflag = 0;
    s = 256; // replace with your choice
    ADC0_InitSWTriggerCh17_12_16();   // initialize channels 17,12,16
    ADC_In17_12_16(&raw17, &raw12, &raw16);  // sample
    LPF_Init(raw17, s);     // P9.0/channel 17
    LPF_Init2(raw12, s);     // P4.1/channel 12
    LPF_Init3(raw16, s);     // P9.1/channel 16
    UART0_Init();          // initialize UART0 115,200 baud rate
    LaunchPad_Init();
    TimerA1_Init(&SensorRead_ISR, 250);    // 2000 Hz sampling
}

//Bump Switch-------------------------------------

volatile uint8_t CollisionData, CollisionFlag;
void HandleCollision(uint8_t bumpSensor){
    Motor_Stop();
    CollisionData = bumpSensor;
    CollisionFlag = 1;
    P4->IFG &= ~0xED;
}

uint8_t ConvertCollisionData(uint8_t data){
    return data&0x3f;
}
//-------------------------------------
//Tacho
uint16_t Period0;              // (1/SMCLK) units = 83.3 ns units
uint16_t First0=0;             // Timer A3 first edge, P10.4
uint32_t Done0=0;              // set each rising

uint16_t Period2;              // (1/SMCLK) units = 83.3 ns units
uint16_t First2=0;             // Timer A3 first edge, P8.2
uint32_t Done2=0;              // set each rising
void PeriodMeasure0(uint16_t time){
  Period0 = (time - First0)&0xFFFF; // 16 bits, 83.3 ns resolution
  First0 = time;                    // setup for next
  Done0++;
}

// max period is (2^16-1)*83.3 ns = 5.4612 ms
// min period determined by time to run ISR, which is about 1 us
void PeriodMeasure2(uint16_t time){
  Period2 = (time - First2)&0xFFFF; // 16 bits, 83.3 ns resolution
  First2 = time;                    // setup for next
  Done2++;
}

void TimedPause(uint32_t time){
  Clock_Delay1ms(time);         // run for a while and stop
  Motor_Stop();
  while(LaunchPad_Input()==0);  // wait for touch
  while(LaunchPad_Input());     // wait for release
}



//--------------------------------

// RSLK Self-Test
// Sample program of how the text based menu can be designed.
// Only one entry (RSLK_Reset) is coded in the switch case. Fill up with other menu entries required for Lab5 assessment.
// Init function to various peripherals are commented off.  For reference only. Not the complete list.

int main(void) {
  uint32_t cmd=0xDEAD, menu=0;

  DisableInterrupts();
  Clock_Init48MHz();  // makes SMCLK=12 MHz
  //SysTick_Init(48000,2);  // set up SysTick for 1000 Hz interrupts
   Motor_Init();
  //Motor_Stop();
   LaunchPad_Init();
  //Bump_Init_Edge();
  //Bumper_Init();
  BumpInt_Init(&HandleCollision);
  IRSensor_Init();
  //Tachometer_Init();
  EUSCIA0_Init();     // initialize UART
  EnableInterrupts();

  while(1){                     // Loop forever
      // write this as part of Lab 5
      EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
      EUSCIA0_OutString("      ______    ______    ______   ________ "); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
      EUSCIA0_OutString("     /      \  /      \  /      \ |        \\");EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
      EUSCIA0_OutString("    |  $$$$$$\|  $$$$$$\|  $$$$$$\| $$$$$$$$"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
      EUSCIA0_OutString("    | $$___\$$| $$   \$$| $$___\$$| $$__    "); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
      EUSCIA0_OutString("     \$$    \ | $$       \$$    \ | $$  \   "); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
      EUSCIA0_OutString("     _\$$$$$$\| $$   __  _\$$$$$$\| $$$$$   "); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
      EUSCIA0_OutString("   |  \__| $$| $$__/  \|  \__| $$| $$_____ "); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
      EUSCIA0_OutString("    \$$    $$ \$$    $$ \$$    $$| $$     \\"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
      EUSCIA0_OutString("      \$$$$$$   \$$$$$$   \$$$$$$  \$$$$$$$$"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
      EUSCIA0_OutString("RSLK Testing"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
      EUSCIA0_OutString("[0] RSLK Reset"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
      EUSCIA0_OutString("[1] Motor Test"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
      EUSCIA0_OutString("[2] IR Sensor Test"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
      EUSCIA0_OutString("[3] Bumper Test"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
      EUSCIA0_OutString("[4] Reflectance Sensor Test"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
      EUSCIA0_OutString("[5] Tachometer Test"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
      EUSCIA0_OutString("[6] Extra: Display RPM"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
      EUSCIA0_OutString("[7] Extra: Line Following"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);



      EUSCIA0_OutString("CMD: ");
      cmd=EUSCIA0_InUDec();
      EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);

      switch(cmd){
//[0] RSLK Reset---------------------------------------------------------------
          case 0:
              EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);
              EUSCIA0_OutString("Resetting ......");
              EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);
              RSLK_Reset();

              menu =1;
              cmd=0xDEAD;
              break;

              // ....
//[1] Motor Test-----------------------------------------------------------------------------
          case 1:
              EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
              EUSCIA0_OutString("Motor Testing running: "); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
              EUSCIA0_OutString("[0] Go Forward"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
              EUSCIA0_OutString("[1] Go Backward"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
              EUSCIA0_OutString("[2] Turn Left"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
              EUSCIA0_OutString("[3] Turn Right"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);


              EUSCIA0_OutString("choice: ");
              uint32_t choice =EUSCIA0_InUDec();
              EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
              switch(choice){
                  case 0:
                      Motor_Forward(3000,3000);
                      Clock_Delay1ms(1000);
                      Motor_Stop();
                      break;
                  case 1:
                      Motor_Backward(3000,3000);
                      Clock_Delay1ms(1000);
                      Motor_Stop();
                      break;

                  case 2:
                      Motor_Left(0,3000);
                      Clock_Delay1ms(1000);
                      Motor_Stop();
                      break;

                  case 3:
                      Motor_Right(3000,0);
                      Clock_Delay1ms(1000);
                      Motor_Stop();
                      break;
                  default:
                      break;
              }
              menu = 1;
              cmd=0xDEAD;
              break;
//[2] IR Sensor test-----------------------------------------------------------------------------
      case 2:
          EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
          EUSCIA0_OutString("IR Sensor testing ......"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);

          int32_t n;
          Clock_Init48MHz();  //SMCLK=12Mhz

          EnableInterrupts();
          for(int i = 0; i<20; i++)
          {
          for(n=0; n<2000; n++){
                while(ADCflag == 0){};
                ADCflag = 0; // show every 2000th point
              }
          UART0_OutUDec5(LeftConvert(nl));UART0_OutString(" cm,");
          UART0_OutUDec5(CenterConvert(nc));UART0_OutString(" cm,");
          UART0_OutUDec5(RightConvert(nr));UART0_OutString(" cm\r\n");
          }

          menu = 1;
          cmd=0xDEAD;
          break;
//[3] Bumper Test-------------------------------------------------------------------------------------------------------------------------------
      case 3:
          EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
          EUSCIA0_OutString("Bump Switches Testing ......"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
          EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);

          CollisionData = 0x3F;
          CollisionFlag = 0;

           while(1){
              Motor_Forward(3000,3000);
              CollisionFlag = 0;
              WaitForInterrupt();
              if(CollisionFlag==1)
              {
                  uint8_t tmp = CollisionData & 0x3f;
                  for(int i = 0; i<6; i++){
                      int digit = tmp & 1;
                      tmp=tmp>>1;
                      if(digit == 1){
                          UART0_OutString("1");
                      }else{
                          UART0_OutString("0");
                      }
                  }
                  UART0_OutUDec5(ConvertCollisionData(CollisionData));
                  break;
              }

          }


            menu = 1;
            cmd = 0xDEAD;
            break;

//[4]Reflectance
      case 4:
          EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
          EUSCIA0_OutString("Reflectance testing......"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
          EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
          uint8_t refData = 0;
          while(LaunchPad_Input()==0){
              refData = Reflectance_Read(1000);
              for(int i = 0; i<8;i++){
                  EUSCIA0_OutUDec(refData%2);
                  EUSCIA0_OutString("-");
                  refData=refData/2;

              }
              EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
              Clock_Delay1ms(100);
          }

          menu = 1;
                      cmd = 0xDEAD;
                      break;

//[5]Tachometer----------------------------------------------------------------------------------------------
      case 5:
                EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
                EUSCIA0_OutString("Tachometer testing......"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
                EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
                uint32_t main_count=0;
                TimerA3Capture_Init(&PeriodMeasure0,&PeriodMeasure2);
                    TimedPause(500);
                    Motor_Forward(3000,3000);
                while(LaunchPad_Input()==0){
                      WaitForInterrupt();
                      main_count++;
                      if(main_count%1000){
                          UART0_OutString("Period0 = ");UART0_OutUDec5(Period0);UART0_OutString(" Period2 = ");UART0_OutUDec5(Period2);UART0_OutString(" \r\n");
                      }
                    }
                    Motor_Stop();
                                    menu = 1;
                                    cmd = 0xDEAD;
                                    break;


//[6]
//Counter of Timer-A is 16 bits wide, so the period measurement will have a precision of 16 bits
//SMCLK at 12 MHz and a prescale of 1 will give 83.33ns of period measurement
//The maximum period that can be measured is the precision in alternatives times the resolution, at current setting, maximum period that can be measured is 5.4ms
//360 pulses per rotation, slowest motor speed would be 30 rpm
/*
 *Speed(rpm)=(rotation/360pulses)*(10000000000ns/sec)*(60sec/min)(Period*83.33ns/pulse)
 */
      case 6:
                     EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
                     EUSCIA0_OutString("Extra: RPM Calc"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
                     EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
                     main_count=0;
                     TimerA3Capture_Init(&PeriodMeasure0,&PeriodMeasure2);
                     TimedPause(500);
                     Motor_Forward(3000,3000);
                     while(LaunchPad_Input()==0){
                           WaitForInterrupt();
                           main_count++;
                           if(main_count%1000){
                               //UART0_OutString("Period0 = ");UART0_OutUDec5(Period0);UART0_OutString(" Period2 = ");UART0_OutUDec5(Period2);UART0_OutString(" \r\n");
                               uint32_t rpm_l = 2000000/Period0;
                               uint32_t rpm_r = 2000000/Period2;
                               UART0_OutString("RpmLeft = ");UART0_OutUDec5(rpm_l);UART0_OutString(" RpmRight = ");UART0_OutUDec5(rpm_r);UART0_OutString(" \r\n");
                           }
                         }
                         Motor_Stop();
                                         menu = 1;
                                         cmd = 0xDEAD;
                                         break;

//[7] Line Following
case 7:
           EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
           EUSCIA0_OutString("Extra: Line Following"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
           EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
           int speed = 2000;
           CollisionData = 0x3F;
           CollisionFlag = 0;
            while (LaunchPad_Input() == 0)
            {


                WaitForInterrupt();
                if(CollisionFlag==1)
                      {
                                 uint8_t tmp = CollisionData & 0x3f;
                                 for(int i = 0; i<6; i++){
                                     int digit = tmp & 1;
                                     tmp=tmp>>1;
                                     if(digit == 1){
                                         UART0_OutString("1");
                                     }else{
                                         UART0_OutString("0");
                                     }
                                 }
                                 UART0_OutUDec5(ConvertCollisionData(CollisionData));
                                 break;
                             }
                int Data = Reflectance_Read(1000);
                int Position = Reflectance_Position(Data);
                Clock_Delay1ms(10);


            if (Position > -47 && Position < 47)
            { //center
                    Motor_Forward(2000, 2000);
            }
            else if (Position <= -47 && Position > -142)
            { //slightly off to the left
                Motor_Left(speed,speed/2);
                Clock_Delay1ms(50);
                Motor_Stop();
            }
            else if (Position >= 47 && Position <142)
            { //slightly off to the right
                Motor_Right(speed/2,speed);
                Clock_Delay1ms(50);
                Motor_Stop();
            }
            else if (Position <= -142 && Position >-237)
            { //off to the left
                Motor_Left(speed,speed/2);
                Clock_Delay1ms(100);
                Motor_Stop();
            }
            else if (Position >= 142 && Position < 237)
            { // off to the right
                Motor_Right(speed/2,speed);
                Clock_Delay1ms(100);
                Motor_Stop();
            }
            else if (Position <= -237 && Position > -332)
            { // way off left
                Motor_Left(speed,speed/2);
                Clock_Delay1ms(150);
                Motor_Stop();
            }
            else if (Position >= 237 && Position < 332)
            { // way off right
                Motor_Right(speed/2,speed);
                Clock_Delay1ms(150);
                Motor_Stop();
            }
            }



    menu = 1;
  cmd = 0xDEAD;
break;

//-----------------------------------------------------------------------------------------------
      default:
              menu=1;
              break;
      }

      if(!menu)Clock_Delay1ms(3000);
      else{
          menu=0;
      }

      // ....
      // ....
  }
}

#if 0
//Sample program for using the UART related functions.
int Program5_4(void){
//int main(void){
    // demonstrates features of the EUSCIA0 driver
  char ch;
  char string[20];
  uint32_t n;
  DisableInterrupts();
  Clock_Init48MHz();  // makes SMCLK=12 MHz
  EUSCIA0_Init();     // initialize UART
  EnableInterrupts();
  EUSCIA0_OutString("\nLab 5 Test program for EUSCIA0 driver\n\rEUSCIA0_OutChar examples\n");
  for(ch='A'; ch<='Z'; ch=ch+1){// print the uppercase alphabet
     EUSCIA0_OutChar(ch);
  }
  EUSCIA0_OutChar(LF);
  for(ch='a'; ch<='z'; ch=ch+1){// print the lowercase alphabet
    EUSCIA0_OutChar(ch);
  }
  while(1){
    EUSCIA0_OutString("\n\rInString: ");
    EUSCIA0_InString(string,19); // user enters a string
    EUSCIA0_OutString(" OutString="); EUSCIA0_OutString(string); EUSCIA0_OutChar(LF);

    EUSCIA0_OutString("InUDec: ");   n=EUSCIA0_InUDec();
    EUSCIA0_OutString(" OutUDec=");  EUSCIA0_OutUDec(n); EUSCIA0_OutChar(LF);
    EUSCIA0_OutString(" OutUFix1="); EUSCIA0_OutUFix1(n); EUSCIA0_OutChar(LF);
    EUSCIA0_OutString(" OutUFix2="); EUSCIA0_OutUFix2(n); EUSCIA0_OutChar(LF);

    EUSCIA0_OutString("InUHex: ");   n=EUSCIA0_InUHex();
    EUSCIA0_OutString(" OutUHex=");  EUSCIA0_OutUHex(n); EUSCIA0_OutChar(LF);
  }
}
#endif
