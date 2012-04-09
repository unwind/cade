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
*/

#if !defined CADE_H
#define	CADE_H

#include <stdint.h>
#include <stdlib.h>

typedef struct DCPU_State	DCPU_State;

void	DCPU_Init(DCPU_State *cpu);
void	DCPU_Load(DCPU_State *cpu, uint16_t address, const uint16_t *data, size_t length);

void	DCPU_PrintState(const DCPU_State *cpu);
void	DCPU_Dump(const DCPU_State *cpu, uint16_t start, size_t length);

void	DCPU_StepCycle(DCPU_State *cpu, size_t num_cycles);
size_t	DCPU_StepInstruction(DCPU_State *cpu);
size_t	DCPU_StepUntilStuck(DCPU_State *cpu);

#endif	/* CADE_H */
