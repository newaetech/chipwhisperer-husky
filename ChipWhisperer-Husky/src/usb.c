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

#define FW_VER_MAJOR 1
#define FW_VER_MINOR 1
#define FW_VER_DEBUG 0

#define REQ_MEMREAD_BULK 0x10
#define REQ_MEMWRITE_BULK 0x11
#define REQ_MEMREAD_CTRL 0x12
#define REQ_MEMWRITE_CTRL 0x13
#define REQ_MEMSTREAM 0x14
#define REQ_FPGA_STATUS 0x15
#define REQ_FPGA_PROGRAM 0x16
#define REQ_FW_VERSION 0x17
#define REQ_USART0_DATA 0x1A
#define REQ_USART0_CONFIG 0x1B
#define REQ_SCARD_DATA 0x1C
#define REQ_SCARD_CONFIG 0x1D
#define REQ_SCARD_AUX 0x1E
#define REQ_USART2DUMP_ENABLE 0x1F
#define REQ_XMEGA_PROGRAM 0x20
#define REQ_AVR_PROGRAM 0x21
#define REQ_SAM3U_CFG 0x22
#define REQ_CC_PROGRAM 0x23
#define REQ_CHANGE_PWR 0x24
#define REQ_FPGA_RESET 0x25
#define REQ_SPI_ADC 0x26 /* Not used - implemented in FPGA */
#define REQ_FAST_FIFO_READS 0x27
#define REQ_CDCI6214_ADDR 0x28 /* Not used anymore */
#define REQ_CDCI6214_DATA 0x29
#define REQ_CDC_SETTINGS_EN 0x31

#define USART_TARGET USART0
#define PIN_USART0_RXD	         (PIO_PA19_IDX)
#define PIN_USART0_RXD_FLAGS      (PIO_PERIPH_A | PIO_DEFAULT)
#define PIN_USART0_TXD	        (PIO_PA18_IDX)
#define PIN_USART0_TXD_FLAGS      (PIO_PERIPH_A | PIO_DEFAULT)
volatile bool g_captureinprogress = true;

static volatile bool main_b_vendor_enable = true;
static bool active = false;
static volatile bool cdc_settings_change[2] = {true, true};

uint8_t USB_PWR_STATE = 0;

COMPILER_WORD_ALIGNED
static uint8_t main_buf_loopback[MAIN_LOOPBACK_SIZE];

void main_vendor_bulk_in_received(udd_ep_status_t status,
                                  iram_size_t nb_transfered, udd_ep_id_t ep);

void stream_vendor_bulk_in_received(udd_ep_status_t status,
                                  iram_size_t nb_transfered, udd_ep_id_t ep);

void main_vendor_bulk_out_received(udd_ep_status_t status,
                                   iram_size_t nb_transfered, udd_ep_id_t ep);

//this stuff just turns leds on and off
void main_suspend_action(void)
{
	active = false;
	ui_powerdown();
}

void main_resume_action(void)
{
    ui_wakeup();
}

void main_sof_action(void)
{
    if (!main_b_vendor_enable)
        return;
    ui_process(udd_get_frame_number());
}

bool main_vendor_enable(void)
{
    active = true;
    main_b_vendor_enable = true;
    // Start data reception on OUT endpoints
#if UDI_VENDOR_EPS_SIZE_BULK_FS
    //main_vendor_bulk_in_received(UDD_EP_TRANSFER_OK, 0, 0);
    udi_vendor_bulk_out_run(
        main_buf_loopback,
        sizeof(main_buf_loopback),
        main_vendor_bulk_out_received);
#endif
    return true;
}

void main_vendor_disable(void)
{
    main_b_vendor_enable = false;
}

#define REQ_MEMREAD_BULK 0x10
#define REQ_MEMWRITE_BULK 0x11
#define REQ_MEMREAD_CTRL 0x12
#define REQ_MEMWRITE_CTRL 0x13
#define REQ_FW_VERSION 0x17
#define REQ_SAM3U_CFG 0x22

COMPILER_WORD_ALIGNED static uint8_t ctrlbuffer[64];
#define CTRLBUFFER_WORDPTR ((uint32_t *) ((void *)ctrlbuffer))

typedef enum {
    bep_emem=0,
    bep_fpgabitstream=10
} blockep_usage_t;

static blockep_usage_t blockendpoint_usage = bep_emem;

static uint8_t * ctrlmemread_buf;
static unsigned int ctrlmemread_size;

void ctrl_readmem_bulk(void);
void ctrl_readmem_ctrl(void);
void ctrl_writemem_bulk(void);
void ctrl_writemem_ctrl(void);
void ctrl_progfpga_bulk(void);

