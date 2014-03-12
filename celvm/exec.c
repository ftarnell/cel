/*
 * CEL: The Compact Embeddable Language.
 * Copyright (c) 2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#include	"celcore/expr.h"

#include	"celvm/vm.h"
#include	"celvm/instr.h"

#define	STACKSZ 16

#define	GET_UINT32(p)	((uint32_t) (p)[0] << 24 |	\
			 (uint32_t) (p)[1] << 16 |	\
			 (uint32_t) (p)[2] <<  8 |	\
			 (uint32_t) (p)[3])

cel_expr_t *
cel_vm_func_execute(f)
	cel_vm_func_t	*f;
{
uint32_t	 stack[STACKSZ];
int		 sp;
uint8_t		*ip;

	ip = f->vf_bytecode;
	sp = 0;

	for (;;) {
	uint32_t	u32;
	uint64_t	u64;

		if (ip > (f->vf_bytecode + f->vf_bytecodesz)) {
			printf("ip overflow\n");
			return NULL;
		}

		switch (*ip) {
		case CEL_I_RET4:
			if (sp == 0)
				return NULL;
			return cel_make_int32(stack[--sp]);

		case CEL_I_RET8:
			if (sp <= 1)
				return NULL;
			return cel_make_int64(stack[--sp]);

		case CEL_I_LOADI4:
			stack[sp++] = GET_UINT32(ip + 1);
			ip += 5;
			break;

		default:
			printf("can't decode %d\n", *ip);
			return NULL;
		}
	}

}
