/*
 * Yet another DCPU-16 emulator/virtual machine/whatever.
 *
 * The acronym is supposed to mean "cycle-accurate dcpu emulator", since that is the
 * main goal. Most of the implementations that have popped up the last few days seem
 * to ignore the cycle-accurate part, which is understandable. I've never written a
 * cycle-accurate CPU emulator though, so that's the itch here.
 *
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------- */

/*
Basic opcodes: (4 bits)
    0x0: non-basic instruction - see below
    0x1: SET a, b - sets a to b
    0x2: ADD a, b - sets a to a+b, sets O to 0x0001 if there's an overflow, 0x0 otherwise
    0x3: SUB a, b - sets a to a-b, sets O to 0xffff if there's an underflow, 0x0 otherwise
    0x4: MUL a, b - sets a to a*b, sets O to ((a*b)>>16)&0xffff
    0x5: DIV a, b - sets a to a/b, sets O to ((a<<16)/b)&0xffff. if b==0, sets a and O to 0 instead.
    0x6: MOD a, b - sets a to a%b. if b==0, sets a to 0 instead.
    0x7: SHL a, b - sets a to a<<b, sets O to ((a<<b)>>16)&0xffff
    0x8: SHR a, b - sets a to a>>b, sets O to ((a<<16)>>b)&0xffff
    0x9: AND a, b - sets a to a&b
    0xa: BOR a, b - sets a to a|b
    0xb: XOR a, b - sets a to a^b
    0xc: IFE a, b - performs next instruction only if a==b
    0xd: IFN a, b - performs next instruction only if a!=b
    0xe: IFG a, b - performs next instruction only if a>b
    0xf: IFB a, b - performs next instruction only if (a&b)!=0
    
 SET, AND, BOR and XOR take 1 cycle, plus the cost of a and b
 ADD, SUB, MUL, SHR, and SHL take 2 cycles, plus the cost of a and b
 DIV and MOD take 3 cycles, plus the cost of a and b
 IFE, IFN, IFG, IFB take 2 cycles, plus the cost of a and b, plus 1 if the test fails
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

typedef enum {
	XOP_JSR = 1,
} DCPU_ExtendedOp;

typedef enum {
	REG_A = 0,
	REG_B,
	REG_C,
	REG_X,
	REG_Y,
	REG_Z,
	REG_I,
	REG_J,
	REG_COUNT
} DCPU_Register;

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

/* Pre-declare this, circular dependency. */
typedef struct DCPU_State	DCPU_State;

typedef struct Thunk	Thunk;

struct Thunk
{
	Thunk	(*execute)(DCPU_State *cpu);
};

#define	MEM_SIZE	0x10000

struct DCPU_State {
	uint16_t	registers[REG_COUNT];
	uint16_t	sp, pc, o;
	uint16_t	memory[MEM_SIZE];

	Thunk		cycle;
	uint16_t	inst;
	uint16_t	*val_a, *val_b;
	unsigned char	skip;
	uint32_t	timer;
};

/* -------------------------------------------------------------------------- */

unsigned int DCPU_ValueLength(unsigned char value)
{
	if(value < VAL_SUCC)
		return 0;
	if(value <= VAL_SUCC_LIT)
		return 1;
	return 0;
}

/* Returns length of the given instruction, in words. */
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
		printf(" register value, 0x%04x\n", **value_result);
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
	}
	else if(value == VAL_POP)
	{
		*value_result = &cpu->memory[cpu->sp++];
		printf(" POP, value 0x%04x\n", **value_result);
	}
	else if(value == VAL_PC)
	{
		*value_result = &cpu->pc;
		printf(" PC target\n");
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
		*value_result = literals + (value - 0x20);
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

static Thunk cycle_if(DCPU_State *cpu)
{
	printf(" in IF, burning a cycle\n");
	printf("ending cycle %u\n", cpu->timer++);
	return get_cycle_refetch(cpu);
}

static Thunk cycle_shl(DCPU_State *cpu)
{
	const uint32_t	res = *cpu->val_a << *cpu->val_b;

	*cpu->val_a = (res & 0xffff);
	cpu->o = res >> 16;
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
	const Thunk	next_if = { cycle_if };
	const Thunk	next_jsr = { cycle_jsr };
	const Thunk	next_shl = { cycle_shl };

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
	case OP_SET:
	case OP_ADD:
	case OP_SUB:
	case OP_SHL:
	case OP_SHR:
	case OP_IFE:
	case OP_IFN:
	case OP_IFG:
	case OP_IFB:
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
		printf("executing SET\n");
		*cpu->val_a = *cpu->val_b;
		break;
	case OP_SUB:
		printf("executing SUB\n");
		*cpu->val_a -= *cpu->val_b;
		break;
	case OP_SHL:
		printf("evaluating SHL\n");
		return next_shl;
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

void DCPU_Init(DCPU_State *cpu)
{
	memset(cpu->registers, 0, sizeof cpu->registers);
	cpu->pc = 0;
	cpu->sp = (sizeof cpu->memory / sizeof *cpu->memory) - 1;
	cpu->inst = 0;
	cpu->val_a = NULL;
	cpu->val_b = NULL;
	cpu->skip = 0;
	cpu->cycle.execute = cycle_fetch;
	cpu->timer = 0;
}

void DCPU_Load(DCPU_State *cpu, uint16_t address, const uint16_t *data, size_t length)
{
	memcpy(cpu->memory + address, data, length * sizeof *data);
}

void DCPU_PrintState(const DCPU_State *cpu)
{
	const char	*reg_names[] = { "A", "B", "C", "X", "Y", "Z", "I", "J" };
	int		i;

	printf("PC=0x%04x\nSP=0x%04x\n", cpu->pc, cpu->sp);
	printf("Next instruction: 0x%04x\n", cpu->memory[cpu->pc]);
	printf("Registers:\n");
	for(i = 0; i < sizeof cpu->registers / sizeof *cpu->registers; i++)
		printf("%6s ", reg_names[i]);
	printf("\n");
	for(i = 0; i < sizeof cpu->registers / sizeof *cpu->registers; i++)
		printf("0x%04x ", cpu->registers[i]);
	printf("\n");
	printf("\n");
}

void DCPU_Dump(const DCPU_State *cpu, uint16_t start, size_t length)
{
	for(; length > 0; --length)
	{
		printf("%04x: 0x%04x\n", start, cpu->memory[start]);
		start++;
	}
}

void DCPU_Execute(DCPU_State *cpu, unsigned int num_cycles)
{
	for(; num_cycles != 0; --num_cycles)
		cpu->cycle = cpu->cycle.execute(cpu);
}

int main(void)
{
	DCPU_State	cpu;
	const uint16_t	test[] = { 0x7c01, 0x0030, 0x7de1, 0x1000, 0x0020, 0x7803, 0x1000, 0xc00d,
				0x7dc1, 0x001a, 0xa861, 0x7c01, 0x2000, 0x2161, 0x2000, 0x8463,
				0x806d, 0x7dc1, 0x000d, 0x9031, 0x7c10, 0x0018, 0x7dc1, 0x001a,
				0x9037, 0x61c1, 0x7dc1, 0x001a, 0x0000, 0x0000, 0x0000, 0x0000 };

	DCPU_Init(&cpu);
	DCPU_Load(&cpu, 0, test, sizeof test / sizeof *test);
	DCPU_Execute(&cpu, 85);
	DCPU_PrintState(&cpu);

	return EXIT_SUCCESS;
}
