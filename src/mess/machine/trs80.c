/***************************************************************************

  machine.c

  Functions to emulate general aspects of the machine (RAM, ROM, interrupts,
  I/O ports)

***************************************************************************/

/* Core includes */
#include "driver.h"
#include "machine/ctronics.h"
#include "machine/ay31015.h"
#include "sound/speaker.h"
#include "includes/trs80.h"

/* Components */
#include "cpu/z80/z80.h"
#include "machine/wd17xx.h"

/* Devices */
#include "devices/basicdsk.h"
#include "devices/cassette.h"
#include "devices/flopdrv.h"


#ifdef MAME_DEBUG
#define VERBOSE 1
#else
#define VERBOSE 0
#endif

#define LOG(x)	do { if (VERBOSE) logerror x; } while (0)


static UINT8 trs80_port_e0 = 0;
UINT8 trs80_port_ff = 0;		// will be changed to pass mode bits to video rather than port bits

#define IRQ_TIMER		0x80	/* RTC on Model I */
#define IRQ_RTC			0x04	/* RTC on Model 4 */
#define IRQ_FDC 		0x40	/* FDC on Model 1 */
static	UINT8 irq_status = 0;

#define MAX_LUMPS	192	 	/* crude storage units - don't now much about it */
#define MAX_GRANULES	8		/* lumps consisted of granules.. aha */
#define MAX_SECTORS 	5		/* and granules of sectors */

#ifdef USE_TRACK
static UINT8 track[4] = {0, };				/* current track per drive */
#endif
static UINT8 head;							 /* current head per drive */
#ifdef USE_SECTOR
static UINT8 sector[4] = {0, }; 			/* current sector per drive */
#endif
static UINT8 irq_mask = 0;
static UINT8 trs80_reg_load=1;

#define FW TRS80_FONT_W
#define FH TRS80_FONT_H

static double old_cassette_val;
static UINT8 cassette_data;
static emu_timer *cassette_data_timer;
static const device_config *trs80_printer;
static const device_config *trs80_ay31015;
static const device_config *trs80_cass;
static const device_config *trs80_speaker;
static const device_config *trs80_fdc;

static TIMER_CALLBACK( cassette_data_callback )
{
	double new_val = cassette_input(trs80_cass);

	/* Check for HI-LO transition */
	if ( old_cassette_val > -0.2 && new_val < -0.2 )
	{
		cassette_data |= 0x80;
	}
	old_cassette_val = new_val;
}


QUICKLOAD_LOAD( trs80_cmd )
{
	const address_space *space = cpu_get_address_space(image->machine->cpu[0], ADDRESS_SPACE_PROGRAM);
	UINT16 entry = 0, block_ofs = 0, block_len = 0;
	unsigned offs = 0;
	UINT8 *cmd_buff;

	cmd_buff = malloc(quickload_size);
	if (!cmd_buff)
		return INIT_FAIL;

	while( quickload_size > 3 )
	{
		UINT8 data = cmd_buff[offs++];

		switch( data ) {
		case 0x01:		   /* CMD file header */
		case 0x07:		   /* another type of CMD file header */
			block_len = cmd_buff[offs++];
			/* on CMD files size=zero means size 256 */
			if( block_len == 0 )
				block_len += 256;
			block_ofs = cmd_buff[offs++];
			block_ofs += 256 * cmd_buff[offs++];
			block_len -= 2;
			if( block_len == 0 )
				block_len = 256;
			quickload_size -= 4;
			LOG(("trs80_cmd_load block ($%02X) %d at $%04X\n", data, block_len, block_ofs));
			while( block_len && quickload_size )
			{
				memory_write_byte(space, block_ofs, cmd_buff[offs]);
				offs++;
				block_ofs++;
				block_len--;
				quickload_size--;
			}
			break;
		case 0x02:
			block_len = cmd_buff[offs++];
			quickload_size -= 1;
			if (entry == 0)
			{
				entry = cmd_buff[offs++];
				entry += 256 * cmd_buff[offs++];
				LOG(("trs80_cmd_load entry ($%02X) at $%04X\n", data, entry));
			}
			else
			{
				UINT16 temp;
				temp = cmd_buff[offs++];
				temp += 256 * cmd_buff[offs++];
				LOG(("trs80_cmd_load 2nd entry ($%02X) at $%04X ignored\n", data, temp));
			}
			quickload_size -= 3;
			break;
		default:
			quickload_size--;
		}
	}
	cpu_set_reg(image->machine->cpu[0], Z80_PC, entry);

	free(cmd_buff);
	return INIT_PASS;
}

