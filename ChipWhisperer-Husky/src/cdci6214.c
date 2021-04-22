/*
 Copyright (c) 2015-2016 NewAE Technology Inc. All rights reserved.

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <asf.h>
#include "cdci6214.h"

#define CDCI6214_ADDR 0b1110100
//0x69

/* Init the CDCI906 chip, set offline */
bool cdci6214_init(void)
{
	gpio_configure_pin(PIN_CDCI_SDA, PIN_CDCI_SDA_FLAGS);
	gpio_configure_pin(PIN_CDCI_SCL, PIN_CDCI_SCL_FLAGS);
	
	twi_master_options_t opt = {
		.speed = 50000,
		.chip  = CDCI6214_ADDR
	};
	
	twi_master_setup(TWI0, &opt);
	
	uint16_t data = 0;
	
	/* Read addr 0 */
	if (cdci6214_read(0x18, &data) == false){
		return false;
	}
	
	/* Check vendor ID matches expected */
	if ((data & 0x0F) == 0x01){
		return true;
	}


	return false;
}



bool cdci6214_write(uint16_t addr, uint16_t data)
{
    uint8_t tmp[2];
    tmp[0] = data >> 8;
    tmp[1] = data & 0xFF;
	twi_package_t packet_write = {
		.addr         = {addr >> 8, addr & 0xFF},      // TWI slave memory address data
		.addr_length  = 2,    // TWI slave memory address data size
		.chip         = CDCI6214_ADDR,      // TWI slave bus address
		.buffer       = tmp, // transfer data source buffer
		.length       = 2  // transfer data size (bytes)
	};

	
	if (twi_master_write(TWI0, &packet_write) == TWI_SUCCESS){
		return true;
	} else {
		return false;
	}
}

bool cdci6214_read(uint16_t addr, uint16_t * data)
{
    uint8_t tmp[2];
	twi_package_t packet_read = {
		.addr         = {addr >> 8, addr & 0xFF},      // TWI slave memory address data
		.addr_length  = 2,    // TWI slave memory address data size
		.chip         = CDCI6214_ADDR,      // TWI slave bus address
		.buffer       = tmp,        // transfer data destination buffer
		.length       = 2,                    // transfer data size (bytes)
	};

	
	if(twi_master_read(TWI0, &packet_read) == TWI_SUCCESS){
        data[0] = tmp[0] << 8;
        data[0] |= tmp[1];
		return true;
	} else {
		return false;
	}	
}