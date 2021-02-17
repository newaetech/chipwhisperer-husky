
/*
 * 
 *
 * Created: 2/17/2021
 *  Author: Alex Dewar
 */ 

#ifndef SPI_DRIVER_H_
#define SPI_DRIVER_H_

#include "circbuffer.h"
#include "spi.h"

bool ctrl_spi(Spi * usart, bool directionIn);
void spi_driver_putchar(Spi * usart, tcirc_buf * txbuf, uint8_t data);
uint8_t spi_driver_getchar(Spi * usart);

#endif /* USART_DRIVER_H_ */