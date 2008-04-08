/*
    experimental LISA driver

    Raphael Nabet, 2000
*/

#include "driver.h"
#include "includes/lisa.h"
#include "devices/sonydriv.h"
/*#include "machine/6522via.h"*/


static ADDRESS_MAP_START(lisa_map, ADDRESS_SPACE_PROGRAM, 16)
	AM_RANGE(0x000000, 0xffffff) AM_READWRITE(lisa_r, lisa_w)			/* no fixed map, we use an MMU */
ADDRESS_MAP_END

static ADDRESS_MAP_START(lisa_fdc_map, ADDRESS_SPACE_PROGRAM, 8)
	AM_RANGE(0x0000, 0x03ff) AM_RAM	AM_BASE(&lisa_fdc_ram)				/* RAM (shared with 68000) */
	AM_RANGE(0x0400, 0x07ff) AM_READWRITE(lisa_fdc_io_r, lisa_fdc_io_w)	/* disk controller (IWM and TTL logic) */
	AM_RANGE(0x0800, 0x0fff) AM_NOP
	AM_RANGE(0x1000, 0x1fff) AM_ROM	AM_BASE(&lisa_fdc_rom)				/* ROM */
	AM_RANGE(0x2000, 0xffff) AM_READWRITE(lisa_fdc_r, lisa_fdc_w)		/* handler for wrap-around */
ADDRESS_MAP_END

static ADDRESS_MAP_START(lisa210_fdc_map, ADDRESS_SPACE_PROGRAM, 8)
	AM_RANGE(0x0000, 0x03ff) AM_RAM	AM_BASE(&lisa_fdc_ram)				/* RAM (shared with 68000) */
	AM_RANGE(0x0400, 0x07ff) AM_NOP										/* nothing, or RAM wrap-around ??? */
	AM_RANGE(0x0800, 0x0bff) AM_READWRITE(lisa_fdc_io_r, lisa_fdc_io_w)	/* disk controller (IWM and TTL logic) */
	AM_RANGE(0x0c00, 0x0fff) AM_NOP										/* nothing, or IO port wrap-around ??? */
	AM_RANGE(0x1000, 0x1fff) AM_ROM	AM_BASE(&lisa_fdc_rom)				/* ROM */
	AM_RANGE(0x2000, 0xffff) AM_READWRITE(lisa_fdc_r, lisa_fdc_w)		/* handler for wrap-around */
ADDRESS_MAP_END


/* Lisa1 and Lisa 2 machine */
static MACHINE_DRIVER_START( lisa )
	/* basic machine hardware */
	MDRV_CPU_ADD_TAG("main", M68000, 5093760)        /* 20.37504 Mhz / 4 */
	MDRV_CPU_PROGRAM_MAP(lisa_map, 0)
	MDRV_CPU_VBLANK_INT("main", lisa_interrupt)

	MDRV_CPU_ADD_TAG("fdc", M6502, 2000000)        /* 16.000 Mhz / 8 in when DIS asserted, 16.000 Mhz / 9 otherwise (?) */
	MDRV_CPU_PROGRAM_MAP(lisa_fdc_map, 0)

	MDRV_INTERLEAVE(1)
	MDRV_MACHINE_RESET( lisa )

	MDRV_VIDEO_ATTRIBUTES(VIDEO_UPDATE_BEFORE_VBLANK)
	MDRV_SCREEN_ADD("main", RASTER)
	MDRV_SCREEN_REFRESH_RATE(60)
	MDRV_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(2500)) /* not accurate */
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MDRV_SCREEN_SIZE(880, 380)
	MDRV_SCREEN_VISIBLE_AREA(0, 720-1, 0, 364-1)
	MDRV_PALETTE_LENGTH(2)
	MDRV_PALETTE_INIT(black_and_white)

	MDRV_VIDEO_START(lisa)
	MDRV_VIDEO_UPDATE(lisa)

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_MONO("mono")
	MDRV_SOUND_ADD(SPEAKER, 0)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 1.00)

	MDRV_NVRAM_HANDLER(lisa)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( lisa210 )
	MDRV_IMPORT_FROM( lisa )
	MDRV_CPU_MODIFY( "fdc" )
	MDRV_CPU_PROGRAM_MAP(lisa210_fdc_map, 0)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( macxl )
	MDRV_IMPORT_FROM( lisa210 )
	MDRV_SCREEN_MODIFY("main")
	MDRV_SCREEN_SIZE(	768/* ???? */, 447/* ???? */)
	MDRV_SCREEN_VISIBLE_AREA(0, 608-1, 0, 431-1)