#define  udd_ack_nak_in(ep)                       (UDPHS->UDPHS_EPT[ep].UDPHS_EPTCLRSTA = UDPHS_EPTCLRSTA_NAK_IN)
volatile uint8_t read_blocker = 0;
void ctrl_readmem_bulk(void){
    uint32_t buflen = *(CTRLBUFFER_WORDPTR);
    uint32_t address = *(CTRLBUFFER_WORDPTR + 1);

    FPGA_releaselock();
    while(!FPGA_setlock(fpga_blockin));

    FPGA_setaddr(address);
    if  (!udi_vendor_bulk_in_run(
        (uint8_t *) PSRAM_BASE_ADDRESS,
        buflen,
        main_vendor_bulk_in_received
        )) {
            //abort
        }
    FPGA_releaselock();
}


void stream_mode_ready_handler(const uint32_t id, const uint32_t index)
{
    if (pio_get(PIN_EBI_USB_SPARE0_PORT, PIO_INPUT, 
                    PIN_EBI_USB_SPARE0_PIN)){

        pio_disable_interrupt(PIN_EBI_USB_SPARE0_PORT, PIN_EBI_USB_SPARE0_PIN);
        pio_configure_pin(LED1_GPIO, PIO_TYPE_PIO_OUTPUT_1 | PIO_DEFAULT);
    }
}

volatile uint32_t stream_buflen = 0;
volatile uint32_t stream_addr = 0;
void ctrl_streammode(void) {
    uint32_t buflen = *(CTRLBUFFER_WORDPTR);
    uint32_t address = *(CTRLBUFFER_WORDPTR + 1);

    stream_buflen = buflen;
    stream_addr = address;

    FPGA_releaselock();
    while(!FPGA_setlock(fpga_blockin));

    FPGA_setaddr(address);

    if (!pio_get(PIN_EBI_USB_SPARE0_PORT, PIO_INPUT, 
                    PIN_EBI_USB_SPARE0_PIN)) {
        buflen = 0;
        // pio_enable_interrupt(PIN_EBI_USB_SPARE0_PORT, PIN_EBI_USB_SPARE0_PIN);

    }
    udi_vendor_bulk_in_run(
        (uint8_t *) PSRAM_BASE_ADDRESS,
        buflen,
        stream_vendor_bulk_in_received
        );
    FPGA_releaselock();
}

void ctrl_readmem_ctrl(void){
    uint32_t buflen = *(CTRLBUFFER_WORDPTR);
    uint32_t address = *(CTRLBUFFER_WORDPTR + 1);

    FPGA_releaselock();
    while(!FPGA_setlock(fpga_ctrlmem));

    /* Set address */
    FPGA_setaddr(address);

    /* Do memory read */
    ctrlmemread_buf = (uint8_t *) PSRAM_BASE_ADDRESS;

    /* Set size to read */
    ctrlmemread_size = buflen;

    /* Start Transaction */
    FPGA_releaselock();
}


void ctrl_writemem_ctrl(void){
    uint32_t buflen = *(CTRLBUFFER_WORDPTR);
    uint32_t address = *(CTRLBUFFER_WORDPTR + 1);

    uint8_t * ctrlbuf_payload = (uint8_t *)(CTRLBUFFER_WORDPTR + 2);

    //printf("Writing to %x, %d\n", address, buflen);

    FPGA_releaselock();
    while(!FPGA_setlock(fpga_generic));

    /* Set address */
    FPGA_setaddr(address);

    /* Start Transaction */

    /* Do memory write */
    for(unsigned int i = 0; i < buflen; i++){
        xram[i] = ctrlbuf_payload[i];
    }

    FPGA_releaselock();
}

static uint32_t bulkread_address = 0;
static uint32_t bulkread_len = 0;

void ctrl_writemem_bulk(void){
//uint32_t buflen = *(CTRLBUFFER_WORDPTR);
    uint32_t address = *(CTRLBUFFER_WORDPTR + 1);

    // TODO: see block in
    FPGA_releaselock();
    while(!FPGA_setlock(fpga_blockout));

    /* Set address */
    FPGA_setaddr(address);

    /* Transaction done in generic callback */
    FPGA_releaselock();
}

static void ctrl_cdc_settings_cb(void)
{
    if (udd_g_ctrlreq.req.wValue & 0x01) {
        cdc_settings_change[0] = 1;
    } else {
        cdc_settings_change[0] = 0;
    }
    if (udd_g_ctrlreq.req.wValue & 0x02) {
        cdc_settings_change[1] = 1;
    } else {
        cdc_settings_change[1] = 0;
    }
}

