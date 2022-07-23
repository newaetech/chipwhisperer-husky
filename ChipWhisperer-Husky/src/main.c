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
#include "circbuffer.h"
#include <string.h>
#include "naeusb_default.h"
#include "naeusb_openadc.h"
#include "naeusb_usart.h"
#include "naeusb_husky.h"
#include "cdci6214.h"

//Serial Number - will be read by device ID
char usb_serial_number[33] = "000000000000DEADBEEF";

extern volatile uint32_t current_transfer_len;


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

    //We don't use address peripheral anymore
/*
    gpio_configure_pin(PIN_EBI_ADDR_BUS_A0, PIN_EBI_ADDR_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_ADDR_BUS_A1, PIN_EBI_ADDR_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_ADDR_BUS_A2, PIN_EBI_ADDR_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_ADDR_BUS_A3, PIN_EBI_ADDR_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_ADDR_BUS_A4, PIN_EBI_ADDR_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_ADDR_BUS_A5, PIN_EBI_ADDR_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_ADDR_BUS_A6, PIN_EBI_ADDR_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_ADDR_BUS_A7, PIN_EBI_ADDR_BUS_FLAG1);
*/
    gpio_configure_pin(PIN_EBI_USB_SPARE0, PIN_EBI_USB_SPARE0_FLAGS);
    gpio_configure_pin(PIN_EBI_USB_SPARE1, PIN_EBI_USB_SPARE1_FLAGS);

    smc_normaltiming();
}

void hacky_delay(void)
{
    for (volatile uint32_t i = 0; i < 250000; i++);
}

// static inline void genclk_enable_config(unsigned int id, enum genclk_source src, unsigned int divider)
// {
//     struct genclk_config gcfg;

//     genclk_config_defaults(&gcfg, id);
//     genclk_enable_source(src);
//     genclk_config_set_source(&gcfg, src);
//     genclk_config_set_divider(&gcfg, divider);
//     genclk_enable(&gcfg, id);
// }
#include <spi.h>

#define ADC_SPI_CS PIO_PA3_IDX
#define ADC_RESET PIO_PA4_IDX

void enable_spi(void)
{
	gpio_configure_pin(SPI_MISO_GPIO, SPI_MISO_FLAGS);
	gpio_configure_pin(SPI_MOSI_GPIO, SPI_MOSI_FLAGS);
    gpio_configure_pin(SPI_SPCK_GPIO, SPI_SPCK_FLAGS);
    gpio_configure_pin(ADC_SPI_CS, SPI_CS_FLAGS);
    gpio_set_pin_high(ADC_SPI_CS);
    gpio_configure_pin(ADC_RESET, SPI_CS_FLAGS);
    // gpio_set_pin_low(ADC_RESET);
    gpio_set_pin_high(ADC_RESET);
    for (volatile uint32_t i = 0; i < 500; i++);
    gpio_set_pin_low(ADC_RESET);
    for (volatile uint32_t i = 0; i < 500; i++);

    spi_enable_clock(SPI);
    int16_t div = spi_calc_baudrate_div(960E3, 96E6); //960kHz
    spi_set_baudrate_div(SPI, 0, div);

    spi_set_master_mode(SPI);
    spi_set_clock_polarity(SPI, 0, 0);
    spi_set_bits_per_transfer(SPI, 0, 8);
    spi_set_clock_phase(SPI, 0, 1);

    spi_enable(SPI);
}


void stream_mode_ready_handler(const uint32_t id, const uint32_t index);
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

    pio_configure_pin_group(PIN_EBI_USB_SPARE0_PORT, 
        PIN_EBI_USB_SPARE0_PIN, PIN_EBI_USB_SPARE0_FLAGS);
    // enable_spi();
	cdci6214_init();
    pio_handler_set(PIN_EBI_USB_SPARE0_PORT, ID_PIOB, PIN_EBI_USB_SPARE0_PIN,
                    PIO_IT_RISE_EDGE, stream_mode_ready_handler);
    pio_disable_interrupt(PIN_EBI_USB_SPARE0_PORT, PIN_EBI_USB_SPARE0_PIN);
    NVIC_EnableIRQ(PIOB_IRQn);
    udc_start();

    ui_init();
    naeusb_register_handlers();
    naeusart_register_handlers();
    openadc_register_handlers();
    husky_register_handlers();
	mpsse_register_handlers();

	while (true) {
        if (current_transfer_len == 0) {
            cdc_send_to_pc();
            MPSSE_main_sendrecv_byte();
        }
	}
}
