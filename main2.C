/*----------------------------------------------------------------------------
 * CMSIS-RTOS 'main' function template
 *---------------------------------------------------------------------------*/
// Faire un switch case o
#define osObjectsPublic                     // define objects in main module
#include "osObjects.h"                      // RTOS object definitions
#include "LPC17xx.h"                    // Device header
#include "GLCD_Config.h"                // Keil.MCB1700::Board Support:Graphic LCD
#include "Board_GLCD.h"                 // ::Board Support:Graphic LCD
#include "RTE_Device.h"                 // Keil::Device:Startup
#include "cmsis_os.h"                   // ARM::CMSIS:RTOS:Keil RTX
#include "CAN_CNA.h"       
#include "math.h"
#include "GPIO_LPC17xx.h"               // Keil::Device:GPIO
#include "stdio.h"
#include "Driver_SPI.h"
#include "Board_Buttons.h"    
#include "Driver_I2C.h"                 // ::CMSIS Driver:I2C

extern GLCD_FONT GLCD_Font_6x8;
extern GLCD_FONT GLCD_Font_16x24;
extern ARM_DRIVER_SPI Driver_SPI2;
extern ARM_DRIVER_I2C Driver_I2C2;

#define SLAVE_I2C_ADDR       0x70		// Adresse esclave sur 7 bits

void Conversion_DA(int sortie);
void contLED (char nbLED, char R, char G, char B);
void LEDvoid (void);
void LEDBlink(char nbLED, char R, char G, char B, char nb);
void Init_SPI(void);

void Thread1 (void const *argument);                             // thread function
osThreadId t1;                                          // thread id
osThreadDef (Thread1, osPriorityNormal, 1, 0);                   // thread object

void Thread2 (void const *argument);                             // thread function
osThreadId t2;                                          // thread id
osThreadDef (Thread2, osPriorityNormal, 1, 0);                   // thread object

//osMailQId MID;
//osEvent MaStructure;
//osMailQDef(monBal, 16, MaStructure);

void mySPI_Thread (void const *argument);                             // thread function
osThreadId tid_mySPI_Thread;                                          // thread id
osThreadDef (mySPI_Thread, osPriorityNormal, 1, 0); 

char tab[100];
char x=0;
/*void Thread5 (void const *argument);                             // thread function
osThreadId t5;                                          // thread id
osThreadDef (Thread5, osPriorityHigh, 1, 0);                   // thread object
*/
int n=0;
/*
 * main: initialize and start the system
 */

void mySPI_callback(uint32_t event)
{
	switch (event) 
	{
		case ARM_SPI_EVENT_TRANSFER_COMPLETE  : 	 osSignalSet(tid_mySPI_Thread, 0x02);
																							break;
		default : break;
	}
}

void cb_event(uint32_t event)
{
	switch (event) 
	{
		case ARM_I2C_EVENT_TRANSFER_DONE  : 	 osSignalSet(tid_mySPI_Thread, 0x01);
																							break;
		default : break;
	}
}

void Init_I2C(void){
	Driver_I2C2.Initialize(cb_event);
	Driver_I2C2.PowerControl(ARM_POWER_FULL);
	Driver_I2C2.Control(	ARM_I2C_BUS_SPEED,				// 2nd argument = débit
							ARM_I2C_BUS_SPEED_STANDARD  );			// 100 kHz
	Driver_I2C2.Control(	ARM_I2C_BUS_CLEAR,
							0 );
}

void Init_SPI(void)
{
	Driver_SPI2.Initialize(mySPI_callback);
	Driver_SPI2.PowerControl(ARM_POWER_FULL);
	Driver_SPI2.Control(ARM_SPI_MODE_MASTER | 
											ARM_SPI_CPOL1_CPHA1 | 
//											ARM_SPI_MSB_LSB | 
											ARM_SPI_SS_MASTER_UNUSED |
											ARM_SPI_DATA_BITS(8), 10000);
	Driver_SPI2.Control(ARM_SPI_CONTROL_SS, ARM_SPI_SS_INACTIVE);
}

