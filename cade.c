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

#include <stdio.h>
#include <string.h>

#include "cade.h"

/* -------------------------------------------------------------------------- */

/** \file cade.c
 *
 * \mainpage Cade, a cycle-accurate DCPU-16 emulator by Emil Brink.
 *
 * See the public API documentation in cade.h.
*/

/** \brief The basic opcodes for the processor.
 *
 * Basic opcodes always have two values \c a and \c b, and typically change \c a.
*/
typedef enum {
	OP_NOBASIC = 0,
	OP_SET,
	OP_ADD,
	OP_SUB,
	OP_MUL,
	OP_DIV,
	OP_MOD,
	OP_SHL,
	OP_SHR,
	OP_AND,
	OP_BOR,
	OP_XOR,
	OP_IFE,
	OP_IFN,
	OP_IFG,
	OP_IFB
} DCPU_BasicOp;

/** \brief The extended opcodes for the processor.
 * 
 * Extended opcodes have only one value, as opposed to two (\c a and \c b) for the basic ones.
*/
typedef enum {
	XOP_JSR = 1,
} DCPU_ExtendedOp;

/** \brief Enumeration of the various value types, this is what is generally
 * encoded in instructions where values are needed. The literal range is excluded.
*/
typedef enum {
	VAL_REG_A = 0,
	VAL_REG_B,
	VAL_REG_C,
	VAL_REG_X,
	VAL_REG_Y,
	VAL_REG_Z,
	VAL_REG_I,
	VAL_REG_J,
	VAL_DEREF_REG_A,
	VAL_DEREF_REG_B,
	VAL_DEREF_REG_C,
	VAL_DEREF_REG_X,
	VAL_DEREF_REG_Y,
	VAL_DEREF_REG_Z,
	VAL_DEREF_REG_I,
	VAL_DEREF_REG_J,
	VAL_SUCC_REG_A,
	VAL_SUCC_REG_B,
	VAL_SUCC_REG_C,
	VAL_SUCC_REG_X,
	VAL_SUCC_REG_Y,
	VAL_SUCC_REG_Z,
	VAL_SUCC_REG_I,
	VAL_SUCC_REG_J,
	VAL_POP,
	VAL_PEEK,
	VAL_PUSH,
	VAL_SP,
	VAL_PC,
	VAL_O,
	VAL_SUCC,
	VAL_SUCC_LIT,
} DCPU_Value;

typedef struct Thunk	Thunk;

/** \brief A wrapped function pointer.
 *
 * Instances of this struct are used to represent what the emulator
 * should on the next clock cycle. It's basically a work-around for
 * the problem of defining a pointer to a function that returns
 * a value of its own type.
*/
struct Thunk
{
	Thunk	(*execute)(DCPU_State *cpu);		/**! Execute a single emulated cycle. */
};

/** \brief The size of the emulated DCPU-16's memory. */
#define	MEM_SIZE	0x10000

/** \brief Internal representation of the state of the emulated DCPU-16.
 *
 * This structure is not public, use the API to access the state of
 * the emulated DCPU-16.
*/
struct DCPU_State {
	uint16_t	registers[DCPU_REG_COUNT];	/**< The registers, indexed by DCPU_Register. */
	uint16_t	sp;				/**< The stack pointer. */
	uint16_t	pc;				/**< The program counter. */
	uint16_t	o;				/**< The overflow register. */
	uint16_t	memory[MEM_SIZE];		/**< The machine's memory. */

	Thunk		cycle;				/**< Function to execute for next clock cycle. */
	uint16_t	inst;				/**< Currently-executing instruction. */
	uint16_t	*val_a, *val_b;			/**< Pointers at resolved values from current instruction, or NULL. */
	uint16_t	dummy;				/**< Target for invalid value (SET of literal). */
	uint32_t	timer;				/**< Instruction-counter. */
	unsigned char	skip;				/**< Signals that the next instruction is to be skipped due to IFx. */
};

/* -------------------------------------------------------------------------- */

/** \brief Returns a string containing the name of the indicated register.
 *
 * @return A constant string (owned by Cade, should not be deallocated by caller).
*/
const char * DCPU_GetRegisterName(DCPU_Register reg)
{
	const char	*names = "A\0B\0C\0X\0Y\0Z\0I\0J\0";

	return names + 2 * reg;
}