MACHINE_DRIVER_END


static INPUT_PORTS_START( lisa )
	PORT_START /* Mouse - X AXIS */
	PORT_BIT( 0xff, 0x00, IPT_TRACKBALL_X) PORT_SENSITIVITY(100) PORT_KEYDELTA(0) PORT_PLAYER(1)

	PORT_START /* Mouse - Y AXIS */
	PORT_BIT( 0xff, 0x00, IPT_TRACKBALL_Y) PORT_SENSITIVITY(100) PORT_KEYDELTA(0) PORT_PLAYER(1)

	/* pseudo-input ports with (unverified) keyboard layout */

	PORT_START	/* 2 */
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_UNUSED)

	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_BUTTON1) PORT_NAME("mouse button")

	PORT_START	/* 3 */
	PORT_BIT(0xFFFF, IP_ACTIVE_HIGH, IPT_UNUSED)

	PORT_START	/* 4 */
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Clear") PORT_CODE(KEYCODE_DEL)
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("- (KP)") PORT_CODE(KEYCODE_NUMLOCK)
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("+ (KP)") PORT_CODE(KEYCODE_SLASH_PAD)
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("* (KP)") PORT_CODE(KEYCODE_ASTERISK)
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("7 (KP)") PORT_CODE(KEYCODE_7_PAD)
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("8 (KP)") PORT_CODE(KEYCODE_8_PAD)
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("9 (KP)") PORT_CODE(KEYCODE_9_PAD)
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("/ (KP)") PORT_CODE(KEYCODE_MINUS_PAD)
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("4 (KP)") PORT_CODE(KEYCODE_4_PAD)
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("5 (KP)") PORT_CODE(KEYCODE_5_PAD)
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("6 (KP)") PORT_CODE(KEYCODE_6_PAD)
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(", (KP)") PORT_CODE(KEYCODE_PLUS_PAD)
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(". (KP)") PORT_CODE(KEYCODE_DEL_PAD)
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("2 (KP)") PORT_CODE(KEYCODE_2_PAD)
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("3 (KP)") PORT_CODE(KEYCODE_3_PAD)
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Enter (KP)") PORT_CODE(KEYCODE_ENTER_PAD)

	PORT_START	/* 5 */
	PORT_BIT(0xFFFF, IP_ACTIVE_HIGH, IPT_UNUSED)

	PORT_START	/* 6 */
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("-") PORT_CODE(KEYCODE_MINUS)
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("=") PORT_CODE(KEYCODE_EQUALS)
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("\\") PORT_CODE(KEYCODE_BACKSLASH)
#if 1
	/* US layout */
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_UNUSED)
#else
	/* European layout */
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_UNUSED) PORT_NAME("<") PORT_CODE(KEYCODE_BACKSLASH2)
#endif
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("P") PORT_CODE(KEYCODE_P)
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Backspace") PORT_CODE(KEYCODE_BACKSPACE)
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Enter") PORT_CODE(KEYCODE_MENU)
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Return") PORT_CODE(KEYCODE_ENTER)
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("0 (KP)") PORT_CODE(KEYCODE_0_PAD)
	PORT_BIT(0x0C00, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("/") PORT_CODE(KEYCODE_SLASH)
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("1 (KP)") PORT_CODE(KEYCODE_1_PAD)
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Right Option") PORT_CODE(KEYCODE_RALT)
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_UNUSED)

	PORT_START	/* 7 */
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("9") PORT_CODE(KEYCODE_9)
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("0") PORT_CODE(KEYCODE_0)
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("U") PORT_CODE(KEYCODE_U)
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("I") PORT_CODE(KEYCODE_I)
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("J") PORT_CODE(KEYCODE_J)
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("K") PORT_CODE(KEYCODE_K)
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("[") PORT_CODE(KEYCODE_OPENBRACE)
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("]") PORT_CODE(KEYCODE_CLOSEBRACE)
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("M") PORT_CODE(KEYCODE_M)
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("L") PORT_CODE(KEYCODE_L)
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(";") PORT_CODE(KEYCODE_COLON)
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("'") PORT_CODE(KEYCODE_QUOTE)
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Space") PORT_CODE(KEYCODE_SPACE)
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(",") PORT_CODE(KEYCODE_COMMA)
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(".") PORT_CODE(KEYCODE_STOP)
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("O") PORT_CODE(KEYCODE_O)

	PORT_START	/* 8 */
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("E") PORT_CODE(KEYCODE_E)
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("6") PORT_CODE(KEYCODE_6)
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("7") PORT_CODE(KEYCODE_7)
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("8") PORT_CODE(KEYCODE_8)
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("5") PORT_CODE(KEYCODE_5)
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("R") PORT_CODE(KEYCODE_R)
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("T") PORT_CODE(KEYCODE_T)
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Y") PORT_CODE(KEYCODE_Y)
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("`") PORT_CODE(KEYCODE_TILDE)
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("F") PORT_CODE(KEYCODE_F)
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("G") PORT_CODE(KEYCODE_G)
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("H") PORT_CODE(KEYCODE_H)
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("V") PORT_CODE(KEYCODE_V)
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("C") PORT_CODE(KEYCODE_C)
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("B") PORT_CODE(KEYCODE_B)
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("N") PORT_CODE(KEYCODE_N)

	PORT_START	/* 9 */
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("A") PORT_CODE(KEYCODE_A)
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("2") PORT_CODE(KEYCODE_2)
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("3") PORT_CODE(KEYCODE_3)
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("4") PORT_CODE(KEYCODE_4)
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("1") PORT_CODE(KEYCODE_1)
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Q") PORT_CODE(KEYCODE_Q)
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("S") PORT_CODE(KEYCODE_S)
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("W") PORT_CODE(KEYCODE_W)
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Tab") PORT_CODE(KEYCODE_TAB)
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Z") PORT_CODE(KEYCODE_Z)
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("X") PORT_CODE(KEYCODE_X)
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("D") PORT_CODE(KEYCODE_D)
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Left Option") PORT_CODE(KEYCODE_LALT)
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Alpha Lock") PORT_CODE(KEYCODE_CAPSLOCK) PORT_TOGGLE
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Shift") PORT_CODE(KEYCODE_LSHIFT)
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Command") PORT_CODE(KEYCODE_LCONTROL)