DEVICE_IMAGE_LOAD( trs80_floppy )
{
	static UINT8 pdrive[4*16];
	int i;
	int tracks; 	/* total tracks count per drive */
	int heads;		/* total heads count per drive */
	int spt;		/* sector per track count per drive */
	int dir_sector; /* first directory sector (aka DDSL) */
	int dir_length; /* length of directory in sectors (aka DDGA) */
	int id = image_index_in_device(image);

    if (device_load_basicdsk_floppy(image) != INIT_PASS)
		return INIT_FAIL;

    if (image_index_in_device(image) == 0)        /* first floppy? */
	{
		image_fseek(image, 0, SEEK_SET);
		image_fread(image, pdrive, 2);
#if 0
		if (pdrive[0] != 0x00 || pdrive[1] != 0xfe)
		{
			basicdsk_read_sectormap(image, &tracks[id], &heads[id], &spt[id]);
		}
		else
#endif

		image_fseek(image, 2 * 256, SEEK_SET);
		image_fread(image, pdrive, 4*16);
	}

	tracks = pdrive[id*16+3] + 1;
	heads = (pdrive[id*16+7] & 0x40) ? 2 : 1;
	spt = pdrive[id*16+4] / heads;
	dir_sector = 5 * pdrive[id*16+0] * pdrive[id*16+5];
	dir_length = 5 * pdrive[id*16+9];

    /* set geometry so disk image can be read */
	basicdsk_set_geometry(image, tracks, heads, spt, 256, 0, 0, FALSE);

	/* mark directory sectors with deleted data address mark */
	/* assumption dir_sector is a sector offset */
	for (i = 0; i < dir_length; i++)
	{
		int track, side, sector_id;
		int track_offset, sector_offset;

		/* calc sector offset */
		sector_offset = dir_sector + i;

		/* get track offset */
		track_offset = sector_offset / spt;

		/* calc track */
		track = track_offset / heads;

		/* calc side */
		side = track_offset % heads;

		/* calc sector id - first sector id is 0! */
		sector_id = sector_offset % spt;

		/* set deleted data address mark for sector specified */
		basicdsk_set_ddam(image, track, side, sector_id, 1);
	}
	return INIT_PASS;
}

MACHINE_RESET( trs80 )
{
	cassette_data = 0;
	cassette_data_timer = timer_alloc(machine,  cassette_data_callback, NULL );
	timer_adjust_periodic( cassette_data_timer, attotime_zero, 0, ATTOTIME_IN_HZ(11025) );
	trs80_printer = devtag_get_device(machine, "centronics");
	trs80_ay31015 = devtag_get_device(machine, "tr1602");
	trs80_cass = devtag_get_device(machine, "cassette");
	trs80_speaker = devtag_get_device(machine, "speaker");
	trs80_fdc = devtag_get_device(machine, "wd179x");
}