void write1byte( unsigned char composant, unsigned char registre, unsigned char valeur)
{
	unsigned char tab [2];
	tab[0] = registre;                  //sous adresse du composant
	tab[1] = valeur;  
	Driver_I2C2.MasterTransmit (composant, tab, 2, false);
  osSignalWait(0x01,osWaitForever);
}

unsigned char read1byte (unsigned char composant, unsigned char registre)
{
		//unsigned char tab[2];
	  char maValeur;   
		// Ecriture vers registre esclave : START + ADDR(W) + 1W_DATA + 1W_DATA + STOP
		Driver_I2C2.MasterTransmit (composant, &registre, 1, true);		// false = avec stop
		osSignalWait(0x01,osWaitForever);	// attente fin transmission*/
		// Lecture de data esclave : START + ADDR(R) + 1R_DATA + STOP
		Driver_I2C2.MasterReceive (composant, &maValeur, 1, false);		// false = avec stop
		osSignalWait(0x01,osWaitForever);	
    return maValeur;
}

int main (void) {
	
  osKernelInitialize ();                    // initialize CMSIS-RTOS
  Init_SPI();
	Init_I2C();
	Buttons_Initialize();
	
	
	t1 = osThreadCreate (osThread(Thread1), NULL);
	t2 = osThreadCreate (osThread(Thread2), NULL);

	//t5 = osThreadCreate (osThread(Thread2), NULL);
	
	tid_mySPI_Thread = osThreadCreate (osThread(mySPI_Thread), NULL);
//	MID = osMailCreate(osMailQ(monBal),NULL);
	
	GLCD_Initialize();
	GLCD_ClearScreen();
	GLCD_SetFont(&GLCD_Font_16x24);
  // initialize peripherals here

  // create 'thread' functions that start executing,
  // example: tid_name = osThreadCreate (osThread(name), NULL);
  ADC_Init();	
	DAC_Init();
	
	LPC_SC->PCONP |= (1<<1); //Config timer 0
	LPC_TIM0->PR =  0;
	LPC_TIM0->MR0 = 1249; // 44.1 kHz
	LPC_TIM0->MCR = 3;		/*reset compteur si MR0=COUNTER + interruption*/
	LPC_TIM0->TCR = 1; 		//démarre le comptage

	NVIC_SetPriority(TIMER0_IRQn,3);
	NVIC_EnableIRQ(TIMER0_IRQn);
	
  LPC_PINCON->PINSEL4|=1<<20; // Config EINT0
	LPC_SC->EXTMODE=1;
	LPC_SC->EXTPOLAR=1;
	NVIC_SetPriority(EINT0_IRQn,3);
	NVIC_EnableIRQ(EINT0_IRQn);	
	NVIC_SetPriority(SPI_IRQn,2);
	
  osKernelStart ();                         // start thread execution 
}

void TIMER0_IRQHandler(void)
{
	LPC_TIM0->IR |= 1<<0;   // drapeau IT Timer0
 //osSignalSet (t1, 0x01);
	//osDelay(1000);
}

void EINT0_IRQHandler(void) 
{
	
	LPC_SC->EXTINT=1;
	LPC_GPIO1->FIOPIN=LPC_GPIO1->FIOPIN^(1<<28); 
//	x++;
//	if(x==3){x=0;}
  osSignalSet(tid_mySPI_Thread,0x04);             // Event 2 tache 1 à 1
	
}


