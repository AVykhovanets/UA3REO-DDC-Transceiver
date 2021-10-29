#ifndef FT8_GUI_H_
#define FT8_GUI_H_

#include <stdint.h>
#include <stdbool.h>
#include "stm32h7xx_hal.h"

#define FT8_Menu_Max_Idx  2			//3 Buttons in total

#define button_spac_x 75				//Spacing betwenn the buttons
#define button_height 30
#define button_line 280
#define button_width 60

extern uint8_t FT8_Menu_Idx;

extern RTC_HandleTypeDef hrtc;

typedef struct
{
        char* text;
				bool  state;       
} ButtonStruct;



/*	- The buch of received messages are printed in the "decode ft8.c" - to be 
		correct will be good to move the print here
		
		- The time, SWR and the PWR are print in the FT8_main.c - - to be 
		correct will be good to move the print here
*/

void Unarm_FT8_Buttons(void);					//used to deactivate all buttons
void Draw_FT8_Buttons(void);
void drawButton(uint16_t i);
void Update_FT8_Menu_Cursor(void);
void FT8_Menu_Pos_Toggle(void);
void FT8_Print_Freq(void);
void FT8_Print_TargetCall(void);
void FT8_Clear_TargetCall(void);
void FT8_Clear_Mess_Field(void);
void FT8_Print_TX_Mess(char* message);
void FT8_Clear_TX_Mess(void);
void Enc2Rotate_Menager(int8_t direction, uint8_t decoded_msg);

#endif