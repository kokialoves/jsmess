/*****************************************************************************
 *
 * includes/kim1.h
 *
 ****************************************************************************/

#ifndef KIM1_H_
#define KIM1_H_


/*----------- defined in machine/kim1.c -----------*/

DRIVER_INIT( kim1 );
MACHINE_RESET( kim1 );

void kim1_cassette_getinfo(const mess_device_class *devclass, UINT32 state, union devinfo *info);

INTERRUPT_GEN( kim1_interrupt );

/*----------- defined in video/kim1.c -----------*/

PALETTE_INIT( kim1 );
VIDEO_START( kim1 );
VIDEO_UPDATE( kim1 );


#endif /* KIM1_H_ */
