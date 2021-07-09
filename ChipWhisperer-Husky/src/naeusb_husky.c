#include "naeusb_husky.h"
#include "cdci6214.h"
#include "spi_driver.h"
void stream_vendor_bulk_in_received(udd_ep_status_t status,
                                  iram_size_t nb_transfered, udd_ep_id_t ep);

volatile uint32_t stream_buflen = 0;
volatile uint32_t stream_addr = 0;
static volatile int32_t stream_total_len = 0;
volatile uint32_t current_transfer_len = 1;

void stream_mode_ready_handler(const uint32_t id, const uint32_t index)
{
    if (pio_get(PIN_EBI_USB_SPARE0_PORT, PIO_INPUT, 
                    PIN_EBI_USB_SPARE0_PIN)){
        pio_disable_interrupt(PIN_EBI_USB_SPARE0_PORT, PIN_EBI_USB_SPARE0_PIN);
        current_transfer_len = stream_buflen;
        FPGA_releaselock();
        while(!FPGA_setlock(fpga_blockin));
        if (stream_total_len < 0) {
            stream_total_len = 0;
        }
        if (stream_total_len <= current_transfer_len) {
            current_transfer_len = stream_total_len;
            udi_vendor_bulk_in_run(
                (uint8_t *) PSRAM_BASE_ADDRESS,
                stream_total_len,
                main_vendor_bulk_in_received
                );
                FPGA_releaselock();
                return;


        }
        FPGA_setaddr(stream_addr);
        udi_vendor_bulk_in_run(
            (uint8_t *) PSRAM_BASE_ADDRESS,
            stream_buflen,
            stream_vendor_bulk_in_received
            );
        FPGA_releaselock();
        return;
    } else {
        pio_configure_pin(LED1_GPIO, PIO_TYPE_PIO_OUTPUT_0 | PIO_DEFAULT);

    }
}

void husky_streammode(void) {
    volatile uint32_t buflen = *(CTRLBUFFER_WORDPTR);
    uint32_t address = *(CTRLBUFFER_WORDPTR + 1);
    stream_total_len = *(CTRLBUFFER_WORDPTR + 2);

    stream_buflen = buflen;
    stream_addr = address;


    FPGA_releaselock();
    while(!FPGA_setlock(fpga_blockin));

    FPGA_setaddr(address);

    if (!pio_get(PIN_EBI_USB_SPARE0_PORT, PIO_INPUT, 
                    PIN_EBI_USB_SPARE0_PIN)) {
        // buflen = 0;
        // started_read = 0;
        pio_enable_interrupt(PIN_EBI_USB_SPARE0_PORT, PIN_EBI_USB_SPARE0_PIN);
        return;

    }     
    
    current_transfer_len = buflen;
    udi_vendor_bulk_in_run(
        (uint8_t *) PSRAM_BASE_ADDRESS,
        buflen,
        stream_vendor_bulk_in_received
        );
}

void stream_vendor_bulk_in_received(udd_ep_status_t status,
                                  iram_size_t nb_transfered, udd_ep_id_t ep)
{
    FPGA_releaselock();
    if (UDD_EP_TRANSFER_OK != status) {
        return; // Transfer aborted/error
    }

    // if (FPGA_lockstatus() == fpga_blockin){
    //     FPGA_setlock(fpga_unlocked);
    // }

    // stream_buflen = buflen;
    // stream_addr = address;
    if (stream_total_len >= nb_transfered) {
        stream_total_len -= nb_transfered;
    }
    volatile uint32_t buflen = stream_buflen;
    uint32_t address = stream_addr;

    FPGA_releaselock();
    while(!FPGA_setlock(fpga_blockin));

    FPGA_setaddr(address);

    int i = 0;
    //check for a bit to make sure it doesn't go high after
    if (!pio_get(PIN_EBI_USB_SPARE0_PORT, PIO_INPUT, 
                    PIN_EBI_USB_SPARE0_PIN)) {
        if (stream_total_len > 0)
            pio_enable_interrupt(PIN_EBI_USB_SPARE0_PORT, PIN_EBI_USB_SPARE0_PIN);
        FPGA_releaselock();
        return;

    }     /* Do memory read */

    current_transfer_len = buflen;
    if (stream_total_len > buflen) {
        udi_vendor_bulk_in_run(
            (uint8_t *) PSRAM_BASE_ADDRESS,
            buflen,
            stream_vendor_bulk_in_received
            );
    } else {
        udi_vendor_bulk_in_run(
            (uint8_t *) PSRAM_BASE_ADDRESS,
            stream_total_len,
            main_vendor_bulk_in_received
            );

    }
    FPGA_releaselock();

}

void fast_fifo_reads_cb(void)
{
    if (udd_g_ctrlreq.payload[0] == 0) {
       smc_normaltiming();
    }
    else {
       smc_fasttiming();
    }
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

bool husky_setup_out_received(void)
{
    switch (udd_g_ctrlreq.req.bRequest) {
    case REQ_MEMSTREAM:
        if (FPGA_setlock(fpga_usblocked)){
                udd_g_ctrlreq.callback = husky_streammode;
                return true;
        }
        break;
    case REQ_FAST_FIFO_READS:
        udd_g_ctrlreq.callback = fast_fifo_reads_cb;
        return true;
    case REQ_CDCI6214_DATA:
        udd_g_ctrlreq.callback = cdci6214_data_cb;
        return true;

    }
    return false;

}

bool husky_setup_in_received(void)
{
    switch (udd_g_ctrlreq.req.bRequest) {
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
    }

    return false;

}

void husky_register_handlers(void)
{
    naeusb_add_in_handler(husky_setup_in_received);
    naeusb_add_out_handler(husky_setup_out_received);
}