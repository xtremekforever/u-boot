/*
 * (C) Copyright 2007-2008
 * Stelian Pop <stelian.pop@leadtechdesign.com>
 * Lead Tech Design <www.leadtechdesign.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <asm/sizes.h>
#include <asm/arch/at91sam9x5.h>
#include <asm/arch/at91sam9x5_matrix.h>
#include <asm/arch/at91sam9_smc.h>
#include <asm/arch/at91_common.h>
#include <asm/arch/at91_pmc.h>
#include <asm/arch/at91_rstc.h>
#include <asm/arch/clk.h>
#include <asm/arch/gpio.h>
#include <asm/arch/io.h>
#include <asm/arch/hardware.h>
#include <asm/arch/one_wire_info.h>
#include <lcd.h>
#include <atmel_lcdc.h>
#if defined(CONFIG_RESET_PHY_R) && defined(CONFIG_MACB)
#include <net.h>
#endif
#include <netdev.h>

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_LOAD_ONE_WIRE_INFO
static u32 system_rev;
static u32 system_serial_low;

u32 get_board_rev(void)
{
	return system_rev;
}

void get_board_serial(struct tag_serialnr *serialnr)
{
	serialnr->high = 0;	/* Not used */
	serialnr->low = system_serial_low;
}

void load_1wire_info(void)
{
	/* serial is in GPBR #2 and revision is in GPBR #3 */
	system_serial_low = at91_sys_read(AT91_GPBR + 4 * 2);
	system_rev = at91_sys_read(AT91_GPBR + 4 * 3);
}


#define nand_nfd0_on_d16(rev)	(cm_rev(rev) > 0)
#define nand_alt_rdy_busy(rev)	(cm_rev(rev) > 0)
#else
/* If no One Wire information: assume we are using revA boards */
#define nand_nfd0_on_d16(rev)	0
#define nand_alt_rdy_busy(rev)	0
#endif

/* ------------------------------------------------------------------------- */
/*
 * Miscelaneous platform dependent initialisations
 */