/* -------------------------------------------------------------------------- */

static unsigned int DCPU_ValueLength(unsigned char value)
{
	if(value < VAL_SUCC)
		return 0;
	if(value <= VAL_SUCC_LIT)
		return 1;
	return 0;
}

/** \brief Returns length of the given instruction, in words. */
unsigned int DCPU_InstructionLength(uint16_t inst)
{
	if(inst & 0xf)
		return 1 + DCPU_ValueLength((inst >> 4) & 0x3f) + DCPU_ValueLength((inst >> 10) & 0x3f);
	else
	{
		fprintf(stderr, "**Can't compute length of non-basic instruction 0x%04x\n", inst);
	}
	return 1;
}

/* Evaluates the given value. Returns how many cycles where spent, i.e. 0 or 1. */
static int eval_value(DCPU_State *cpu, DCPU_Value value, int dest, uint16_t **value_result)
{
	static uint16_t	literals[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
				       17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31 };

	printf("evaluating value %c (%x)\n", "ba"[dest], value);
	if(value >= VAL_REG_A && value <= VAL_REG_J)
	{
		/* Register value, evaluates immediately. */
		*value_result = &cpu->registers[value];
		printf(" register %c (current value 0x%04x)\n", "ABCXYZIJ"[value - VAL_REG_A], **value_result);
	}
	else if(value >= VAL_DEREF_REG_A && value <= VAL_DEREF_REG_J)
	{
		*value_result = &cpu->memory[cpu->memory[cpu->registers[value - VAL_DEREF_REG_A]]];
		printf(" register indirect, address 0x%04x\n", (unsigned short) (*value_result - cpu->memory));
	}
	else if(value >= VAL_SUCC_REG_A && value <= VAL_SUCC_REG_J)
	{
		const uint16_t	succ = cpu->memory[cpu->pc++];

		*value_result = &cpu->memory[succ + cpu->registers[value - VAL_SUCC_REG_A]];
		printf(" indexing, address 0x%04x\n", (unsigned short) (*value_result - cpu->memory));
		return 1;
	}
	else if(value == VAL_POP)
	{
		*value_result = &cpu->memory[cpu->sp++];
		printf(" POP, value 0x%04x\n", **value_result);
	}
	else if(value == VAL_PEEK)
	{
		*value_result = &cpu->memory[cpu->sp];
		printf(" PEEK, value 0x%04x\n", **value_result);
	}
	else if(value == VAL_PUSH)
	{
		*value_result = &cpu->memory[--cpu->sp];
		printf(" PUSH, value 0x%04x\n", **value_result);
	}
	else if(value == VAL_SP)
	{
		*value_result = &cpu->sp;
		printf(" SP, value 0x%04x\n", **value_result);
	}
	else if(value == VAL_PC)
	{
		*value_result = &cpu->pc;
		printf(" PC target\n");
	}
	else if(value == VAL_O)
	{
		*value_result = &cpu->o;
		printf(" O, value 0x%04x\n", **value_result);
	}
	else if(value == VAL_SUCC)
	{
		*value_result = &cpu->memory[cpu->memory[cpu->pc++]];
		printf(" memory target, address 0x%04x\n", (unsigned short) (*value_result - cpu->memory));
		return 1;
	}
	else if(value == VAL_SUCC_LIT)
	{
		*value_result = &cpu->memory[cpu->pc++];
		printf(" literal, value 0x%04x\n", **value_result);
		return 1;
	}
	else if(value >= 0x20 && value <= 0x3f)
	{
		*value_result = dest ? &cpu->dummy : literals + (value - 0x20);
		printf(" small literal, value 0x%02x\n", **value_result);
	}
	else
		fprintf(stderr, "**Unhandled value type 0x%x\n", value);

	return 0;
}

static Thunk cycle_fetch(DCPU_State *cpu);

static Thunk get_cycle_refetch(DCPU_State *cpu)
{
	const Thunk	next = { cycle_fetch };

	cpu->inst = 0;
	cpu->val_a = NULL;
	cpu->val_b = NULL;
	printf("ending cycle %u\n", cpu->timer++);

	return next;
}