DRIVER_INIT( trs80 )
{
	UINT8 *FNT = memory_region(machine, "gfx1");
	int i, y;

	for( i = 0; i < 0x80; i++ )
	{
		/* copy eight lines from the character generator */
		for (y = 0; y < 8; y++)
			FNT[i*FH+y] = FNT[0x800+i*8+y] << 3;
		/* wipe out the lower lines (no descenders!) */
		for (y = 8; y < FH; y++)
			FNT[i*FH+y] = 0;
	}
	/* setup the 2x3 chunky block graphics (two times 64 characters) */
	for( i = 0x80; i < 0x100; i++ )
	{
		UINT8 b0, b1, b2, b3, b4, b5;
		b0 = (i & 0x01) ? 0xe0 : 0x00;
		b1 = (i & 0x02) ? 0x1c : 0x00;
		b2 = (i & 0x04) ? 0xe0 : 0x00;
		b3 = (i & 0x08) ? 0x1c : 0x00;
		b4 = (i & 0x10) ? 0xe0 : 0x00;
		b5 = (i & 0x20) ? 0x1c : 0x00;

		FNT[i*FH+ 0] = FNT[i*FH+ 1] = FNT[i*FH+ 2] = FNT[i*FH+ 3] = b0 | b1;
		FNT[i*FH+ 4] = FNT[i*FH+ 5] = FNT[i*FH+ 6] = FNT[i*FH+ 7] = b2 | b3;
		FNT[i*FH+ 8] = FNT[i*FH+ 9] = FNT[i*FH+10] = FNT[i*FH+11] = b4 | b5;
	}
}

DRIVER_INIT( radionic )
{
	UINT8 *FNT = memory_region(machine, "gfx1");
	int i, y;

	for( i = 0; i < 0x80; i++ )
	{
		/* copy eight lines from the character generator, reversing the order of the dots */
		for (y = 0; y < 8; y++)
			FNT[i*FH+y] = BITSWAP8(FNT[0x800+i*8+y], 0, 1, 2, 3, 4, 5, 6, 7);

		/* now add descenders */
		for (y = 0; y < 4; y++)
			FNT[i*FH+y+8] = BITSWAP8(FNT[0x1000+i*8+y], 0, 1, 2, 3, 4, 5, 6, 7);
	}
	/* setup the 2x3 chunky block graphics (two times 64 characters) */
	for( i = 0x80; i < 0x100; i++ )
	{
		UINT8 b0, b1, b2, b3, b4, b5;
		b0 = (i & 0x01) ? 0xe0 : 0x00;
		b1 = (i & 0x02) ? 0x1c : 0x00;
		b2 = (i & 0x04) ? 0xe0 : 0x00;
		b3 = (i & 0x08) ? 0x1c : 0x00;
		b4 = (i & 0x10) ? 0xe0 : 0x00;
		b5 = (i & 0x20) ? 0x1c : 0x00;

		FNT[i*FH+ 0] = FNT[i*FH+ 1] = FNT[i*FH+ 2] = FNT[i*FH+ 3] = b0 | b1;
		FNT[i*FH+ 4] = FNT[i*FH+ 5] = FNT[i*FH+ 6] = FNT[i*FH+ 7] = b2 | b3;
		FNT[i*FH+ 8] = FNT[i*FH+ 9] = FNT[i*FH+10] = FNT[i*FH+11] = b4 | b5;
	}
}

DRIVER_INIT( lnw80 )
{
	UINT8 y, *FNT = memory_region(machine, "gfx1");
	UINT16 i, rows[] = { 0, 0x200, 0x100, 0x300, 1, 0x201, 0x101, 0x301 };

	for( i = 0; i < 0x80; i++ )
	{
		/* copy eight lines from the character generator */
		for (y = 0; y < 8; y++)
			FNT[i*FH+y] = BITSWAP8(FNT[0x800+(i<<1)+rows[y]], 2, 1, 6, 7, 5, 3, 4, 0); /* bits 0,3,4 are blank */
	}
	/* setup the 2x3 chunky block graphics (two times 64 characters) */
	for( i = 0x80; i < 0x100; i++ )
	{
		UINT8 b0, b1, b2, b3, b4, b5;
		b0 = (i & 0x01) ? 0xe0 : 0x00;
		b1 = (i & 0x02) ? 0x1c : 0x00;
		b2 = (i & 0x04) ? 0xe0 : 0x00;
		b3 = (i & 0x08) ? 0x1c : 0x00;
		b4 = (i & 0x10) ? 0xe0 : 0x00;
		b5 = (i & 0x20) ? 0x1c : 0x00;

		FNT[i*FH+ 0] = FNT[i*FH+ 1] = FNT[i*FH+ 2] = FNT[i*FH+ 3] = b0 | b1;
		FNT[i*FH+ 4] = FNT[i*FH+ 5] = FNT[i*FH+ 6] = FNT[i*FH+ 7] = b2 | b3;
		FNT[i*FH+ 8] = FNT[i*FH+ 9] = FNT[i*FH+10] = FNT[i*FH+11] = b4 | b5;
	}
}

