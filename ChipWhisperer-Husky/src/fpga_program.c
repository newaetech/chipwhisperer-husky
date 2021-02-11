/*
  Xilinx Spartan 6 FPGA Programming Routines

  Copyright (c) 2014-2015 NewAE Technology Inc.  All rights reserved.
  Author: Colin O'Flynn, <coflynn@newae.com>

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
#include "fpga_program.h"
#include "spi.h"

/* FPGA Programming: Init pins, set to standby state */
void fpga_program_init(void)
{
    FPGA_NPROG_SETUP();
    FPGA_NPROG_HIGH();
}

/* FPGA Programming Step 1: Erase FPGA, setup SPI interface */
void fpga_program_setup1(void)
{
    /* Init - set program low to erase FPGA */
    FPGA_NPROG_LOW();

    usart_spi_opt_t spiopts;
    spiopts.baudrate = 10000000UL;
    spiopts.char_length = US_MR_CHRL_8_BIT;
    spiopts.channel_mode = US_MR_CHMODE_NORMAL;
    spiopts.spi_mode = SPI_MODE_0;

    sysclk_enable_peripheral_clock(FPGA_PROG_USART_ID);
    usart_init_spi_master(FPGA_PROG_USART, &spiopts, sysclk_get_cpu_hz());
    FPGA_DO_SETUP();
    FPGA_CCLK_SETUP();

    //pin setup doesn't transfer control over to USART, so have to do it manually
    PIOA->PIO_PDR = (1 << PIN_FPGA_DO_GPIO) | (1 << PIN_FPGA_CCLK_GPIO);
    usart_enable_tx(FPGA_PROG_USART);
}

/* FPGA Programming Step 2: Prepare FPGA for receiving programming data */
void fpga_program_setup2(void)
{
    FPGA_NPROG_HIGH();
}

//For debug only
//uint32_t fpga_total_bs_len;

/* FPGA Programming Step 3: Send data until done */
void fpga_program_sendbyte(uint8_t databyte)
{
    //For debug only
    //fpga_total_bs_len++;
    usart_putchar(FPGA_PROG_USART, databyte);
}