static Thunk cycle_add(DCPU_State *cpu)
{
	const uint32_t	tmp = *cpu->val_a + *cpu->val_b;

	*cpu->val_a = tmp & 0xffff;
	cpu->o = (tmp > 0xffff);
	printf("in ADD, ending cycle %u\n", cpu->timer++);

	return get_cycle_refetch(cpu);
}

static Thunk cycle_sub(DCPU_State *cpu)
{
	const uint32_t	tmp = *cpu->val_a - *cpu->val_b;

	*cpu->val_a = tmp & 0xffff;
	cpu->o = (tmp > 0xffff) ? 0xffff : 0;
	printf("in SUB, ending cycle %u\n", cpu->timer++);

	return get_cycle_refetch(cpu);
}

static Thunk cycle_mul(DCPU_State *cpu)
{
	const uint32_t	tmp = *cpu->val_a * *cpu->val_b;

	*cpu->val_a = tmp & 0xffff;
	cpu->o = (tmp >> 16) & 0xffff;

	return get_cycle_refetch(cpu);
}

static Thunk cycle_divmod2(DCPU_State *cpu)
{
	printf("in middle cycle of DIV/MOD, ending cycle %u\n", cpu->timer++);

	return get_cycle_refetch(cpu);
}

static Thunk cycle_div1(DCPU_State *cpu)
{
	const Thunk	next_div2 = { cycle_divmod2 };

	if(*cpu->val_b != 0)
	{
		const uint32_t	tmp = (*cpu->val_a << 16) / *cpu->val_b;

		*cpu->val_a = *cpu->val_a / *cpu->val_b;
		cpu->o = tmp >> 16;
	}
	else
	{
		*cpu->val_a = cpu->o = 0;
	}
	printf("in first cycle of DIV, ending cycle %u\n", cpu->timer++);

	return next_div2;
}

static Thunk cycle_mod1(DCPU_State *cpu)
{
	const Thunk	next_mod2 = { cycle_divmod2 };

	if(*cpu->val_b != 0)
	{
		*cpu->val_a = *cpu->val_a % *cpu->val_b;
	}
	else
		*cpu->val_a = 0;

	printf("in first cycle of MOD, ending cycle %u\n", cpu->timer++);

	return next_mod2;
}

static Thunk cycle_shl(DCPU_State *cpu)
{
	const uint32_t	res = *cpu->val_a << *cpu->val_b;

	*cpu->val_a = (res & 0xffff);
	cpu->o = res >> 16;

	return get_cycle_refetch(cpu);
}

static Thunk cycle_shr(DCPU_State *cpu)
{
	const uint32_t	res = *cpu->val_a >> *cpu->val_b;

	cpu->o = (*cpu->val_a << 16) >> *cpu->val_b;
	*cpu->val_a = (res & 0xffff);

	return get_cycle_refetch(cpu);
}

static Thunk cycle_if(DCPU_State *cpu)
{
	printf(" in IF, burning a cycle\n");
	printf("ending cycle %u\n", cpu->timer++);

	return get_cycle_refetch(cpu);
}

static Thunk cycle_skip(DCPU_State *cpu)
{
	const uint16_t	inst = cpu->memory[cpu->pc];

	printf("skip of instruction 0x%04x\n", inst);
	cpu->pc += DCPU_InstructionLength(inst);
	cpu->skip = 0;

	return get_cycle_refetch(cpu);
}

static Thunk cycle_jsr(DCPU_State *cpu)
{
	printf(" in JSR\n");
	cpu->memory[--cpu->sp] = cpu->pc;
	cpu->pc = *cpu->val_a;
	printf("ending cycle %u\n", cpu->timer++);

	return get_cycle_refetch(cpu);
}

