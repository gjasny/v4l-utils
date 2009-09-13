/*
 *  Driver for the Conexant CX25821 PCIe bridge
 *
 *  Copyright (C) 2009 Conexant Systems Inc. 
 *  Authors  <shu.lin@conexant.com>, <hiep.huynh@conexant.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "cx25821.h"


// Array for GPIO selected bits for a specific decoder.

enum GPIO_DEF { DECA_SEL = 0, 
			DECB_SEL, 
			DECC_SEL,
			DECD_SEL,
			MON_SEL = 4};

enum GPIO_OCTAL_DEF { OCTAL_DECA_SEL = 0, 
			OCTAL_DECB_SEL, 
			OCTAL_DECC_SEL,
			OCTAL_DECD_SEL,
			OCTAL_DECE_SEL,
			OCTAL_DECF_SEL,
			OCTAL_DECG_SEL,
			OCTAL_DECH_SEL,
			OCTAL_MON_SEL = 8 };


// Direction bits are at offset 23:16
#define Set_GPIO_Direction_Bit_To_OUTPUT(Bit)	((1 << Bit) << 16) 
    
struct medusa_decoder {
    int					_cur_dec;
	unsigned short		_num_vdec;
};

////////////////////////////////////////////////////////////////////////////////////////
// Selects Medusa Decoder Channel.
////////////////////////////////////////////////////////////////////////////////////////
void medusa_decoder_select(struct cx25821_dev *dev, struct medusa_decoder *dec, int decoder)
{            
    
	if (decoder < DECA_SEL && decoder > MON_SEL )
		return;

	dec->_cur_dec = decoder; 

    u32 gpioRegister = cx_read(GP0_IO);
	u32 value = (gpioRegister & 0xFFFFFFF0) | dec->_cur_dec;
    cx_write( GP0_IO, value );  
}

////////////////////////////////////////////////////////////////////////////////////////
// Sets the GPIO pin directions
// Parameters: 
//				
////////////////////////////////////////////////////////////////////////////////////////
void medusa_video_set_gpiopin_directions_to_output(struct cx25821_dev *dev, struct medusa_decoder *dec )
{
	// Here we will make sure that the GPIOs 0-3 are output. keep the rest as is
    u32 gpioRegister = cx_read( GP0_IO );
	
	// This operation will set the GPIO bits below.
	if (dec->_num_vdec == 4)
	{
		cx_write( GP0_IO, 
				gpioRegister 
    			| (Set_GPIO_Direction_Bit_To_OUTPUT(DECA_SEL)) 
    			| (Set_GPIO_Direction_Bit_To_OUTPUT(DECB_SEL)) 
				| (Set_GPIO_Direction_Bit_To_OUTPUT(DECC_SEL))
				| (Set_GPIO_Direction_Bit_To_OUTPUT(DECD_SEL))
				| (Set_GPIO_Direction_Bit_To_OUTPUT(MON_SEL)) );
		medusa_decoder_select(dev, dec, MON_SEL);
	}
	else
	{
		cx_write( GP0_IO, 
				gpioRegister 
    			| (Set_GPIO_Direction_Bit_To_OUTPUT(OCTAL_DECA_SEL)) 
    			| (Set_GPIO_Direction_Bit_To_OUTPUT(OCTAL_DECB_SEL)) 
				| (Set_GPIO_Direction_Bit_To_OUTPUT(OCTAL_DECC_SEL))
				| (Set_GPIO_Direction_Bit_To_OUTPUT(OCTAL_DECD_SEL))
				| (Set_GPIO_Direction_Bit_To_OUTPUT(OCTAL_DECE_SEL)) 
    			| (Set_GPIO_Direction_Bit_To_OUTPUT(OCTAL_DECF_SEL)) 
				| (Set_GPIO_Direction_Bit_To_OUTPUT(OCTAL_DECG_SEL))
				| (Set_GPIO_Direction_Bit_To_OUTPUT(OCTAL_DECH_SEL))
				| (Set_GPIO_Direction_Bit_To_OUTPUT(OCTAL_MON_SEL)) );
		medusa_decoder_select(dev, dec, OCTAL_MON_SEL);
	}
}