DRIVER_INIT( ht1080z )
{
}


DRIVER_INIT( ht108064 )
{
	UINT8 *FNT = memory_region(machine, "gfx1");
	int i;
	for( i=0;i<0x800;i++) {
		FNT[i] &= 0xf8;
	}
}

/*************************************
 *
 *				Port handlers.
 *
 *************************************/

READ8_HANDLER( trs80m4_e0_r )
{
	return ~trs80_port_e0;
}

READ8_HANDLER( trs80m4_e8_r )
{
/* These bits directly read pins on the RS-232 socket, and are not emulated
	d7 Clear-to-Send (CTS), Pin 5 
	d6 Data-Set-Ready (DSR), pin 6 
	d5 Carrier Detect (CD), pin 8 
	d4 Ring Indicator (RI), pin 22 
	d3,d2,d0 Not used 
	d1 UART Receiver Input, pin 20 (pin 20 is also DTR) */

	return 0;	// this is a guess
}

READ8_HANDLER( trs80m4_ea_r )
{
/* UART Status Register 
	d7 Data Received ('1'=condition true) 
	d6 Transmitter Holding Register empty ('1'=condition true) 
	d5 Overrun Error ('1'=condition true) 
	d4 Framing Error ('1'=condition true) 
	d3 Parity Error ('1'=condition true) 
	d2..d0 Not used */

	UINT8 data=7;
	ay31015_set_input_pin( trs80_ay31015, AY31015_SWE, 0 );
	data |= ay31015_get_output_pin( trs80_ay31015, AY31015_TBMT ) ? 0x40 : 0;
	data |= ay31015_get_output_pin( trs80_ay31015, AY31015_DAV  ) ? 0x80 : 0;
	data |= ay31015_get_output_pin( trs80_ay31015, AY31015_OR   ) ? 0x20 : 0;
	data |= ay31015_get_output_pin( trs80_ay31015, AY31015_FE   ) ? 0x10 : 0;
	data |= ay31015_get_output_pin( trs80_ay31015, AY31015_PE   ) ? 0x08 : 0;
	ay31015_set_input_pin( trs80_ay31015, AY31015_SWE, 1 );

	return data;
}

READ8_HANDLER( trs80m4_eb_r )
{
/* UART received data */
	UINT8 data = ay31015_get_received_data( trs80_ay31015 );
	ay31015_set_input_pin( trs80_ay31015, AY31015_RDAV, 0 );
	ay31015_set_input_pin( trs80_ay31015, AY31015_RDAV, 1 );
	return data;
}

READ8_HANDLER( trs80m4_ec_r )
{
/* Reset the RTC interrupt */
//	trs80_port_e0 &= ~IRQ_RTC;
	return 0;
}