/* The base of all instruction cycles: fetch a new instruction, and decode it. */
static Thunk cycle_fetch(DCPU_State *cpu)
{
	const Thunk	next_add = { cycle_add }, next_sub = { cycle_sub}, next_mul = { cycle_mul };
	const Thunk	next_div = { cycle_div1 }, next_mod = { cycle_mod1 };
	const Thunk	next_shl = { cycle_shl };
	const Thunk	next_shr = { cycle_shr };
	const Thunk	next_if = { cycle_if };
	const Thunk	next_jsr = { cycle_jsr };

	if(cpu->inst == 0)
	{
		if(cpu->skip)
		{
			const Thunk	skip = { cycle_skip };

			printf(" SKIP\n");
			printf(" ending cycle %u\n", cpu->timer++);
			return skip;
		}
		cpu->inst = cpu->memory[cpu->pc++];
		printf("Cycle %u: fetched instruction 0x%04x from 0x%04x\n", cpu->timer, cpu->inst, cpu->pc - 1);
		if(cpu->skip)
		{
			cpu->skip = 0;
		}
	}

	/* First, evaluate arguments for those instructions that have them. */
	switch((DCPU_BasicOp) (cpu->inst & 0xf))
	{
	case OP_NOBASIC:
		if(cpu->val_a == NULL)
		{
			if(eval_value(cpu, (cpu->inst >> 10) & 0x3f, 0, &cpu->val_a))
			{
				printf(" ending cycle %u\n", cpu->timer++);
				return cpu->cycle;
			}
		}
		break;
	default:
		/* All basic instructions have the same arguments. */
		if(cpu->val_a == NULL)
		{
			if(eval_value(cpu, (cpu->inst >> 4) & 0x3f, 1, &cpu->val_a))
			{
				printf(" ending cycle %u\n", cpu->timer++);
				return cpu->cycle;
			}
		}
		if(cpu->val_b == NULL)
		{
			if(eval_value(cpu, (cpu->inst >> 10) & 0x3f, 0, &cpu->val_b))
			{
				printf(" ending cycle %u\n", cpu->timer++);
				return cpu->cycle;
			}
		}
		break;
	}

	switch((DCPU_BasicOp) (cpu->inst & 0xf))
	{
	case OP_NOBASIC:
		{
			printf("running extended op\n");
			switch((DCPU_ExtendedOp) ((cpu->inst >> 4) & 0x3f))
			{
			case XOP_JSR:
				return next_jsr;
				break;
			}
		}
		break;
	case OP_SET:
		printf("executing SET (a at %p, b at %p, dummy at %p)\n", cpu->val_a, cpu->val_b, &cpu->dummy);
		*cpu->val_a = *cpu->val_b;
		break;
	case OP_ADD:
		printf("executing ADD\n");
		return next_add;
	case OP_SUB:
		printf("executing SUB\n");
		return next_sub;
	case OP_MUL:
		printf("executing MUL\n");
		return next_mul;
	case OP_DIV:
		printf("executing DIV\n");
		return next_div;
	case OP_MOD:
		printf("executing MOD\n");
		return next_mod;
	case OP_SHL:
		printf("evaluating SHL\n");
		return next_shl;
	case OP_SHR:
		printf("evaluating SHR\n");
		return next_shr;
	case OP_AND:
		printf("evaluating AND\n");
		*cpu->val_a &= *cpu->val_b;
		break;
	case OP_BOR:
		printf("evaluating BOR\n");
		*cpu->val_a |= *cpu->val_b;
		break;
	case OP_XOR:
		printf("evaluating XOR\n");
		*cpu->val_a ^= *cpu->val_b;
		break;
	case OP_IFE:
		printf("evaluating IFE [%04x == 0x%04x]\n", *cpu->val_a, *cpu->val_b);
		cpu->skip = !(*cpu->val_a == *cpu->val_b);
		printf(" set skip to %u\n", cpu->skip);
		return next_if;
	case OP_IFN:
		printf("evaluating IFN [0x%04x != 0x%04x]\n", *cpu->val_a, *cpu->val_b);
		cpu->skip = !(*cpu->val_a != *cpu->val_b);
		printf(" set skip to %u\n", cpu->skip);
		return next_if;
	case OP_IFG:
		printf("evaluating IFG [0x%04x > 0x%04x]\n", *cpu->val_a, *cpu->val_b);
		cpu->skip = !(*cpu->val_a > *cpu->val_b);
		printf(" set skip to %u\n", cpu->skip);
		return next_if;
	case OP_IFB:
		printf("evaluating IFB [0x%04x > 0x%04x]\n", *cpu->val_a, *cpu->val_b);
		cpu->skip = !((*cpu->val_a & *cpu->val_b) != 0);
		printf(" set skip to %u\n", cpu->skip);
		return next_if;
	default:
		fprintf(stderr, "**No implementation for opcode 0x%x\n", cpu->inst & 0xf);
	}
	/* Done with the instruction, clear state. */
	cpu->inst = 0;
	cpu->val_a = cpu->val_b = NULL;
	printf(" ending cycle %u\n", cpu->timer++);

	return cpu->cycle;
}