INPUT_PORTS_END


ROM_START( lisa2 )
	ROM_REGION16_BE(0x204000,REGION_CPU1,0)	/* 68k rom and ram */
	ROM_LOAD16_BYTE( "booth.hi", 0x000000, 0x2000, CRC(adfd4516) SHA1(97a89ce1218b8aa38f69f92f6f363f435c887914))
	ROM_LOAD16_BYTE( "booth.lo", 0x000001, 0x2000, CRC(546d6603) SHA1(2a81e4d483f50ae8a2519621daeb7feb440a3e4d))

	ROM_REGION(0x2000,REGION_CPU2,0)		/* 6504 RAM and ROM */
	ROM_LOAD( "ioa8.rom", 0x1000, 0x1000, CRC(bc6364f1) SHA1(f3164923330a51366a06d9d8a4a01ec7b0d3a8aa))

	ROM_REGION(0x100,REGION_GFX1,0)		/* video ROM (includes S/N) */
	ROM_LOAD( "vidstate.rom", 0x00, 0x100, CRC(75904783) SHA1(3b0023bd90f2ca1be0b099160a566b044856885d))
ROM_END

ROM_START( lisa210 )
	ROM_REGION16_BE(0x204000,REGION_CPU1, 0)	/* 68k rom and ram */
	ROM_LOAD16_BYTE( "booth.hi", 0x000000, 0x2000, CRC(adfd4516) SHA1(97a89ce1218b8aa38f69f92f6f363f435c887914))
	ROM_LOAD16_BYTE( "booth.lo", 0x000001, 0x2000, CRC(546d6603) SHA1(2a81e4d483f50ae8a2519621daeb7feb440a3e4d))