READ8_HANDLER( sys80_f9_r )
{
/* UART Status Register (d4..d6 not emulated)
	d7 Transmit buffer empty (inverted)
	d6 CTS pin
	d5 DSR pin
	d4 CD pin
	d3 Parity Error
	d2 Framing Error
	d1 Overrun
	d0 Data Available */

	UINT8 data=70;
	ay31015_set_input_pin( trs80_ay31015, AY31015_SWE, 0 );
	data |= ay31015_get_output_pin( trs80_ay31015, AY31015_TBMT ) ? 0 : 0x80;
	data |= ay31015_get_output_pin( trs80_ay31015, AY31015_DAV  ) ? 0x01 : 0;
	data |= ay31015_get_output_pin( trs80_ay31015, AY31015_OR   ) ? 0x02 : 0;
	data |= ay31015_get_output_pin( trs80_ay31015, AY31015_FE   ) ? 0x04 : 0;
	data |= ay31015_get_output_pin( trs80_ay31015, AY31015_PE   ) ? 0x08 : 0;
	ay31015_set_input_pin( trs80_ay31015, AY31015_SWE, 1 );

	return data;
}

READ8_HANDLER( trs80_ff_r )
{
	UINT8 data = (~trs80_port_ff & 8) << 3;	// MODESEL bit (32 or 64 chars per line)
	return data | cassette_data;
}

READ8_HANDLER( trs80m4_ff_r )
{
/* Return of cassette data stream from tape
	d7 Low-speed data
	d0 High-speed data (not emulated yet) */

	UINT8 data = trs80m4_ec_r(space, 0);	// this is a mirror of 0xec
	data++;					// get rid of compile error
	return cassette_data;
}

WRITE8_HANDLER( trs80m4_84_w )
{
/* Hi-res graphics control (not emulated)
	d7 Page Control
	d6 Fix upper memory
	d5 Memory bit 1
	d4 Memory bit 0
	d3 Invert Video (whole screen)
	d2 80/64 width
	d1 Select bit 1
	d0 Select bit 0 */
}

WRITE8_HANDLER( trs80m4_90_w )
{
	speaker_level_w(trs80_speaker, ~data & 1);
}

WRITE8_HANDLER( trs80m4_e0_w )
{
/* Interrupt settings
	d6 Enable Rec Err
	d5 Enable Rec Data
	d4 Enable Xmit Emp
	d3 Enable I/O int
	d2 Enable RT int
	d1 C fall Int
	d0 C Rise Int */

	trs80_port_e0 = data;
}

WRITE8_HANDLER( trs80m4_e8_w )
{
/* d1 when '1' enables control register load (see below) */
	trs80_reg_load = data & 2;
}

WRITE8_HANDLER( trs80m4_e9_w )
{
/* UART set baud rate. Rx = bits 0..3, Tx = bits 4..7
	00h    50  
	11h    75  
	22h    100  
	33h    134.5  
	44h    150  
	55h    300  
	66h    600  
	77h    1200  
	88h    1800  
	99h    2000  
	AAh    2400  
	BBh    3600  
	CCh    4800  
	DDh    7200  
	EEh    9600  
	FFh    19200 */

	int baud_clock[]={ 800, 1200, 1600, 2152, 2400, 4800, 9600, 19200, 28800, 32000, 38400, 57600, 76800, 115200, 153600, 307200 };
	ay31015_set_receiver_clock( trs80_ay31015, baud_clock[data & 0x0f]);
	ay31015_set_transmitter_clock( trs80_ay31015, baud_clock[data>>4]);
}

