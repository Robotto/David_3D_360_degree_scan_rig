//Main file for the David 3D Scanner hardware automation robot machine..
//on the PIC24HJ128GP502
//The worlds messiest main file. EVER!

//include stuff:
#include <p24hj128gp502.h>
#include <libpic30.h>
#include "delay.h"
#include "pic24f.h"
#include "comm.h"
#include <stdio.h>	//Contains the sprintf  function


//Define stuff:
#define DIR 			_LATB7
#define MS1 			_LATB5
#define MS2 			_LATB6
#define STEP 			_LATB4
#define Servo_latch 	_LATB15
#define Servo_tris  	_TRISB15
#define LASER_tris 		_TRISB14
#define LASER	   		_LATB14
#define HALOGEN	   		_LATB10
#define HALOGEN_tris	_TRISB10




//Definitions of David-3D commands
#define ModeCalib		'1'
#define ModeScan		'2'
#define ModeTexture		'3'
#define StartScanning 	'S'
#define StopScanning 	'T'
#define SaveScan		'Z'
#define EraseScan		'E'
#define EraseTexture 	'F'
#define GrabTexture 	'G'
#define DavidClosed		'C'
#define DavidOpened		'O'

//Definitions for messages sent from david upon opening of 
//the different camera calibration dialogues:
#define SetupCamCalibAndTexture	'x'
#define SetupCamScanning		'y'




//global variables:

#define Servo_MAX  	1100 		//(1000*1µS = 1mS)  - At the top the value is 1100
#define Servo_MIN	750 		// static value...

#define Sweep_MAX	5 			//sets no. of laser sweeps pr scan.
#define Rotation_Degrees 36		//Sets number of degrees to rotate after each scan.

//set to 4 seconds:
#define texturedelaycount 40 	//Number of times X 100mS to delay after lights have gone on for texture grab.




//Servo controls:
int Tick;		// 1MHz incremented by timer1
int Pulse_hightime = Servo_MIN; //Startup Value for laser position
int Pulse_Period = 20000; //20000*1µS = 20 ms period time

int Kill_Switch; //turns everything on or off. if killswitch = 0 , everything runs as normal

//Variables used for controlling scans:
int performed_scans = 0;					  //counts performed scans
int scans_to_perform = 360/Rotation_Degrees;  //calculates number of times the object should rotate to have gone 360deg.
char title[30];							      //string that contains the name and number of the current scan
											  //(to send as part of the save command)



//function prototyping:
void GoToScanMode(void);
void GoToCamSetup_ScanMode(void);
void LightsButnoLine(void);
void RunScan(void);

void IoInit(void);
void Servo_sweep(char repeat);
void Step_degrees(int degrees);
void Microstep(int fraction,int degrees);
void RunGrabTexture(void);
void RunSaveScan(void);
void RunEraseScanAndTexture(void);
void RunTurnAndRepeat(void);
void RunFlashyAlarmLikeFunction(void);

//timer 1 interrupt function (for PWM generation):
void __attribute__((interrupt, auto_psv)) _T1Interrupt (void)
	{
	Tick++;		//PWM counter for this timer
	if(Tick>=Pulse_Period) Servo_latch = 1;  //output on Pulse pin goes high
	if(Tick>=Pulse_Period+Pulse_hightime) {Servo_latch = 0; Tick=0; } //Pulse pin goes low, timer tick is reset
	_T1IF = 0;		// Clear irq flag 
	}



//configure stuff:
_FOSCSEL(FNOSC_FRCPLL)	// Fast RC oscillator w/ divide and PLL
_FWDT(FWDTEN_OFF)	// Watchdog Timer Disabled
_FOSC(OSCIOFNC_ON)	// OSC2 (RA3) I/O functionality ON, so RA3 is NOT clock Output
//_FOSC(OSCIOFNC_OFF)	// OSC2 (RA3) I/O functionality OFF, so RA3 IS clock Output


		
		