static void ctrl_sam3ucfg_cb(void)
{
    switch(udd_g_ctrlreq.req.wValue & 0xFF)
    {
        /* Turn on slow clock */
    case 0x01:
        osc_enable(OSC_MAINCK_XTAL);
        osc_wait_ready(OSC_MAINCK_XTAL);
        pmc_switch_mck_to_mainck(CONFIG_SYSCLK_PRES);
        break;

        /* Turn off slow clock */
    case 0x02:
        pmc_switch_mck_to_pllack(CONFIG_SYSCLK_PRES);
        break;

        /* Jump to ROM-resident bootloader */
    case 0x03:
        /* Turn off connected stuff */
        //board_power(0);

        /* Clear ROM-mapping bit. */
        efc_perform_command(EFC0, EFC_FCMD_CGPB, 1);

        /* Disconnect USB (will kill connection) */
        udc_detach();

        /* With knowledge that I will rise again, I lay down my life. */
        while (RSTC->RSTC_SR & RSTC_SR_SRCMP);
        RSTC->RSTC_CR |= RSTC_CR_KEY(0xA5) | RSTC_CR_PERRST | RSTC_CR_PROCRST;
        while(1);
        break;
        /* Disconnect USB (will kill stuff) */

        /* Make the jump */
        break;
    case 0x10:
        udc_detach();
        while (RSTC->RSTC_SR & RSTC_SR_SRCMP);
        RSTC->RSTC_CR |= RSTC_CR_KEY(0xA5) | RSTC_CR_PERRST | RSTC_CR_PROCRST;
        while(1);
        break;
        
    case 0x11: // use in case of pipe error emergency
        FPGA_releaselock();
        break;

        /* Oh well, sucks to be you */
    default:
        break;
    }
}

void ctrl_progfpga_bulk(void){

    switch(udd_g_ctrlreq.req.wValue){
    case 0xA0:
        fpga_program_setup1();
        break;

    case 0xA1:
        /* Waiting on data... */
        fpga_program_setup2();
        blockendpoint_usage = bep_fpgabitstream;
        break;

    case 0xA2:
        /* Done */
        blockendpoint_usage = bep_emem;
        break;

    default:
        break;
    }
}

void ctrl_fpga_reset(void) {
  gpio_set_pin_high(PIN_EBI_USB_SPARE0);
  gpio_set_pin_low(PIN_EBI_USB_SPARE0);
}

static void ctrl_usart_cb(void)
{
	ctrl_usart(USART_TARGET, false);
}

static void ctrl_usart_cb_data(void)
{		
	//Catch heartbleed-style error
	if (udd_g_ctrlreq.req.wLength > udd_g_ctrlreq.payload_size){
		return;
	}
	
	for (int i = 0; i < udd_g_ctrlreq.req.wLength; i++){
		usart_driver_putchar(USART_TARGET, NULL, udd_g_ctrlreq.payload[i]);
	}
}

/*
  Implemented in FPGA - if this feature is needed in the future need to make it
  like I2C/PLL code, where both read & write are in this function to avoid
  too much delay in USB response time.
/*
static void spi_adc_cb(void)
{
	//Just do single byte writes for now
	if (udd_g_ctrlreq.req.wLength > 2){
		return;
	}
    write_spi_adc(udd_g_ctrlreq.payload[0], udd_g_ctrlreq.payload[1]);
}
*/

static void fast_fifo_reads_cb(void)
{
    if (udd_g_ctrlreq.payload[0] == 0) {
       smc_normaltiming();
    }
    else {
       smc_fasttiming();
    }
}


void ctrl_xmega_program_void(void)
{
	XPROGProtocol_Command();
}

void ctrl_avr_program_void(void)
{
	V2Protocol_ProcessCommand();
}


uint8_t cdci_status[3] = {0,0,0};