void Thread1 (void const *argument) 
{
	int en;
//	char tab[50];

  while (1) 
	{

 // osSignalWait(0x01, osWaitForever);
	if(x!=0)
	{for(n=0;n<5000;n++)
		{
	en = 2048*sin(n*0.13823);
	Conversion_DA(en); // Conversion de l'entrée sinus
		}

	}
		switch (x) {
			case 0:
//		sprintf(tab,"Blabla %04d",1);	// Affichage texte
//		GLCD_DrawString(100,100,tab);
			case 1:
		//Conversion_DA(-2048);
		osDelay(800); // Sommeil toutes les secondes
//		sprintf(tab,"Blabla %04d",1);	// Affichage texte
//		GLCD_DrawString(100,100,tab);

			break;
			case 2:
		//Conversion_DA(-2048);
		osDelay(500); // Sommeil toutes les secondes
//		sprintf(tab,"Blabla %04d",2);	// Affichage texte
//		GLCD_DrawString(100,100,tab);

			break;
			case 3:
		//Conversion_DA(-2048);
 // Sommeil toutes les secondes
//		sprintf(tab,"Blabla %04d",3);	// Affichage texte
//		GLCD_DrawString(100,100,tab);
			
			break;
  }
}
}

void Thread2 (void const *argument) 
{
  while (1) 
	{
	 osDelay(osWaitForever);
  }
}

void LEDBlink(char nbLED, char R, char G, char B, char nb)
{
	int i, y;
	for(i=4;i<100;i=i+4)
	{
		tab[i]=0xf0;
		tab[i+1]=0x00; //B
		tab[i+2]=0x00; //V
		tab[i+3]=0x00; //R
	}
	for (y=0; y<nb; y++)
	{
		for(i=4;i<(nbLED*4);i=i+4)
		{
			tab[i]=0xf0;
			tab[i+1]=B; //B
			tab[i+2]=G; //V
			tab[i+3]=R; //R
		}
		osDelay(5);
		Driver_SPI2.Send(tab,80);
		//osSignalWait(0x02, osWaitForever);	// sommeil fin emission
		for(i=4;i<(nbLED*4);i=i+4)
		{
			tab[i]=0xf0;
			tab[i+1]=0; //B
			tab[i+2]=0; //V
			tab[i+3]=0; //R
		}
		osDelay(5);
		Driver_SPI2.Send(tab,80);
		//osSignalWait(0x02, osWaitForever);	// sommeil fin emission
	}
}

//fonction de CB lancee si Event T ou R

void LEDvoid (void)
{
	int i; 
	for (i=0;i<100;i++)
	{
		tab[i] = 0;
	}
}


void contLED (char nbLED, char R, char G, char B)
{ 
	int i;

	LEDvoid();
	for(i=4;i<100;i=i+4)
	{
		tab[i]=0xf0;
		tab[i+1]=0x00; //B
		tab[i+2]=0x00; //V
		tab[i+3]=0x00; //R
	}
	
	for(i=4;i<(nbLED*4);i=i+4)
	{
		tab[i]=0xF0;
		tab[i+1]=B; //B
		tab[i+2]=G; //V
		tab[i+3]=R; //R
	}
	
	Driver_SPI2.Send(tab,80);
	osSignalWait(0x02, osWaitForever);	// sommeil fin emission
//	osSignalWait(0x04, osWaitForever);
}

void mySPI_Thread (void const *argument) 
{
//	osEvent evt;
	int i,LEDVAL;
	
	for (i=0;i<100;i++)
	{
		tab[i] = 0;
	}

  while (1) 
	{ 	
		osDelay(10);
		write1byte(SLAVE_I2C_ADDR,0x00, 0x51);
		osDelay(70);
		LEDVAL=read1byte(SLAVE_I2C_ADDR,0x03);
		sprintf(tab,"Blabla %04d",x);	// Affichage texte
		GLCD_DrawString(10,10,tab);
		if(LEDVAL<10){x=3;}
		else if((LEDVAL<30)&&(LEDVAL>=10)){x=2;}
		else if((LEDVAL<45)&&(LEDVAL>=30)){x=1;}
		else  {x=0;}
		switch(x)
			{
				case 0:
					contLED(19, 0x00, 0x00, 0x00);
					break;
				case 1:
					contLED(15, 0x00, 0xFF, 0x00); //vert
					break;
				case 2:
					contLED(10, 0xFB, 0x20, 0x00); //orange
					break;
				case 3:
				//LEDBlink(5, 0xFF, 0x00, 0x00, 5);//clignotement rouge
				
			contLED(5, 0xFF, 0x00, 0x00); 
					break;
			}
  }
}