int main(void)
{
	//__delay32(Delay_1S_Cnt);  //startup delay
	//Frequency setup:
	_PLLPRE=0;	 //prescale fosc to 5MHz
	_PLLDIV=40;	 // Multip. 5MHz to 260MHz
	_PLLPOST=0;	 // Divide 260MHz to 80MHz
	OSCTUN=8;	 //tune in to measured CLK		


		
	IoInit();			//initialize I/O
	uart_init();		/* Initialize UART driver */
	__delay32(Delay_100mS_Cnt);
	uart_string("David 3D hardware control module connected and ready!\n\n\r");



	while(1)
		{
		if(uart_test())
			{
			char input=uart_get();

			if		(input == DavidOpened)			{GoToScanMode(); RunFlashyAlarmLikeFunction();} //lights off, laser off, laser to bottom
			else if (input == DavidClosed)				GoToScanMode(); //lights off, laser off, laser to bottom

			else if	(input == ModeCalib) 				LightsButnoLine();	
			else if (input == SetupCamCalibAndTexture)	LightsButnoLine();
			
			

			else if (input == ModeScan) 				GoToScanMode();
			else if (input == SetupCamScanning) 		GoToCamSetup_ScanMode();
			else if (input == ModeTexture)				LightsButnoLine();

			else if (input == StartScanning)			
				{
				Kill_Switch=1;
				while(performed_scans<scans_to_perform)
					{
					RunScan();
							
					RunGrabTexture();
							
					RunSaveScan();

					performed_scans++;

					RunEraseScanAndTexture();
							
					if(scans_to_perform-performed_scans>0) RunTurnAndRepeat(); //if last scan wasn't just completed
					}
				performed_scans=0; //reset scan counter!
				RunFlashyAlarmLikeFunction();
				}

			////TEST MODE:
			else if (input == ',') {uart_string("36 grader\r\n"); Microstep(2,36);}
			else if (input == '.') {uart_string("sweep\r\n"); Servo_sweep(1);}

			}
		}
}

void GoToScanMode(void) 
	{
	HALOGEN=0;					//lights off
	LASER=0;					//laser off (doesn't turn on until scanning starts)
	Pulse_hightime=Servo_MIN;	//laser to bottom (start position)
	}

void GoToCamSetup_ScanMode(void)
	{
	//For determining the middle position of the laser, so that this can be set when calibrating the camera to scan mode:
	HALOGEN=0;				  //lights off
	LASER=1;				  //laser on
	Pulse_hightime=Servo_MAX; //set laser to middle position (for calibration of camera)
	}

void LightsButnoLine(void)
	{
	HALOGEN=1;		//lights on
	LASER=0;		//laser off
	}

void RunScan(void)
	{
	Servo_sweep(Sweep_MAX); //sweep!
	uart_put(StopScanning);  uart_put('\r');//Sends the StopScanning command to David-3D
	__delay32(Delay_10mS_Cnt);				//wait for david...
	if(uart_get()==StopScanning) __delay32(Delay_1mS_Cnt);	//If pause was pressed before sweeps were done, (david won't respond to the stop command)
	else Kill_Switch=1;										//this means that the killswitch is cleared, it has to be set again.
	}

void RunGrabTexture(void)
	{
	int texturedelaycounter; //initialize the for loop counter for texture grab delay
	HALOGEN=1;
	uart_put(ModeTexture); uart_put('\r');	//tell david to prepare for texture grab
	while(uart_get()!=ModeTexture)  __delay32(Delay_5mS_Cnt);	//wait for david to confirm
	
	//wait for lamps to go on, and especially for the camera to adjust.
	for(texturedelaycounter=0; texturedelaycounter<=texturedelaycount; texturedelaycounter++) 	__delay32(Delay_100mS_Cnt);

	uart_put(GrabTexture); uart_put('\r');	//tell david to grab the texture
	while(uart_get()!=GrabTexture)  __delay32(Delay_5mS_Cnt);	//wait for david to confirm texture is grabbed
	//HALOGEN=0;						//lamps off!
	}

void RunSaveScan(void)
	{
	//save scan
	// sprintf writes the scan number to the 'title' string, so it can be sent for each scan:
	sprintf(title, " Laser_Scanner_Scan_no_%d.obj\r", performed_scans);
	uart_put(SaveScan);  //tell david to send the scan
	uart_string(title);  uart_put('\r');//send the title

	while(uart_get()!=SaveScan) __delay32(Delay_5mS_Cnt);	//wait for david to confirm scan is saved
	}

void RunEraseScanAndTexture(void)
	{
		//Clear scan and texture, to prepare for another run.
		uart_put(EraseScan); uart_put('\r');
		while(uart_get()!=EraseScan) __delay32(Delay_5mS_Cnt);	//wait for david to confirm scan is erased

		uart_put(EraseTexture); uart_put('\r');		//is texture erase necessary?? i dunno..
		while(uart_get()!=EraseTexture) __delay32(Delay_5mS_Cnt);	//wait for david to confirm texture is erased
	}

