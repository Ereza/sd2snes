/* sd2snes - SD card based universal cartridge for the SNES
   Copyright (C) 2009-2010 Maximilian Rehkopf <otakon@gmx.net>
   AVR firmware portion

   Inspired by and based on code from sd2iec, written by Ingo Korb et al.
   See sdcard.c|h, config.h.

   FAT file system access based on code by ChaN, Jim Brain, Ingo Korb,
   see ff.c|h.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License only.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

   fpga.c: FPGA (re)configuration
*/


/*
   FPGA pin mapping
   ================
   CCLK        P0.11 out
   PROG_B      P1.10 out
   INIT_B      P2.9  in
   DIN         P2.8  out
   DONE        P0.22 in
 */

#include <arm/NXP/LPC17xx/LPC17xx.h>
#include "bits.h"

#include "fpga.h"
#include "fpga_spi.h"
#include "config.h"
#include "uart.h"
#include "sdcard.h"
#include "diskio.h"
#include "integer.h"
#include "ff.h"
#include "fileops.h"
// XXX #include "fpga_spi.h"
#include "spi.h"
#include "led.h"
#include "timer.h"

/*DWORD get_fattime(void) {
	return 0L;
}*/
void fpga_set_prog_b(uint8_t val) {
	if(val)
	  BITBAND(PROGBREG->FIOSET, PROGBBIT) = 1;
	else
	  BITBAND(PROGBREG->FIOCLR, PROGBBIT) = 1;
}

void fpga_set_cclk(uint8_t val) {
	if(val)
	  BITBAND(CCLKREG->FIOSET, CCLKBIT) = 1;
	else
	  BITBAND(CCLKREG->FIOCLR, CCLKBIT) = 1;
}

int fpga_get_initb() {
	return BITBAND(INITBREG->FIOPIN, INITBBIT);
}

void fpga_init() {
/* mainly GPIO directions */
	BITBAND(CCLKREG->FIODIR, CCLKBIT) = 1; /* CCLK */
	BITBAND(DONEREG->FIODIR, DONEBIT) = 0; /* DONE */
	BITBAND(PROGBREG->FIODIR, PROGBBIT) = 1; /* PROG_B */
	BITBAND(DINREG->FIODIR, DINBIT) = 1; /* DIN */
	BITBAND(INITBREG->FIODIR, INITBBIT) = 0; /* INIT_B */

	LPC_GPIO2->FIOMASK1 = 0;

	SPI_OFFLOAD=0;
	fpga_set_cclk(0);    /* initial clk=0 */
}

int fpga_get_done(void) {
	return BITBAND(LPC_GPIO0->FIOPIN, 22);
}

void fpga_postinit() {
	LPC_GPIO2->FIOMASK1 = 0;
}

void fpga_pgm(uint8_t* filename) {
	int MAXRETRIES = 10;
	int retries = MAXRETRIES;
	do {
		fpga_set_prog_b(0);
		uart_putc('P');
		fpga_set_prog_b(1);
		while(!fpga_get_initb());
		LPC_GPIO2->FIOMASK1 = ~(BV(0));
		uart_putc('p');
		
		UINT bytes_read;

		// open configware file
		file_open(filename, FA_READ);
		if(file_res) {
			uart_putc('?');
			uart_putc(0x30+file_res);
			return;
		}
	
		for (;;) {
			bytes_read = file_read();
			if (file_res || bytes_read == 0) break;   // error or eof
			for(int i=0; i<bytes_read; i++) {
				FPGA_SEND_BYTE_SERIAL(file_buf[i]);
			}
		}
		file_close();
	} while (!fpga_get_done() && retries--);
	if(!fpga_get_done()) {
		printf("FPGA failed to configure after %d tries.\n", MAXRETRIES);
		delay_ms(50);
		led_panic();
	}
	printf("FPGA configured\n");
	fpga_postinit();
}

void set_mcu_ovr(uint8_t val) {
	if(!val) { // shared mode
		FPGA_SPI_SLOW();
		SET_MCU_OVR();
		// Disable SPI double speed mode -> clock = f/4
// XXX		SPSR = 0;
		printf("SPI slow\n");
	} else { // mcu only
		FPGA_SPI_FAST();
		CLR_MCU_OVR();
		// Enable SPI double speed mode -> clock = f/2
// XXX		SPSR = _BV(SPI2X);
		printf("SPI fast\n");
	}
}
