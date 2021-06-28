/*

  Copyright (c) 2014-2015 NewAE Technology Inc. All rights reserved.
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
#include <string.h>
#include "conf_usb.h"
#include "stdio_serial.h"
#include "ui.h"
#include "genclk.h"
#include "usb.h"
#include "usb_xmem.h"
#include "fpga_program.h"
#include "circbuffer.h"
#include "usart_driver.h"
#include <string.h>
#include <cw521.h>
#include "cdci6214.h"
#include "twi.h"
#include "twi_master.h"


#define REQ_FPGA_STATUS 0x15
#define REQ_FPGA_PROGRAM 0x16
#define REQ_FW_VERSION 0x17
#define REQ_USART0_DATA 0x1A
#define REQ_USART0_CONFIG 0x1B

#define REQ_XMEGA_PROGRAM 0x20
#define REQ_AVR_PROGRAM 0x21
#define REQ_SAM3U_CFG 0x22
#define REQ_CC_PROGRAM 0x23
#define REQ_CHANGE_PWR 0x24
#define REQ_FPGA_RESET 0x25

#define REQ_FAST_FIFO_READS 0x27
#define REQ_CDCI6214_DATA 0x29
#define REQ_CDC_SETTINGS_EN 0x31




void ctrl_xmega_program_void(void)
{
	XPROGProtocol_Command();
}

void ctrl_avr_program_void(void)
{
	V2Protocol_ProcessCommand();
}


bool main_setup_out_received(void)
{
    //Add buffer if used
    udd_g_ctrlreq.payload = ctrlbuffer;
    udd_g_ctrlreq.payload_size = min(udd_g_ctrlreq.req.wLength,	sizeof(ctrlbuffer));

    blockendpoint_usage = bep_emem;
    static uint8_t  respbuf[128];
    switch(udd_g_ctrlreq.req.bRequest){
        /* Memory Read */
    // case REQ_XMEGA_PROGRAM:
    //     /*
    //     udd_g_ctrlreq.payload = xmegabuffer;
    //     udd_g_ctrlreq.payload_size = min(udd_g_ctrlreq.req.wLength,	sizeof(xmegabuffer));
    //     */
    //     udd_g_ctrlreq.callback = ctrl_xmega_program_void;
    //     return true;

    // /* AVR Programming */
    // case REQ_AVR_PROGRAM:
    //     udd_g_ctrlreq.callback = ctrl_avr_program_void;
    //     return true;


    case REQ_MEMWRITE_CTRL:
        if (FPGA_setlock(fpga_usblocked)){
            udd_g_ctrlreq.callback = ctrl_writemem_ctrl;
            return true;
        }
        break;

		/* Target serial */

    case REQ_FPGA_PROGRAM:
        udd_g_ctrlreq.callback = ctrl_progfpga_bulk;
        return true;

    case REQ_SAM3U_CFG:
        udd_g_ctrlreq.callback = ctrl_sam3ucfg_cb;
        return true;

    case REQ_FPGA_RESET:
        udd_g_ctrlreq.callback = ctrl_fpga_reset;
        return true;

    case REQ_SPI_ADC:
		/* Not needed as implemented in FPGA instead. */
		/*
        udd_g_ctrlreq.callback = spi_adc_cb;
        return true;
		*/
		return false;

    case REQ_FAST_FIFO_READS:
        udd_g_ctrlreq.callback = fast_fifo_reads_cb;
        return true;

    case REQ_CDC_SETTINGS_EN:
        udd_g_ctrlreq.callback = ctrl_cdc_settings_cb;
        return true;

    case REQ_CDCI6214_DATA:
        udd_g_ctrlreq.callback = cdci6214_data_cb;
        return true;

    default:
        return false;
    }

    return false;
}

/*
  udd_g_ctrlreq.req.bRequest == 0
  && (udd_g_ctrlreq.req.bRequest == 0)
  && (0 != udd_g_ctrlreq.req.wLength)
*/

bool main_setup_in_received(void)
{
    /*
      udd_g_ctrlreq.payload = main_buf_loopback;
      udd_g_ctrlreq.payload_size =
      min( udd_g_ctrlreq.req.wLength,
      sizeof(main_buf_loopback) );
    */

    static uint8_t  respbuf[64];
    unsigned int cnt;

    switch(udd_g_ctrlreq.req.bRequest){
    case REQ_MEMREAD_CTRL:
        udd_g_ctrlreq.payload = ctrlmemread_buf;
        udd_g_ctrlreq.payload_size = ctrlmemread_size;
        ctrlmemread_size = 0;

        if (FPGA_lockstatus() == fpga_ctrlmem){
            FPGA_setlock(fpga_unlocked);
        }

        return true;
        break;

    // case REQ_XMEGA_PROGRAM:
    //     return XPROGProtocol_Command();
    //     break;
        
    // case REQ_AVR_PROGRAM:
    //     return V2Protocol_ProcessCommand();
    //     break;
        
        
    case REQ_USART0_CONFIG:
        return ctrl_usart(USART_TARGET, true);
        break;
        
    case REQ_USART0_DATA:						
        for(cnt = 0; cnt < udd_g_ctrlreq.req.wLength; cnt++){
            respbuf[cnt] = usart_driver_getchar(USART_TARGET);
        }
        udd_g_ctrlreq.payload = respbuf;
        udd_g_ctrlreq.payload_size = cnt;
        return true;
        break;

    case REQ_FW_VERSION:
        respbuf[0] = FW_VER_MAJOR;
        respbuf[1] = FW_VER_MINOR;
        respbuf[2] = FW_VER_DEBUG;
        udd_g_ctrlreq.payload = respbuf;
        udd_g_ctrlreq.payload_size = 3;
        return true;
        break;

    case REQ_BUILD_DATE:
        0;
        strncpy(respbuf, BUILD_DATE, 100);
        udd_g_ctrlreq.payload = respbuf;
        udd_g_ctrlreq.payload_size = strlen(respbuf);
        return true;
        break;

    case REQ_FPGA_STATUS:
        respbuf[0] = FPGA_ISDONE();
        respbuf[1] = 0;
        respbuf[2] = 0;
        respbuf[3] = 0;
        udd_g_ctrlreq.payload = respbuf;
        udd_g_ctrlreq.payload_size = 4;
        return true;
        break;

    case REQ_SPI_ADC:
		return false;
		/* This should be done in the callback, but
		   we didn't fix at as the SPI comms were implemented
		   in the FPGA so this wasn't needed in the end.
		/*
        respbuf[0] = read_spi_adc(udd_g_ctrlreq.req.wValue & 0xFF);
        udd_g_ctrlreq.payload = respbuf;
        udd_g_ctrlreq.payload_size = 1;
        return true;
		*/
        break;

    case REQ_CDC_SETTINGS_EN:
        respbuf[0] = cdc_settings_change[0];
        respbuf[1] = cdc_settings_change[1];
        udd_g_ctrlreq.payload = respbuf;
        udd_g_ctrlreq.payload_size = 2;
        return true;
        break;

    case REQ_CDCI6214_DATA:		
		//0 means nothing new? Shouldn't be called then
		//if (cdci_status[0] == 0) {
		//	return false;
		//}
		respbuf[0] = cdci_status[0];
		respbuf[1] = cdci_status[1];
		respbuf[2] = cdci_status[2];
		
		cdci_status[0] = 0;
		cdci_status[1] = 0;
		cdci_status[2] = 0;        
        udd_g_ctrlreq.payload = respbuf;
        udd_g_ctrlreq.payload_size = 3;
        return true;
        break;

    default:
        return false;

    }
    return false;
}