void RunTurnAndRepeat(void)
	{
	
	Microstep(2,Rotation_Degrees); //Turn object defined number of degrees using half step. (see below)
	HALOGEN=0;						//lamps off!
	uart_put(ModeScan);  uart_put('\r'); //tell david to go back to scan mode.
	while(uart_get()!=ModeScan) __delay32(Delay_5mS_Cnt);	//wait for david to confirm scan mode
	uart_put(StartScanning); uart_put('\r'); 				//tell david to start scanning
	while(uart_get()!=StartScanning) __delay32(Delay_5mS_Cnt);	//wait for david to confirm scan start
	}

void RunFlashyAlarmLikeFunction(void) //DONE!! FLASHY FLASHY! x3!
	{
	int i;
	for(i=0;i<4;i++)  //flash 4 times!
		{
		HALOGEN=!HALOGEN;
		__delay32(Delay_100mS_Cnt);
		}
	}

void IoInit(void)
	{
	
	//IO configuration:
	AD1PCFGL=0xFFFF;     //all pins digital
	//I/O:
	TRISB = 0b1110000001000;
	Servo_tris = 0;
	Servo_latch = 0;
	LASER_tris = 0;
	HALOGEN_tris = 0;
	HALOGEN	 = 0; //lights off!
	LASER = 0; 	  //laser OFF!!!
	LATB=0;		//all outputs reset.. redundant...

	//Peripheral pin select for UART1:
	RPOR1 = 0b11;	    // U1TX -- RP2 (RB2)
	RPINR18bits.U1RXR = 0b11;	// U1RX -- RP3 (RB3) and set clear to send bits for uart 1

	/* Start Timer1 in interval time of xxµS */
	PR1 = 40; 				//timer1 PR1 register limit configured to interrupt every 1µS
	
	T1CONbits.TCKPS0 = 0;	//Prescaler set to Fcy/1
	T1CONbits.TON = 1;		// Start Timer1
	IEC0bits.T1IE = 1;  	// Enable Timer1 interrupt


	}


void Servo_sweep(char repeat)
	{
	//The repeat character determines the number of sweeps

	LASER = 1;  //laser on

	int sweep_count=0;		//counts number of performed sweeps
	for (sweep_count = 0; sweep_count < repeat; sweep_count++)  //for loop until desired number of sweeps
		{

		if(Kill_Switch==0) break; //stops the servo if the kill command has been recieved.

		//these two while loops change the PWM duty cycle for the servo, first up, then down..
		while(Pulse_hightime<Servo_MAX) //keep increasing until maximum high time 
			{
			Pulse_hightime++;
			__delay32(Delay_10mS_Cnt); //wait for servo to settle...
			}
	
		
		if(Kill_Switch==0) break; //check again at the top... :)
		
		while(Pulse_hightime>Servo_MIN) //keep decreasing until miniumum high time 
			{
			Pulse_hightime--;
			__delay32(Delay_10mS_Cnt); //wait for servo to settle...
			}
	

		}

	LASER = 0;  //turn the laser off

	}

void Step_degrees(int degrees)
	{
//TEST:
//LASER=1;
//test
	int steps = degrees * 100 / 180;
	//the step motor turns 1,8 degrees per step. This is translated into no. of step pulses per degree.
	while(steps>0)
		{
		//stepparoo!
		//remember acceleration and deceleration, and monkeys.. remember monkeys...
		STEP=1;
		__delay32(Delay_10mS_Cnt); //how is this delay?? should it be shorter?
		STEP=0;
		__delay32(Delay_10mS_Cnt);
		
		steps--;
		}
//TEST;
//LASER=0;
//test
	}

void Microstep(int fraction,int degrees)
	{
//the character "fraction" refers to the denominator (nævneren på dansk.. hvis du har drukket) in the
//fractions that represent the microstepping capabilities of the A3976 controller.
//the fractions are: 1/1, 1/2, 1/4 and 1/8. Therefore this function takes the arguments:
// 1 , 2 , 4, and 8.

	switch (fraction) 
		{
		case 1: MS1 = 0; MS2 = 0; break;
		case 2: MS1 = 1; MS2 = 0; break;
		case 4: MS1 = 0; MS2 = 1; break;
		case 8: MS1 = 1; MS2 = 1; break;
		default: MS1 = 0; MS2 = 0; break; //default is full step (no microstepping)
		}
	int new_degrees = fraction*degrees; //multiply by the microstep denominator to get the correct angle
	Step_degrees(new_degrees);

	}
