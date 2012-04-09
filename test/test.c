/*
 * Automated tests for the "CADE" DCPU-16 emulator.
 *
 * Written by Emil Brink <emil@obsession.se>, April 2012.
*/

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "cade.h"

static FILE	*old_stdout;

static struct {
	size_t	tests;
	size_t	successes;
} test_state;

static void test_begin(const char *format, ...)
{
	char	buf[1024];
	va_list	args;

	va_start(args, format);
	vsnprintf(buf, sizeof buf, format, args);
	va_end(args);
	printf("%-30s: ", buf);
	fflush(stdout);
	old_stdout = stdout;
	stdout = fopen("dump.txt", "w+");
}

static int test_end(int result)
{
	fclose(stdout);
	stdout = old_stdout;

	printf("%s\n", result ? "PASS" : "FAIL");

	test_state.tests++;
	test_state.successes += result;

	return result;
}

static int test_single_set_register_literal(DCPU_State *cpu, DCPU_Register reg, const uint16_t value)
{
	const uint16_t	set_reg_literal[] = { (0x20 + value) << 10 | (reg << 4) | 1, DCPU_STOP };

	test_begin("SET %s=0x%02x", DCPU_GetRegisterName(reg), value);

	DCPU_Init(cpu);
	DCPU_Load(cpu, 0x0000, set_reg_literal, sizeof set_reg_literal / sizeof *set_reg_literal);
	DCPU_StepUntilStuck(cpu);

	return test_end(DCPU_GetRegister(cpu, reg) == value);
}

static void test_set_register_literal(DCPU_State *cpu)
{
	const DCPU_Register	regs[] = { DCPU_REG_A, DCPU_REG_B, DCPU_REG_C, DCPU_REG_X, DCPU_REG_Y, DCPU_REG_Z, DCPU_REG_I, DCPU_REG_J };
	size_t			i, j;

	for(i = 0; i < sizeof regs / sizeof *regs; i++)
	{
		for(j = 0; j < 0x20; j++)
		{
			test_single_set_register_literal(cpu, regs[i], j);
		}
	}
}

int main(void)
{
	DCPU_State	*cpu;
	int		success = 0;

	test_state.successes = 0;
	test_state.tests = 0;

	if((cpu = DCPU_Create()) != NULL)
	{
		test_set_register_literal(cpu);
		printf("%zu/%zu tests succeeded.\n", test_state.successes, test_state.tests);

		DCPU_Destroy(cpu);
	}
	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