/* -------------------------------------------------------------------------- */

/** \brief Creates a new DCPU-16 instance.
 *
 * Memory is dynamically allocated to hold the emulated CPU's state; use DCPU_Destroy() to free it.
*/
DCPU_State * DCPU_Create(void)
{
	DCPU_State	*cpu;

	if((cpu = malloc(sizeof *cpu)) != NULL)
	{
		DCPU_Init(cpu);
	}
	return cpu;
}

/** \brief Destroys a DCPU-16 instance. */
void DCPU_Destroy(DCPU_State *cpu)
{
	free(cpu);
}

/** \brief Initializes (resets) the state of an emulated DCPU-16 instance.
 *
 * All memory and registers (including PC and O) are cleared to 0x0000, and the
 * stack pointer is set to 0xffff. Any executing instruction is aborted, on the
 * next cycle executed the DCPU-16 will fetch a new instruction to execute.
*/
void DCPU_Init(DCPU_State *cpu)
{
	memset(cpu, 0, sizeof *cpu);
	cpu->sp = 0xffff;
	cpu->cycle.execute = cycle_fetch;
}

/** \brief Loads some data into the emulated DPCU-16's memory.
 *
 * \param address The address where the first word will be loaded.
 * \param data Pointer to data to load from.
 * \param length The number of words to load.
*/
void DCPU_Load(DCPU_State *cpu, uint16_t address, const uint16_t *data, size_t length)
{
	memcpy(cpu->memory + address, data, length * sizeof *data);
}

/** \brief Prints the state of the emulated DCPU-16 instance.
 *
 * The contents of all registers, including the program counter, stack pointer
 * and overflow register, are printed.
*/
void DCPU_PrintState(const DCPU_State *cpu)
{
	const char	*reg_names = "ABCXYZIJ";
	int		i;

	printf("PC     SP     O      ");
	for(i = 0; i < sizeof cpu->registers / sizeof *cpu->registers; i++)
		printf("%-6c ", reg_names[i]);
	printf("\n");
	printf("0x%04x 0x%04x 0x%04x ",
		cpu->pc, cpu->sp, cpu->o);
	for(i = 0; i < sizeof cpu->registers / sizeof *cpu->registers; i++)
		printf("0x%04x ", cpu->registers[i]);
	printf("\n");
}

/** \brief Very primitive memory-dumping.
 *
 * Prints contents of the emulated DCPU-16's memory in a fantastically simplistic format
 * with one word per line.
 *
 * \param start The start address.
 * \param length The number of words to print.
*/
void DCPU_Dump(const DCPU_State *cpu, uint16_t start, size_t length)
{
	for(; length > 0; --length)
	{
		printf("%04x: 0x%04x\n", start, cpu->memory[start]);
		start++;
	}
}

/* -------------------------------------------------------------------------- */

/** \brief Read out the current value of a CPU register.
 *
 * \return The indicated register's value.
*/
uint16_t DCPU_GetRegister(const DCPU_State *cpu, DCPU_Register reg)
{
	if(cpu != NULL && reg < DCPU_REG_COUNT)
		return cpu->registers[reg];
	return 0;
}

/** \brief Read out the current value of the program counter (\c PC).
 *
 * \return The program counter's value.
*/
uint16_t DCPU_GetPC(const DCPU_State *cpu)
{
	return cpu != NULL ? cpu->pc : 0;
}

/** \brief Read out the current value of the stack pointer (\c SP).
 *
 * \return The stack pointer's value.
*/
uint16_t DCPU_GetSP(const DCPU_State *cpu)
{
	return cpu != NULL ? cpu->sp : 0;
}

/** \brief Read out the current value of the overflow register (\c O).
 *
 * \return The O register's value.
*/
uint16_t DCPU_GetO(const DCPU_State *cpu)
{
	return cpu != NULL ? cpu->o : 0;
}

/** \brief Read out the current contents of a word of memory.
 *
 * \param address The desired address to read out.
 *
 * \return The indicated word's contents.
*/
uint16_t DCPU_GetMemory(const DCPU_State *cpu, uint16_t address)
{
	return cpu != NULL ? cpu->memory[address] : 0;
}