#ifdef CONFIG_CMD_NAND
static void at91sam9x5ek_nand_hw_init(void)
{
	unsigned long csa;

	/* Enable CS3 */
	csa = at91_sys_read(AT91_MATRIX_EBICSA);

	if (nand_nfd0_on_d16(system_rev)) {
		csa |= AT91_MATRIX_NFD0_ON_D16;
#ifndef CONFIG_SYS_NAND_DBW_16
		csa |= AT91_MATRIX_MP_ON;
#endif
	} else {
		csa &= ~AT91_MATRIX_NFD0_ON_D16;
		csa &= ~AT91_MATRIX_MP_ON;
	}
	/* Configure IO drive */
	csa &= ~AT91_MATRIX_EBI_EBI_IOSR;

	at91_sys_write(AT91_MATRIX_EBICSA,
		       csa | AT91_MATRIX_EBI_CS3A_SMC_NANDFLASH);

	/* Configure SMC CS3 for NAND/SmartMedia */



#ifdef CONFIG_ATMEL_NAND_HW_PMECC
	at91_sys_write(AT91_SMC_SETUP(3),
		       AT91_SMC_NWESETUP_(1) | AT91_SMC_NCS_WRSETUP_(0) |
		       AT91_SMC_NRDSETUP_(1) | AT91_SMC_NCS_RDSETUP_(0));
	at91_sys_write(AT91_SMC_PULSE(3),
		       AT91_SMC_NWEPULSE_(3) | AT91_SMC_NCS_WRPULSE_(5) |
		       AT91_SMC_NRDPULSE_(4) | AT91_SMC_NCS_RDPULSE_(6));
	at91_sys_write(AT91_SMC_CYCLE(3),
		       AT91_SMC_NWECYCLE_(5) | AT91_SMC_NRDCYCLE_(6));
	at91_sys_write(AT91_SMC_MODE(3),
		       AT91_SMC_READMODE | AT91_SMC_WRITEMODE |
		       AT91_SMC_EXNWMODE_DISABLE |
#ifdef CONFIG_SYS_NAND_DBW_16
		       AT91_SMC_DBW_16 |
#else /* CONFIG_SYS_NAND_DBW_8 */
		       AT91_SMC_DBW_8 |
#endif
		       AT91_SMC_TDF_(1));
#else
	at91_sys_write(AT91_SMC_SETUP(3),
		       AT91_SMC_NWESETUP_(2) | AT91_SMC_NCS_WRSETUP_(0) |
		       AT91_SMC_NRDSETUP_(2) | AT91_SMC_NCS_RDSETUP_(0));
	at91_sys_write(AT91_SMC_PULSE(3),
		       AT91_SMC_NWEPULSE_(4) | AT91_SMC_NCS_WRPULSE_(4) |
		       AT91_SMC_NRDPULSE_(4) | AT91_SMC_NCS_RDPULSE_(4));
	at91_sys_write(AT91_SMC_CYCLE(3),
		       AT91_SMC_NWECYCLE_(7) | AT91_SMC_NRDCYCLE_(7));
	at91_sys_write(AT91_SMC_MODE(3),
		       AT91_SMC_READMODE | AT91_SMC_WRITEMODE |
		       AT91_SMC_EXNWMODE_DISABLE |
#ifdef CONFIG_SYS_NAND_DBW_16
		       AT91_SMC_DBW_16 |
#else /* CONFIG_SYS_NAND_DBW_8 */
		       AT91_SMC_DBW_8 |
#endif
		       AT91_SMC_TDF_(3));
#endif /* CONFIG_ATMEL_NAND_HW_PMECC */

	at91_sys_write(AT91_PMC_PCER, 1 << AT91SAM9X5_ID_PIOCD);

	/* Configure RDY/BSY */
	if (nand_alt_rdy_busy(system_rev))
		at91_set_gpio_input(CONFIG_SYS_NAND_ALT_READY_PIN, 1);
	else
		at91_set_gpio_input(CONFIG_SYS_NAND_READY_PIN, 1);

	/* Enable NandFlash */
	at91_set_gpio_output(CONFIG_SYS_NAND_ENABLE_PIN, 1);

	at91_set_a_periph(AT91_PIO_PORTD, 0, 1);	/* NAND OE */
	at91_set_a_periph(AT91_PIO_PORTD, 1, 1);	/* NAND WE */
	at91_set_a_periph(AT91_PIO_PORTD, 2, 1);	/* ALE */
	at91_set_a_periph(AT91_PIO_PORTD, 3, 1);	/* CLE */

	if (nand_nfd0_on_d16(system_rev)) {
		at91_set_a_periph(AT91_PIO_PORTD, 6, 1);
		at91_set_a_periph(AT91_PIO_PORTD, 7, 1);
		at91_set_a_periph(AT91_PIO_PORTD, 8, 1);
		at91_set_a_periph(AT91_PIO_PORTD, 9, 1);
		at91_set_a_periph(AT91_PIO_PORTD, 10, 1);
		at91_set_a_periph(AT91_PIO_PORTD, 11, 1);
		at91_set_a_periph(AT91_PIO_PORTD, 12, 1);
		at91_set_a_periph(AT91_PIO_PORTD, 13, 1);
	}
}
#endif

#ifdef CONFIG_RESET_PHY_R
void reset_phy(void)
{
#ifdef CONFIG_MACB
	/*
	 * Initialize ethernet HW addr prior to starting Linux,
	 * needed for nfsroot
	 */
	eth_init(gd->bd);
#endif
}
#endif

int board_eth_init(bd_t *bis)
{
	int rc = 0;
#ifdef CONFIG_MACB
	if (has_emac0())
		rc = macb_eth_initialize(0,
			(void *)AT91SAM9X5_BASE_EMAC0, 0x00);
	if (has_emac1())
		rc = macb_eth_initialize(1,
			(void *)AT91SAM9X5_BASE_EMAC1, 0x00);
#endif
	return rc;
}