void cdci6214_data_cb(void)
{
	if (udd_g_ctrlreq.req.wLength != 5){
		return;
	}
    twi_enable_master_mode(TWI0);
	
	uint16_t cdci6214_addr = udd_g_ctrlreq.payload[1] | (udd_g_ctrlreq.payload[2] << 8);
	uint16_t data;
	
	if (udd_g_ctrlreq.payload[0]) {
		// WRITE request
		data = udd_g_ctrlreq.payload[3] | (udd_g_ctrlreq.payload[4] << 8);
		cdci_status[0] = cdci6214_write(cdci6214_addr, data) + 1;
		cdci_status[1] = udd_g_ctrlreq.payload[3];
		cdci_status[2] = udd_g_ctrlreq.payload[4];
	} else {
		//READ request
		cdci_status[0] = cdci6214_read(cdci6214_addr, &data) + 1;
		cdci_status[1] = data & 0xff;
		cdci_status[2] = data >> 8;
	}

    twi_disable_master_mode(TWI0);
	
	//cdci_status[0]: 1 == failed, 2 == ok, 0 == not done yet
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
    case REQ_MEMREAD_BULK:
        if (FPGA_setlock(fpga_usblocked)){
            udd_g_ctrlreq.callback = ctrl_readmem_bulk;
            return true;
        }
        break;
    case REQ_MEMREAD_CTRL:
        if (FPGA_setlock(fpga_usblocked)){
            udd_g_ctrlreq.callback = ctrl_readmem_ctrl;
            return true;
        }
        break;

    case REQ_MEMSTREAM:
        if (FPGA_setlock(fpga_usblocked)){
                udd_g_ctrlreq.callback = ctrl_streammode;
                return true;
        }
        break;

        /* Memory Write */
    case REQ_MEMWRITE_BULK:
        if (FPGA_setlock(fpga_usblocked)){
            udd_g_ctrlreq.callback = ctrl_writemem_bulk;
            return true;
        }
        break;
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
    case REQ_USART0_CONFIG:
        udd_g_ctrlreq.callback = ctrl_usart_cb;
        return true;
        
    case REQ_USART0_DATA:
        udd_g_ctrlreq.callback = ctrl_usart_cb_data;
        return true;

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

void stream_vendor_bulk_in_received(udd_ep_status_t status,
                                  iram_size_t nb_transfered, udd_ep_id_t ep)
{
    if (UDD_EP_TRANSFER_OK != status) {
        //return; // Transfer aborted/error
    }

    // if (FPGA_lockstatus() == fpga_blockin){
    //     FPGA_setlock(fpga_unlocked);
    // }

    // stream_buflen = buflen;
    // stream_addr = address;
    uint32_t buflen = stream_buflen;
    uint32_t address = stream_addr;

    FPGA_releaselock();
    while(!FPGA_setlock(fpga_blockin));

    FPGA_setaddr(address);

    if (!pio_get(PIN_EBI_USB_SPARE0_PORT, PIO_INPUT, 
                    PIN_EBI_USB_SPARE0_PIN)) {
        buflen = 0;
        // pio_enable_interrupt(PIN_EBI_USB_SPARE0_PORT, PIN_EBI_USB_SPARE0_PIN);

    }
    /* Do memory read */
    udi_vendor_bulk_in_run(
        (uint8_t *) PSRAM_BASE_ADDRESS,
        buflen,
        stream_vendor_bulk_in_received
        );
    FPGA_releaselock();

}
void main_vendor_bulk_in_received(udd_ep_status_t status,
                                  iram_size_t nb_transfered, udd_ep_id_t ep)
{
    UNUSED(nb_transfered);
    UNUSED(ep);
    if (UDD_EP_TRANSFER_OK != status) {
        return; // Transfer aborted/error
    }

    if (FPGA_lockstatus() == fpga_blockin){
        FPGA_setlock(fpga_unlocked);
    }
}

void main_vendor_bulk_out_received(udd_ep_status_t status,
                                   iram_size_t nb_transfered, udd_ep_id_t ep)
{
    UNUSED(ep);
    if (UDD_EP_TRANSFER_OK != status) {
        // Transfer aborted

        //restart
        udi_vendor_bulk_out_run(
            main_buf_loopback,
            sizeof(main_buf_loopback),
            main_vendor_bulk_out_received);

        return;
    }

    if (blockendpoint_usage == bep_emem){
        for(unsigned int i = 0; i < nb_transfered; i++){
            xram[i] = main_buf_loopback[i];
        }

        if (FPGA_lockstatus() == fpga_blockout){
            FPGA_releaselock();
        }
    } else if (blockendpoint_usage == bep_fpgabitstream){

        /* Send byte to FPGA - this could eventually be done via SPI */
        // TODO: is this dangerous?
        for(unsigned int i = 0; i < nb_transfered; i++){
            fpga_program_sendbyte(main_buf_loopback[i]);
        }
#if FPGA_USE_BITBANG
        FPGA_CCLK_LOW();
#endif
    }

    //printf("BULKOUT: %d bytes\n", (int)nb_transfered);

    udi_vendor_bulk_out_run(
        main_buf_loopback,
        sizeof(main_buf_loopback),
        main_vendor_bulk_out_received);
}



/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
// USB CDC
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
#include "usb_protocol_cdc.h"
volatile bool enable_cdc_transfer[2] = {false, false};
	extern volatile bool usart_x_enabled[4];
bool cdc_enable(uint8_t port)
{
	enable_cdc_transfer[port] = true;
	return true;
}

void cdc_disable(uint8_t port)
{
	enable_cdc_transfer[port] = false;
}

/*
		case REQ_USART0_DATA:
		for(cnt = 0; cnt < udd_g_ctrlreq.req.wLength; cnt++){
			respbuf[cnt] = usart_driver_getchar(USART_TARGET);
		}
		udd_g_ctrlreq.payload = respbuf;
		udd_g_ctrlreq.payload_size = cnt;
		return true;
		break;
		
			//Catch heartbleed-style error
			if (udd_g_ctrlreq.req.wLength > udd_g_ctrlreq.payload_size){
				return;
			}
			
			for (int i = 0; i < udd_g_ctrlreq.req.wLength; i++){
				usart_driver_putchar(USART_TARGET, NULL, udd_g_ctrlreq.payload[i]);
			}
*/
static uint8_t uart_buf[512] = {0};
void my_callback_rx_notify(uint8_t port)
{
	//iram_size_t udi_cdc_multi_get_nb_received_data
	
	if (enable_cdc_transfer[port] && usart_x_enabled[0]) {
		iram_size_t num_char = udi_cdc_multi_get_nb_received_data(port);
		while (num_char > 0) {
			num_char = (num_char > 512) ? 512 : num_char;
			udi_cdc_multi_read_buf(port, uart_buf, num_char);
			for (uint16_t i = 0; i < num_char; i++) { //num_char; num_char > 0; num_char--) {
				//usart_driver_putchar(USART_TARGET, NULL, udi_cdc_multi_getc(port));
				usart_driver_putchar(USART_TARGET, NULL, uart_buf[i]);
			}
			num_char = udi_cdc_multi_get_nb_received_data(port);
		}
		#if 0
		udi_cdc_read_no_polling(uart_buf, 128);
		uint8_t *st = uart_buf;
		while (*st) {
			udi_cdc_putc(*st++);
		}
		#endif
	}
}

extern tcirc_buf rx0buf, tx0buf;
extern tcirc_buf usb_usart_circ_buf;
extern volatile bool usart_x_enabled[4];

void my_callback_config(uint8_t port, usb_cdc_line_coding_t * cfg)
{
    if (port > 0)
        return;
	if (enable_cdc_transfer[port] && cdc_settings_change[port]) {
        usart_x_enabled[port] = true;
		sam_usart_opt_t usartopts;
		if (port != 0){
			return;
		}
		if (cfg->bDataBits < 5)
			return;
		if (cfg->bCharFormat > 2)
			return;
	
		usartopts.baudrate = cfg->dwDTERate;
		usartopts.channel_mode = US_MR_CHMODE_NORMAL;
		usartopts.stop_bits = ((uint32_t)cfg->bCharFormat) << 12;
		usartopts.char_length = ((uint32_t)cfg->bDataBits - 5) << 6;
		switch(cfg->bParityType) {
			case CDC_PAR_NONE:
			usartopts.parity_type = US_MR_PAR_NO;
			break;
			case CDC_PAR_ODD:
			usartopts.parity_type = US_MR_PAR_ODD;
			break;
			case CDC_PAR_EVEN:
			usartopts.parity_type = US_MR_PAR_EVEN;
			break;
			case CDC_PAR_MARK:
			usartopts.parity_type = US_MR_PAR_MARK;
			break;
			case CDC_PAR_SPACE:
			usartopts.parity_type = US_MR_PAR_SPACE;
			break;
			default:
			return;
		}
		if (port == 0)
		{
			//completely restart USART - otherwise breaks tx or stalls
			sysclk_enable_peripheral_clock(ID_USART0);
			init_circ_buf(&usb_usart_circ_buf);
			init_circ_buf(&tx0buf);
			init_circ_buf(&rx0buf);
			usart_init_rs232(USART0, &usartopts,  sysclk_get_cpu_hz());
			
			usart_enable_rx(USART0);
			usart_enable_tx(USART0);
			
			usart_enable_interrupt(USART0, UART_IER_RXRDY);
			
			gpio_configure_pin(PIN_USART0_RXD, PIN_USART0_RXD_FLAGS);
			gpio_configure_pin(PIN_USART0_TXD, PIN_USART0_TXD_FLAGS);
			irq_register_handler(USART0_IRQn, 5);
		}
	}
		
}
