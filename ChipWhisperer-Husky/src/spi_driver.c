#include <asf.h>
// #include <cw521.h>
#include "spi_driver.h"
#include "spi.h"


#define SPI_WVREQ_INIT    0x0010
#define SPI_WVREQ_ENABLE  0x0011
#define SPI_WVREQ_DISABLE 0x0012
#define SPI_WVREQ_NUMWAIT_TX 0x0018

#define word2buf(buf, word)   do{buf[0] = LSB0W(word); buf[1] = LSB1W(word); buf[2] = LSB2W(word); buf[3] = LSB3W(word);}while (0)
#define buf2word(word, buf)   do{word = *((uint32_t *)buf);}while (0)

tcirc_buf spi_rx_buf, spi_tx_buf;

#define ADC_SPI_CS PIO_PA3_IDX
#define ADC_RESET PIO_PA4_IDX

static inline void spi_driver_enable(void)
{
    //pmc_enable_periph_clk(ID_SPI);
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
}

bool write_spi_adc(uint8_t addr, uint8_t data)
{
    gpio_set_pin_low(ADC_SPI_CS);

    spi_put(SPI, addr);
    while(!spi_is_tx_empty(SPI));
    spi_put(SPI, data);
    while(!spi_is_tx_empty(SPI));

    gpio_set_pin_high(ADC_SPI_CS);

    return true;
}

uint8_t read_spi_adc(uint8_t addr)
{
    write_spi_adc(0x00, 0x01); //enable read, disable write
    gpio_set_pin_low(ADC_SPI_CS);
    spi_put(SPI, addr);
    while(!spi_is_tx_empty(SPI));
    spi_put(SPI, 0x00); //shift data in
    while(!spi_is_tx_empty(SPI));

    uint8_t rtn = spi_get(SPI);
    gpio_set_pin_high(ADC_SPI_CS);

    write_spi_adc(0x00, 0x00); //disable read, enable write
    return rtn;
}

// void spi_driver_putword(Spi *spi, tcirc_buf *txbuf, uint16_t data)
// {
//     // add_to_circ_buf(txbuf, data & 0xFF, false);
//     // add_to_circ_buf(txbuf, (data >> 8), false);

//     // if (spi_is_tx_ready(spi)) {

//     // }

//     gpio_set_pin_low(ADC_SPI_CS);
//     spi_put(spi, 0b1);
//     while(!spi_is_tx_empty(spi));
//     spi_put(spi, (0b11011 << 2));
//     while(!spi_is_tx_empty(spi));
//     gpio_set_pin_high(ADC_SPI_CS);

//     volatile uint16_t i = 0;
//     gpio_set_pin_low(ADC_SPI_CS);
//     spi_put(spi, 0x00); // enable rdout
//     while(!spi_is_tx_empty(spi));
//     spi_put(spi, 0x01); // enable rdout
//     while(!spi_is_tx_empty(spi));
//     gpio_set_pin_high(ADC_SPI_CS);

//     gpio_set_pin_low(ADC_SPI_CS);
//     spi_put(spi, (0b1)); // enable rdout
//     while(!spi_is_tx_empty(spi));
//     spi_put(spi, (0b0)); // enable rdout
//     while(!spi_is_tx_empty(spi));
//     i = spi_get(spi);
//     gpio_set_pin_high(ADC_SPI_CS);

//     gpio_set_pin_low(ADC_SPI_CS);
//     spi_put(spi, 0x00); // enable rdout
//     while(!spi_is_tx_empty(spi));
//     spi_put(spi, 0x00); // enable rdout
//     while(!spi_is_tx_empty(spi));
//     gpio_set_pin_high(ADC_SPI_CS);
// }