#ifdef CONFIG_MACB
static void at91sam9x5ek_macb_hw_init(void)
{
	/* Enable clock */
	if (has_emac0())
		at91_sys_write(AT91_PMC_PCER, 1 << AT91SAM9X5_ID_EMAC0);
	if (has_emac1())
		at91_sys_write(AT91_PMC_PCER, 1 << AT91SAM9X5_ID_EMAC1);
	at91_macb_hw_init();
}
#endif

#ifdef CONFIG_LCD

vidinfo_t panel_info = {
	vl_col:		800,
	vl_row:		480,
	vl_clk:		24000000,
	vl_sync:	ATMEL_LCDC_INVLINE_NORMAL |
			ATMEL_LCDC_INVFRAME_NORMAL,
	vl_bpix:	LCD_BPP,
	vl_tft:		1,
	vl_hsync_len:	128,
	vl_left_margin:	64,
	vl_right_margin:64,
	vl_vsync_len:	2,
	vl_upper_margin:22,
	vl_lower_margin:21,
	mmio:		AT91SAM9X5_BASE_LCDC,
};


void lcd_enable(void)
{
	if (has_lcdc())
		at91_set_A_periph(AT91_PIN_PC29, 1);	/* power up */
}

void lcd_disable(void)
{
	if (has_lcdc())
		at91_set_A_periph(AT91_PIN_PC29, 0);	/* power down */
}

static void at91sam9x5ek_lcd_hw_init(void)
{
	if (has_lcdc()) {
		at91_set_A_periph(AT91_PIN_PC26, 0);	/* LCDPWM */
		at91_set_A_periph(AT91_PIN_PC27, 0);	/* LCDVSYNC */
		at91_set_A_periph(AT91_PIN_PC28, 0);	/* LCDHSYNC */
		at91_set_A_periph(AT91_PIN_PC24, 0);	/* LCDDISP */
		at91_set_A_periph(AT91_PIN_PC29, 0);	/* LCDDEN */
		at91_set_A_periph(AT91_PIN_PC30, 0);	/* LCDPCK */

		at91_set_A_periph(AT91_PIN_PC0, 0);	/* LCDD0 */
		at91_set_A_periph(AT91_PIN_PC1, 0);	/* LCDD1 */
		at91_set_A_periph(AT91_PIN_PC2, 0);	/* LCDD2 */
		at91_set_A_periph(AT91_PIN_PC3, 0);	/* LCDD3 */
		at91_set_A_periph(AT91_PIN_PC4, 0);	/* LCDD4 */
		at91_set_A_periph(AT91_PIN_PC5, 0);	/* LCDD5 */
		at91_set_A_periph(AT91_PIN_PC6, 0);	/* LCDD6 */
		at91_set_A_periph(AT91_PIN_PC7, 0);	/* LCDD7 */
		at91_set_A_periph(AT91_PIN_PC8, 0);	/* LCDD8 */
		at91_set_A_periph(AT91_PIN_PC9, 0);	/* LCDD9 */
		at91_set_A_periph(AT91_PIN_PC10, 0);	/* LCDD10 */
		at91_set_A_periph(AT91_PIN_PC11, 0);	/* LCDD11 */
		at91_set_A_periph(AT91_PIN_PC12, 0);	/* LCDD12 */
		at91_set_B_periph(AT91_PIN_PC13, 0);	/* LCDD13 */
		at91_set_A_periph(AT91_PIN_PC14, 0);	/* LCDD14 */
		at91_set_A_periph(AT91_PIN_PC15, 0);	/* LCDD15 */
		at91_set_A_periph(AT91_PIN_PC16, 0);	/* LCDD16 */
		at91_set_A_periph(AT91_PIN_PC17, 0);	/* LCDD17 */
		at91_set_A_periph(AT91_PIN_PC18, 0);	/* LCDD18 */
		at91_set_A_periph(AT91_PIN_PC19, 0);	/* LCDD19 */
		at91_set_A_periph(AT91_PIN_PC20, 0);	/* LCDD20 */
		at91_set_B_periph(AT91_PIN_PC21, 0);	/* LCDD21 */
		at91_set_A_periph(AT91_PIN_PC22, 0);	/* LCDD22 */
		at91_set_A_periph(AT91_PIN_PC23, 0);	/* LCDD23 */

		at91_sys_write(AT91_PMC_PCER, 1 << AT91SAM9X5_ID_LCDC);

		gd->fb_base = CONFIG_AT91SAM9X5_LCD_BASE;
	}
}

