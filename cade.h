/*
 * Yet another DCPU-16 emulator/virtual machine/whatever.
 *
 * The acronym is supposed to mean "cycle-accurate dcpu emulator", since that is the
 * main goal. Most of the implementations that have popped up the last few days seem
 * to ignore the cycle-accurate part, which is understandable. I've never written a
 * cycle-accurate CPU emulator though, so that's the itch here.
 *
 * Written by Emil Brink <emil@obsession.se>, created April 2012.
 *
 * Of course, also see the DCPU-16 spec at <http://0x10c.com/doc/dcpu-16.txt>.
 *
 * Licensed under the GNU Lesser General Public License, v3.
*/

#if !defined CADE_H
#define	CADE_H

#include <stdint.h>
#include <stdlib.h>

/* -------------------------------------------------------------------------- */

/** \file cade.h
 *
 * This file declares the public API for the Cade DCPU-16 emulator.
*/

/* -------------------------------------------------------------------------- */

/** \brief Enumeration for the DCPU-16 registers. */
typedef enum {
	DCPU_REG_A = 0,
	DCPU_REG_B,
	DCPU_REG_C,
	DCPU_REG_X,
	DCPU_REG_Y,
	DCPU_REG_Z,
	DCPU_REG_I,
	DCPU_REG_J,
	DCPU_REG_COUNT
} DCPU_Register;

typedef struct DCPU_State	DCPU_State;

/** This is <tt>SUB PC, 1</tt>, which is a 1-instruction infinite loop
 * that doesn't depend on the address it's assembled at.
*/
#define	DCPU_STOP	(0x21 << 10 | (0x1c << 4) | 3)

/* -------------------------------------------------------------------------- */

const char *	DCPU_GetRegisterName(DCPU_Register);

DCPU_State *	DCPU_Create(void);
void		DCPU_Destroy(DCPU_State *cpu);

void		DCPU_Init(DCPU_State *cpu);
void		DCPU_Load(DCPU_State *cpu, uint16_t address, const uint16_t *data, size_t length);

uint16_t	DCPU_GetRegister(const DCPU_State *cpu, DCPU_Register reg);
uint16_t	DCPU_GetPC(const DCPU_State *cpu);
uint16_t	DCPU_GetSP(const DCPU_State *cpu);
uint16_t	DCPU_GetO(const DCPU_State *cpu);
uint16_t	DCPU_GetMemory(const DCPU_State *cpu, uint16_t address);

void		DCPU_PrintState(const DCPU_State *cpu);
void		DCPU_Dump(const DCPU_State *cpu, uint16_t start, size_t length);

void		DCPU_StepCycle(DCPU_State *cpu, size_t num_cycles);
size_t		DCPU_StepInstruction(DCPU_State *cpu);
size_t		DCPU_StepUntilStuck(DCPU_State *cpu);

#endif	/* CADE_H */