/* -------------------------------------------------------------------------- */

/** \brief Execute a fixed number of instructions.
 *
 * This function runs the emulated DCPU-16 for a given number of instruction cycles.
 *
 * Note that this might leave the processor "mid-instruction".
 *
 * \param num_cycles The number of clock cycles to run the processor for.
*/
void DCPU_StepCycles(DCPU_State *cpu, size_t num_cycles)
{
	for(; num_cycles != 0; --num_cycles)
		cpu->cycle = cpu->cycle.execute(cpu);
}

/** \brief Execute a single whole instruction.
 *
 * This runs the emulated DCPU-16 until one instruction has been completed.
 * If the CPU is already in the middle of executing an instruction when this
 * is called, it will finish that instruction but not start a new.
 *
 * \return The number of clock cycles spent.
*/
size_t DCPU_StepInstruction(DCPU_State *cpu)
{
	size_t	num_cycles = 0;

	do {
		cpu->cycle = cpu->cycle.execute(cpu);
		++num_cycles;
	} while(cpu->inst != 0 || cpu->skip != 0);

	return num_cycles;
}

/** \brief Execute until the CPU seems "stuck".
 *
 * Runs the emulated DCPU-16 until it appears "stuck". The DCPU-16 is considered
 * to be stuck if executing a full instruction leaves the program counter (\c PC)
 * register unchanged. This means the current instruction is a 1-instruction
 * infinite loop, and since the DCPU doesn't have interrupts there's no way for it
 * to ever exit the loop.
 *
 * It's of course possible to come up with any number of n-instruction infinite
 * loops that will \em not be detected by this function, so beware.
 * 
 * Note that it's possible for this to never return, since there is no guarantee
 * that the DCPU-16 will end up in a stuck state as defined by the above.
 *
 * \return The number of clock cycles spent until an infinite loop was detected.
*/
size_t DCPU_StepUntilStuck(DCPU_State *cpu)
{
	size_t	num_cycles = 0;
	int	stuck;

	do {
		const uint16_t	old_pc = cpu->pc;
		num_cycles += DCPU_StepInstruction(cpu);
		stuck = cpu->pc == old_pc;
	} while(!stuck);

	return num_cycles;
}

/* -------------------------------------------------------------------------- */

#if defined CADE_STANDALONE

int main(void)
{
	DCPU_State	cpu;
	const uint16_t	test[] = { 0x7c01, 0x0030, 0x7de1, 0x1000, 0x0020, 0x7803, 0x1000, 0xc00d,
				0x7dc1, 0x001a, 0xa861, 0x7c01, 0x2000, 0x2161, 0x2000, 0x8463,
				0x806d, 0x7dc1, 0x000d, 0x9031, 0x7c10, 0x0018, 0x7dc1, 0x001a,
				0x9037, 0x61c1, 0x7dc1, 0x001a, 0x0000, 0x0000, 0x0000, 0x0000 };
	const uint16_t	test_and[] = { 0x7c01, 0xffff, 0x7c11, 0x5555, 0x0409 };
	const uint16_t	test_div[] = { 0x7c01, 0xffff, 0x7c11, 0x0471, 0x0405 };
	const uint16_t	test_mod[] = { 0x7c01, 0xffff, 0x7c11, 0x0471, 0x0406 };
	const uint16_t	test_set00[] = { 0x8201 };
	const uint16_t	test_stop[] = { (0x21 << 10 | (0x1c << 4) | 3) };
	size_t		count;

	DCPU_Init(&cpu);

/*	DCPU_Load(&cpu, 0, test, sizeof test / sizeof *test);
	DCPU_Load(&cpu, 0, test_and, sizeof test_and / sizeof *test_and);
	DCPU_Load(&cpu, 0, test_set00, sizeof test_set00 / sizeof *test_set00);
*/
	DCPU_Load(&cpu, 0, test_stop, sizeof test_stop / sizeof *test_stop);

	count = DCPU_StepUntilStuck(&cpu);

	DCPU_PrintState(&cpu);

	printf("Ran %zu cycles before becoming stuck.\n", count);

	return EXIT_SUCCESS;
}

#endif	/* CADE_STANDALONE */