#if 1
	ROM_REGION(0x2000,REGION_CPU2, 0)		/* 6504 RAM and ROM */
	ROM_LOAD( "io88.rom", 0x1000, 0x1000, CRC(e343fe74) SHA1(a0e484ead2d2315fca261f39fff2f211ff61b0ef))
#else
	ROM_REGION(0x2000,REGION_CPU2, 0)		/* 6504 RAM and ROM */
	ROM_LOAD( "io88800k.rom", 0x1000, 0x1000, CRC(8c67959a))
#endif

	ROM_REGION(0x100,REGION_GFX1, 0)		/* video ROM (includes S/N) */
	ROM_LOAD( "vidstate.rom", 0x00, 0x100, CRC(75904783) SHA1(3b0023bd90f2ca1be0b099160a566b044856885d))
ROM_END

ROM_START( macxl )
	ROM_REGION16_BE(0x204000,REGION_CPU1, 0)	/* 68k rom and ram */
	ROM_LOAD16_BYTE( "boot3a.hi", 0x000000, 0x2000, CRC(80add605) SHA1(82215688b778d8c712a8186235f7981e3dc4dd7f))
	ROM_LOAD16_BYTE( "boot3a.lo", 0x000001, 0x2000, CRC(edf5222f) SHA1(b0388ee8dbbc51a2d628473dc29b65ce913fcd76))

#if 1
	ROM_REGION(0x2000,REGION_CPU2, 0)		/* 6504 RAM and ROM */
	ROM_LOAD( "io88.rom", 0x1000, 0x1000, CRC(e343fe74) SHA1(a0e484ead2d2315fca261f39fff2f211ff61b0ef))
#else
	ROM_REGION(0x2000,REGION_CPU2, 0)		/* 6504 RAM and ROM */
	ROM_LOAD( "io88800k.rom", 0x1000, 0x1000, CRC(8c67959a))
#endif

	ROM_REGION(0x100,REGION_GFX1, 0)		/* video ROM (includes S/N) ; no dump known, although Lisa ROM works fine at our level of emulation */
	ROM_LOAD( "vidstatem.rom", 0x00, 0x100, NO_DUMP)
ROM_END


static void lisa_floppy_getinfo(const mess_device_class *devclass, UINT32 state, union devinfo *info)
{
	/* floppy */
	switch(state)
	{
		/* --- the following bits of info are returned as 64-bit signed integers --- */
		case MESS_DEVINFO_INT_SONYDRIV_ALLOWABLE_SIZES:		info->i = SONY_FLOPPY_ALLOW400K | SONY_FLOPPY_ALLOW800K; break;

		default:										sonydriv_device_getinfo(devclass, state, info); break;
	}
}

SYSTEM_CONFIG_START(lisa)
	/* Lisa should eventually support floppies, hard disks, etc. */
	CONFIG_DEVICE(lisa_floppy_getinfo)
SYSTEM_CONFIG_END

SYSTEM_CONFIG_START(lisa210)
	CONFIG_IMPORT_FROM(lisa)
	/* actually, there is an additionnal 10 meg HD, but it is not implemented... */
SYSTEM_CONFIG_END

/*
    Lisa drivers boot MacWorks, but do not boot the Lisa OS, which is why we set
    the GAME_NOT_WORKING flag...
*/
/*     YEAR  NAME      PARENT   COMPAT  MACHINE   INPUT  INIT      CONFIG   COMPANY  FULLNAME */
COMP( 1984, lisa2,    0,		0,		lisa,     lisa,	 lisa2,    lisa,	"Apple Computer",  "Lisa2", GAME_NOT_WORKING )
COMP( 1984, lisa210,  lisa2,	0,		lisa210,  lisa,	 lisa210,  lisa210,	"Apple Computer",  "Lisa2/10", GAME_NOT_WORKING )
COMP( 1985, macxl,    lisa2,	0,		macxl,    lisa,	 mac_xl,   lisa210,	"Apple Computer",  "Macintosh XL", /*GAME_NOT_WORKING*/0 )
