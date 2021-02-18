
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

// bool ctrl_spi(Spi * usart, bool directionIn);
// void spi_driver_putword(Spi * usart, tcirc_buf * txbuf, uint16_t data);
// uint8_t spi_driver_getchar(Spi * usart);

bool write_spi_adc(uint8_t addr, uint8_t data);
uint8_t read_spi_adc(uint8_t addr);
#endif /* USART_DRIVER_H_ */