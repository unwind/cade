/*
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