#ifdef CONFIG_LCD_INFO
#include <nand.h>
#include <version.h>

void lcd_show_board_info(void)
{
	ulong dram_size, nand_size;
	int i;
	char temp[32];

	if (has_lcdc()) {
		lcd_printf("%s\n", U_BOOT_VERSION);
		lcd_printf("(C) 2010 ATMEL Corp\n");
		lcd_printf("at91support@atmel.com\n");
		lcd_printf("%s CPU at %s MHz\n",
			get_cpu_name(),
			strmhz(temp, get_cpu_clk_rate()));

		dram_size = 0;
		for (i = 0; i < CONFIG_NR_DRAM_BANKS; i++)
			dram_size += gd->bd->bi_dram[i].size;
		nand_size = 0;
		for (i = 0; i < CONFIG_SYS_MAX_NAND_DEVICE; i++)
			nand_size += nand_info[i].size;
		lcd_printf("  %ld MB SDRAM, %ld MB NAND\n",
			dram_size >> 20,
			nand_size >> 20);
	}
}
#endif /* CONFIG_LCD_INFO */
#endif /* CONFIG_LCD */

/* SPI chip select control */
#ifdef CONFIG_ATMEL_SPI
#include <spi.h>

int spi_cs_is_valid(unsigned int bus, unsigned int cs)
{
	return bus == 0 && cs < 2;
}

void spi_cs_activate(struct spi_slave *slave)
{
	switch (slave->cs) {
	case 1:
		at91_set_gpio_output(AT91_PIN_PA7, 0);
		break;
	case 0:
	default:
		at91_set_gpio_output(AT91_PIN_PA14, 0);
		break;
	}
}

void spi_cs_deactivate(struct spi_slave *slave)
{
	switch (slave->cs) {
	case 1:
		at91_set_gpio_output(AT91_PIN_PA7, 1);
		break;
	case 0:
	default:
		at91_set_gpio_output(AT91_PIN_PA14, 1);
		break;
	}
}
#endif /* CONFIG_ATMEL_SPI */


int board_init(void)
{
	/* Enable Ctrlc */
	console_init_f();

	/* arch number of AT91SAM9X5EK-Board */
	gd->bd->bi_arch_number = MACH_TYPE_AT91SAM9X5EK;

	/* adress of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM + 0x100;

	at91_serial_hw_init();

#ifdef CONFIG_LOAD_ONE_WIRE_INFO
	load_1wire_info();
#endif

#ifdef CONFIG_CMD_NAND
	at91sam9x5ek_nand_hw_init();
#endif

#ifdef CONFIG_ATMEL_SPI
	at91_spi0_hw_init(1 << 0);
	at91_spi0_hw_init(1 << 4);
#endif

#ifdef CONFIG_MACB
	at91sam9x5ek_macb_hw_init();
#endif

#ifdef CONFIG_LCD
	at91sam9x5ek_lcd_hw_init();
#endif
	return 0;
}

int dram_init(void)
{
	gd->bd->bi_dram[0].start = PHYS_SDRAM;
	gd->bd->bi_dram[0].size = PHYS_SDRAM_SIZE;
	return 0;
}


