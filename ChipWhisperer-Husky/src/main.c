#include <asf.h>
#include "conf_usb.h"
#include "stdio_serial.h"
#include "ui.h"
#include "genclk.h"
#include "tasks.h"
#include "usb_xmem.h"
#include "fpga_program.h"
#include "usb.h"
#include "sysclk.h"
#include <string.h>

//Serial Number - will be read by device ID
char usb_serial_number[33] = "000000000000DEADBEEF";


void phywhisperer_setup_pins(void)
{
    board_init();

    
    //Configure FPGA to allow programming via USB
    fpga_program_init();

    /* Enable SMC */
    pmc_enable_periph_clk(ID_SMC);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D0, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D1, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D2, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D3, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D4, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D5, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D6, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D7, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_NRD, PIN_EBI_NRD_FLAGS);
    gpio_configure_pin(PIN_EBI_NWE, PIN_EBI_NWE_FLAGS);
    gpio_configure_pin(PIN_EBI_NCS0, PIN_EBI_NCS0_FLAGS);

    gpio_configure_group(FPGA_ADDR_PORT, FPGA_ADDR_PINS, (PIO_TYPE_PIO_OUTPUT_0 | PIO_DEFAULT));
    pio_enable_output_write(FPGA_ADDR_PORT, FPGA_ADDR_PINS);

    gpio_configure_pin(PIN_EBI_ADDR_BUS_A0, PIN_EBI_ADDR_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_ADDR_BUS_A1, PIN_EBI_ADDR_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_ADDR_BUS_A2, PIN_EBI_ADDR_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_ADDR_BUS_A3, PIN_EBI_ADDR_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_ADDR_BUS_A4, PIN_EBI_ADDR_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_ADDR_BUS_A5, PIN_EBI_ADDR_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_ADDR_BUS_A6, PIN_EBI_ADDR_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_ADDR_BUS_A7, PIN_EBI_ADDR_BUS_FLAG1);

    gpio_configure_pin(PIN_EBI_USB_SPARE0, PIN_EBI_USB_SPARE0_FLAGS);
    gpio_configure_pin(PIN_EBI_USB_SPARE1, PIN_EBI_USB_SPARE1_FLAGS);

    smc_set_setup_timing(SMC, 0, SMC_SETUP_NWE_SETUP(0)
                         | SMC_SETUP_NCS_WR_SETUP(0)
                         | SMC_SETUP_NRD_SETUP(0)
                         | SMC_SETUP_NCS_RD_SETUP(0));
    smc_set_pulse_timing(SMC, 0, SMC_PULSE_NWE_PULSE(0)
                         | SMC_PULSE_NCS_WR_PULSE(0)
                         | SMC_PULSE_NRD_PULSE(0)
                         | SMC_PULSE_NCS_RD_PULSE(0));
    smc_set_cycle_timing(SMC, 0, SMC_CYCLE_NWE_CYCLE(1)
                         | SMC_CYCLE_NRD_CYCLE(1));
    smc_set_mode(SMC, 0, SMC_MODE_READ_MODE | SMC_MODE_WRITE_MODE
                 | SMC_MODE_DBW_BIT_8);
}

void hacky_delay(void)
{
    for (volatile uint32_t i = 0; i < 250000; i++);
}

static inline void genclk_enable_config(unsigned int id, enum genclk_source src, unsigned int divider)
{
    struct genclk_config gcfg;

    genclk_config_defaults(&gcfg, id);
    genclk_enable_source(src);
    genclk_config_set_source(&gcfg, src);
    genclk_config_set_divider(&gcfg, divider);
    genclk_enable(&gcfg, id);
}


int main(void)
{
    uint32_t serial_number[4];

    // Read Device-ID from SAM3U. Do this before enabling interrupts etc.
    flash_read_unique_id(serial_number, sizeof(serial_number));

    irq_initialize_vectors();
    cpu_irq_enable();

    // Initialize the sleep manager
    sleepmgr_init();
#if !SAMD21 && !SAMR21
    sysclk_init();
    phywhisperer_setup_pins();
#else
    system_init();
#endif

	//Convert serial number to ASCII for USB Serial number
	for(unsigned int i = 0; i < 4; i++){
		sprintf(usb_serial_number+(i*8), "%08x", (unsigned int)serial_number[i]);	
	}
	usb_serial_number[32] = 0;

    genclk_enable_config(GENCLK_PCK_1, GENCLK_PCK_SRC_MCK, GENCLK_PCK_PRES_1);
    udc_start();

    ui_init();
    while(1) {
        sleepmgr_enter_sleep();
    }
}
