/*
 * CEL: The Compact Embeddable Language.
 * Copyright (c) 2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#ifndef	CEL_VM_H
#define	CEL_VM_H

#include	<inttypes.h>

#include	"celcore/scope.h"

typedef union cel_vm_any_t {
	int8_t		i8;
	uint8_t		u8;
	int16_t		i16;
	uint16_t	u16;
	int32_t		i32;
	uint32_t	u32;
	int64_t		i64;
	uint64_t	u64;
	float		sflt;
	double		dflt;
	long double	qflt;
	uintptr_t	ptr;
} cel_vm_any_t;

struct cel_function;

typedef struct cel_vm_func {
	uint8_t		*vf_bytecode;
	size_t		 vf_bytecodesz;

	char const	**vf_vars;
	size_t		  vf_nvars;

	struct cel_function **vf_funcs;
	size_t		      vf_nfuncs;
} cel_vm_func_t;

cel_vm_func_t	*cel_vm_func_compile_stmts(cel_scope_t *, cel_expr_list_t *);
cel_vm_func_t	*cel_vm_func_compile(cel_scope_t *, struct cel_function *);
int		 cel_vm_func_execute(cel_scope_t *, cel_vm_func_t *, cel_vm_any_t *ret);
void		 cel_vm_func_free(cel_scope_t *, cel_expr_t *);

#endif	/* CEL_VM_H */
