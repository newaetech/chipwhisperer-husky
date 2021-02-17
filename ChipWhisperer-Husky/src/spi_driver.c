#include <asf.h>
#include <cw521.h>
#include "spi_driver.h"
#include "spi.h"


#define SPI_WVREQ_INIT    0x0010
#define SPI_WVREQ_ENABLE  0x0011
#define SPI_WVREQ_DISABLE 0x0012
#define SPI_WVREQ_NUMWAIT_TX 0x0018

#define word2buf(buf, word)   do{buf[0] = LSB0W(word); buf[1] = LSB1W(word); buf[2] = LSB2W(word); buf[3] = LSB3W(word);}while (0)
#define buf2word(word, buf)   do{word = *((uint32_t *)buf);}while (0)

tcirc_buf spi_rx_buf, spi_tx_buf;

static inline void spi_enable(void)
{
	gpio_configure_pin(SPI_MISO_GPIO, SPI_MISO_FLAGS);
	gpio_configure_pin(SPI_MOSI_GPIO, SPI_MOSI_FLAGS);
    gpio_configure_pin(SPI_SPCK_GPIO, SPI_SPCK_FLAGS);
    gpio_configure_pin(SPI_CS_GPIO, SPI_CS_FLAGS);
}

bool ctrl_spi(Spi * spi, bool directionIn)
{
    uint32_t baud;

    switch(udd_g_ctrlreq.req.wValue & 0xFF) {
        case SPI_WVREQ_INIT:
            if (directionIn) {
                if (udd_g_ctrlreq.req.wLength == 4) {
                    return true;
                }
            } else {
                if (udd_g_ctrlreq.req.wLength == 4) {
                    buf2word(baud, udd_g_ctrlreq.payload);
                    int16_t div = spi_calc_baudrate_div(baud, 0xFFFF);
                    if (div == -1) {
                        return false;
                    }
                    spi_set_baudrate_div(spi, 0, div);
                }
            }
    }
}

void spi_driver_putword(Spi *spi, tcirc_buf *txbuf, uint16_t data)
{
    add_to_circ_buf(txbuf, data & 0xFF, false);
    add_to_circ_buf(txbuf, (data >> 8), false);

}