WRITE8_HANDLER( trs80m4_ea_w )
{
	if (trs80_reg_load)

/* bits d7..d3 are UART control; d2..d0 directly adjust levels at the RS-232 socket - we emulate UART control only
	d7 Even Parity Enable ('1'=even, '0'=odd) 
	d6='1',d5='1' for 8 bits 
	d6='0',d5='1' for 7 bits 
	d6='1',d5='0' for 6 bits 
	d6='0',d5='0' for 5 bits
	d4 Stop Bit Select ('1'=two stop bits, '0'=one stop bit) 
	d3 Parity Inhibit ('1'=disable; No parity, '0'=parity enabled) 
	d2 Break ('0'=disable transmit data; continuous RS232 'SPACE' condition) 
	d1 Request-to-Send (RTS), pin 4
	d0 Data-Terminal-Ready (DTR), pin 20 */

	{
		ay31015_set_input_pin( trs80_ay31015, AY31015_CS, 0 );
		ay31015_set_input_pin( trs80_ay31015, AY31015_NB1, ( data & 0x40 ) ? 1 : 0 );
		ay31015_set_input_pin( trs80_ay31015, AY31015_NB2, ( data & 0x20 ) ? 1 : 0 );
		ay31015_set_input_pin( trs80_ay31015, AY31015_TSB, ( data & 0x10 ) ? 1 : 0 );
		ay31015_set_input_pin( trs80_ay31015, AY31015_EPS, ( data & 0x80 ) ? 1 : 0 );
		ay31015_set_input_pin( trs80_ay31015, AY31015_NP,  ( data & 0x08 ) ? 1 : 0 );
		ay31015_set_input_pin( trs80_ay31015, AY31015_CS, 1 );
	}
	else

/* These directly adjust levels at the RS-232 socket - not emulated
	d7,d6 Not used
	d5 Secondary Unassigned, pin 18 
	d4 Secondary Transmit Data, pin 14 
	d3 Secondary Request-to-Send, pin 19 
	d2 Break ('0'=disable transmit data; continuous RS232 'SPACE' condition) 
	d1 Data-Terminal-Ready (DTR), pin 20 
	d0 Request-to-Send (RTS), pin 4 */

	{
	}
}

WRITE8_HANDLER( trs80m4_eb_w )
{
	ay31015_set_transmit_data( trs80_ay31015, data );
}

WRITE8_HANDLER( trs80m4_ec_w )
{
/* Hardware settings - not yet emulated
	d6 CPU fast (1=4MHz, 0=2MHz)
	d5 Enable Video Wait
	d4 Enable External I/O bus
	d3 Enable Alternate Character Set
	d2 Mode Select (0=64 chars, 1=32chars)
	d1 Cassette Motor (1=On) */

	cassette_change_state( trs80_cass, ( data & 2 ) ? CASSETTE_MOTOR_ENABLED : CASSETTE_MOTOR_DISABLED, CASSETTE_MASK_MOTOR );
}

WRITE8_HANDLER( sys80_f8_w )
{
/* These adjust levels at the socket pins - not emulated
	d2 reset UART (XR pin)
	d1 DTR
	d0 RTS */
}

WRITE8_HANDLER( trs80_ff_w )
{
	static const double levels[4] = { 0.0, -1.0, 0.0, 1.0 };

	cassette_change_state( trs80_cass, ( data & 4 ) ? CASSETTE_MOTOR_ENABLED : CASSETTE_MOTOR_DISABLED, CASSETTE_MASK_MOTOR );
	cassette_output( trs80_cass, levels[data & 3]);
	cassette_data &= ~0x80;
	trs80_port_ff = data;
}

WRITE8_HANDLER( trs80m4_ff_w )
{
	static const double levels[4] = { 0.0, -1.0, 0.0, 1.0 };
	cassette_output( trs80_cass, levels[data & 3]);
	cassette_data &= ~0x80;
}


/*************************************
 *
 *		Interrupt handlers.
 *
 *************************************/

INTERRUPT_GEN( trs80_timer_interrupt )
{
	if( (irq_status & IRQ_TIMER) == 0 )
	{
		irq_status |= IRQ_TIMER;
		cpu_set_input_line(device, 0, HOLD_LINE);
	}
}

INTERRUPT_GEN( trs80m4_rtc_interrupt )
{
//	if( (irq_status & IRQ_RTC) == 0 )
	if( (irq_status & 1) == 0 )
	{
		irq_status |= IRQ_RTC;
		irq_status |= 1;	// indicate irq in progress
		cpu_set_input_line(device, 0, HOLD_LINE);
	}
	else
	if (irq_status & 1)
	{
		irq_status &= 0xfe;
		cpu_set_input_line(device, 0, CLEAR_LINE);
	}
}

