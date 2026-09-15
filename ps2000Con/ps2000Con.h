#ifndef PS2000CON_H
#define PS2000CON_H

#include "ps2000.h"

#ifdef __cplusplus
extern "C" {
#endif

	/* CONFIGURACION */
	int abrir_picoscope(void);
	void cerrar_picoscope(void);
	void menu_picoscope(void);

	void set_timebase(void);
	void set_voltages(void);
	void set_defaults(void);

	extern volatile int captura_activa;


	/* ADQUISICION */

	void collect_block_immediate(void);
	void collect_block_triggered(void);
	void collect_streaming(void);
	void collect_fast_streaming(void);
	void collect_fast_streaming_triggered(void);
	void collect_fast_streaming_triggered2(void);


	/* OTRAS FUNCIONES */

	void set_sig_gen(void);
	void set_sig_gen_arb(void);
	void collect_block_ets(void);

#ifdef __cplusplus
}
#endif

#endif