static void trs80_fdc_interrupt_internal(running_machine *machine)
{
	if ((irq_status & IRQ_FDC) == 0)
	{
		irq_status |= IRQ_FDC;
		cpu_set_input_line(machine->cpu[0], 0, HOLD_LINE);
	}
}

INTERRUPT_GEN( trs80_fdc_interrupt )
{
	trs80_fdc_interrupt_internal(device->machine);
}

static WD17XX_CALLBACK( trs80_fdc_callback )
{
	switch (state)
	{
		case WD17XX_IRQ_CLR:
			irq_status &= ~IRQ_FDC;
			break;
		case WD17XX_IRQ_SET:
			trs80_fdc_interrupt_internal(device->machine);
			break;
		case WD17XX_DRQ_CLR:
		case WD17XX_DRQ_SET:
			/* do nothing */
			break;
	}
}

const wd17xx_interface trs80_wd17xx_interface = { trs80_fdc_callback, NULL };


INTERRUPT_GEN( trs80_frame_interrupt )
{
}

/*************************************
 *				     *
 *		Memory handlers      *
 *				     *
 *************************************/

 READ8_HANDLER ( trs80_printer_r )
{
	/* Bit 7 - 1 = Busy; 0 = Not Busy
	   Bit 6 - 1 = Out of Paper; 0 = Paper
	   Bit 5 - 1 = Ready; 0 = Not Ready
	   Bit 4 - 1 = Printer selected; 0 = Printer not selected
	   Bits 3..0 - Not used */

	return 0x30 | (centronics_busy_r(trs80_printer) << 7);
}

WRITE8_HANDLER( trs80_printer_w )
{
	centronics_strobe_w(trs80_printer, 1);
	centronics_data_w(trs80_printer, 0, data);
	centronics_strobe_w(trs80_printer, 0);
}

 READ8_HANDLER( trs80_irq_status_r )
{
	int result = irq_status;
	irq_status &= ~(IRQ_TIMER | IRQ_FDC);
	return result;
}

WRITE8_HANDLER( trs80_irq_mask_w )
{
	irq_mask = data;
}

WRITE8_HANDLER( trs80_motor_w )
{
	UINT8 drive = 255;

	LOG(("trs80 motor_w $%02X\n", data));

	switch (data)
	{
	case 1:
		drive = 0;
		head = 0;
		break;
	case 2:
		drive = 1;
		head = 0;
		break;
	case 4:
		drive = 2;
		head = 0;
		break;
	case 8:
		drive = 3;
		head = 0;
		break;
	case 9:
		drive = 0;
		head = 1;
		break;
	case 10:
		drive = 1;
		head = 1;
		break;
	case 12:
		drive = 2;
		head = 1;
		break;
	}

	if (drive > 3)
		return;

	wd17xx_set_drive(trs80_fdc,drive);
	wd17xx_set_side(trs80_fdc,head);

}

/*************************************
 *		Keyboard					 *
 *************************************/
READ8_HANDLER( trs80_keyboard_r )
{
	int result = 0;

	if (offset & 1)
		result |= input_port_read(space->machine, "LINE0");
	if (offset & 2)
		result |= input_port_read(space->machine, "LINE1");
	if (offset & 4)
		result |= input_port_read(space->machine, "LINE2");
	if (offset & 8)
		result |= input_port_read(space->machine, "LINE3");
	if (offset & 16)
		result |= input_port_read(space->machine, "LINE4");
	if (offset & 32)
		result |= input_port_read(space->machine, "LINE5");
	if (offset & 64)
		result |= input_port_read(space->machine, "LINE6");
	if (offset & 128)
		result |= input_port_read(space->machine, "LINE7");

	return result;
}


