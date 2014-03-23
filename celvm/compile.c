/*
 * CEL: The Compact Embeddable Language.
 * Copyright (c) 2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>

#include	"celcore/expr.h"
#include	"celcore/type.h"
#include	"celcore/function.h"

#include	"celvm/vm.h"
#include	"celvm/instr.h"

static int32_t	cel_vm_add_bytecode(cel_vm_func_t *, uint8_t *, size_t);
static int32_t	cel_vm_emit_expr(cel_scope_t *, cel_vm_func_t *, cel_expr_t *, int);
static int32_t	cel_vm_emit_instr(cel_vm_func_t *, int);
static int32_t	cel_vm_emit_instr_immed8(cel_vm_func_t *, int, int);
static int32_t	cel_vm_emit_instr_immed16(cel_vm_func_t *, int, int);
static int32_t	cel_vm_emit_while(cel_scope_t *, cel_vm_func_t *, cel_expr_t *);
static int32_t	cel_vm_emit_binary(cel_scope_t *, cel_vm_func_t *, cel_expr_t *);
static int32_t	cel_vm_emit_unary(cel_scope_t *, cel_vm_func_t *, cel_expr_t *);
static int32_t	cel_vm_emit_literal(cel_scope_t *, cel_vm_func_t *, cel_expr_t *);
static int32_t	cel_vm_emit_variable(cel_scope_t *, cel_vm_func_t *, cel_expr_t *);
static int32_t	cel_vm_emit_vaddr(cel_scope_t *, cel_vm_func_t *, cel_expr_t *);
static int32_t	cel_vm_emit_vardecl(cel_scope_t *, cel_vm_func_t *, cel_expr_t *);
static int32_t	cel_vm_emit_assign(cel_scope_t *, cel_vm_func_t *, cel_expr_t *);
static int32_t	cel_vm_emit_call(cel_scope_t *, cel_vm_func_t *, cel_expr_t *);
static int32_t	cel_vm_emit_incr(cel_scope_t *, cel_vm_func_t *, cel_expr_t *);
static int32_t	cel_vm_emit_immed8(cel_vm_func_t *, uint32_t);
static int32_t	cel_vm_emit_immed16(cel_vm_func_t *, uint32_t);
static int32_t	cel_vm_emit_immed32(cel_vm_func_t *, uint32_t);
static int32_t	cel_vm_emit_immed64(cel_vm_func_t *, uint64_t);
static int32_t	cel_vm_emit_immedsf(cel_vm_func_t *, double);
static int32_t	cel_vm_emit_immeddf(cel_vm_func_t *, double);
static int32_t	cel_vm_emit_immedqf(cel_vm_func_t *, long double);
static int32_t	cel_vm_emit_loadi16(cel_vm_func_t *, uint32_t);
static int32_t	cel_vm_emit_loadi32(cel_vm_func_t *, uint32_t);
static int32_t	cel_vm_emit_loadi64(cel_vm_func_t *, uint64_t);
static int32_t	cel_vm_emit_loadsf(cel_vm_func_t *, double);
static int32_t	cel_vm_emit_loaddf(cel_vm_func_t *, double);
static int32_t	cel_vm_emit_loadqf(cel_vm_func_t *, long double);
static int32_t	cel_vm_emit_loadip(cel_vm_func_t *, void *);
static uint32_t	cel_vm_expr_to_u32(cel_expr_t *e);
static uint64_t cel_vm_expr_to_u64(cel_expr_t *e);
static uint8_t	va_from_type(int);

cel_vm_func_t *
cel_vm_func_compile(s, f)
	cel_scope_t	*s;
	cel_function_t	*f;
{
cel_expr_t		*e;
cel_vm_func_t		*ret;
int			 i = 0;
int32_t			 sz = 0;
int32_t			 patch_nvars, space;

	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;

	if (f->cf_extern) {
		/*
		 * Our arguments are already on the stack, so we just
		 * need to emit a CALLE with the correct address.
		 */
		sz += cel_vm_emit_loadip(ret, f);
		sz += cel_vm_emit_instr(ret, CEL_I_CALLE);
		sz += cel_vm_emit_instr(ret, CEL_I_RET);
		return ret;
	}

/* Allocate variable storage on the stack */
#if 0
	sz += cel_vm_emit_instr(ret, CEL_I_ALLV);
#endif

/* mov %r4, %sp */
	sz += cel_vm_emit_instr(ret, CEL_I_MOVR);
	sz += cel_vm_emit_immed8(ret, 4);
	sz += cel_vm_emit_immed8(ret, 1);

/* dec %r4, (8 * f->cf_nargs) */
	sz += cel_vm_emit_instr(ret, CEL_I_DECR);
	sz += cel_vm_emit_immed8(ret, 4);
	sz += cel_vm_emit_immed32(ret, 8 * f->cf_nargs);

/* inc %sp, (8 * f->vf_nvars) */
	sz += cel_vm_emit_instr(ret, CEL_I_INCR);
	sz += cel_vm_emit_immed8(ret, 1);
	patch_nvars = sz;
	sz += cel_vm_emit_immed32(ret, 0);

/* Emit a variable for each function argument */
	for (i = 0; i < f->cf_nargs; i++) {
		ret->vf_vars = realloc(ret->vf_vars, sizeof(char const *) * (ret->vf_nvars + 1));
		ret->vf_vars[ret->vf_nvars] = f->cf_argnames[i];
		ret->vf_nvars++;
	}

	CEL_TAILQ_FOREACH(e, &f->cf_body, ce_entry) 
		sz += cel_vm_emit_expr(s, ret, e, 0);

	sz += cel_vm_emit_instr(ret, CEL_I_RET);
	space = 8 * ret->vf_nvars;

	ret->vf_bytecode[patch_nvars + 0] = ((space >> 24) & 0xFF);
	ret->vf_bytecode[patch_nvars + 1] = ((space >> 16) & 0xFF);
	ret->vf_bytecode[patch_nvars + 2] = ((space >>  8) & 0xFF);
	ret->vf_bytecode[patch_nvars + 3] = ((space >>  0) & 0xFF);
	return ret;
}

static uint8_t
va_from_type(t)
{
	switch (t) {
	case cel_type_int8:	return CEL_VA_INT8;
	case cel_type_uint8:	return CEL_VA_UINT8;
	case cel_type_int16:	return CEL_VA_INT16;
	case cel_type_uint16:	return CEL_VA_UINT16;
	case cel_type_bool:
	case cel_type_int32:	return CEL_VA_INT32;
	case cel_type_uint32:	return CEL_VA_UINT32;
	case cel_type_int64:	return CEL_VA_INT64;
	case cel_type_uint64:	return CEL_VA_UINT64;
	case cel_type_sfloat:	return CEL_VA_SFLOAT;
	case cel_type_dfloat:	return CEL_VA_DFLOAT;
	case cel_type_qfloat:	return CEL_VA_QFLOAT;
	case cel_type_ptr:	return CEL_VA_PTR;
	default:
		printf("can't get va for type %d\n", t);
		return -1;
	}
}

cel_vm_func_t *
cel_vm_func_compile_stmts(s, el)
	cel_scope_t	*s;
	cel_expr_list_t	*el;
{
cel_expr_t	*e;
cel_vm_func_t	*ret;

	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;

	CEL_TAILQ_FOREACH(e, el, ce_entry)
		if (cel_vm_emit_expr(s, ret, e, 0) == -1)
			return NULL;

	return ret;
}

static int32_t
cel_vm_emit_expr(s, f, e, nr)
	cel_scope_t	*s;
	cel_vm_func_t	*f;
	cel_expr_t	*e;
{
int32_t	sz = 0;

	switch (e->ce_tag) {
	case cel_exp_binary:	sz += cel_vm_emit_binary(s, f, e); break;
	case cel_exp_unary:	sz += cel_vm_emit_unary(s, f, e); break;
	case cel_exp_literal:	sz += cel_vm_emit_literal(s, f, e); break;
	case cel_exp_while:	sz += cel_vm_emit_while(s, f, e); break;
	case cel_exp_variable:	sz += cel_vm_emit_variable(s, f, e); break;
	case cel_exp_vardecl:	sz += cel_vm_emit_vardecl(s, f, e); break;
	case cel_exp_call:	sz += cel_vm_emit_call(s, f, e); break;

	default:
		printf("can't compile expr type %d\n", e->ce_tag);
		return -1;
	}

	/*
	 * If the expr created a value we don't need, discard it.
	 */
	if (!nr)
		if (!e->ce_type || (e->ce_type && (e->ce_type->ct_tag != cel_type_void)))
			sz += cel_vm_emit_instr(f, CEL_I_POPD);
	return sz;
}

static int32_t
cel_vm_emit_unary(s, f, e)
	cel_scope_t	*s;
	cel_vm_func_t	*f;
	cel_expr_t	*e;
{
int	type;
int32_t	sz = 0;

	if (e->ce_op.ce_unary.oper == cel_op_addr)
		return cel_vm_emit_vaddr(s, f, e->ce_op.ce_unary.operand);

	else if (e->ce_op.ce_unary.oper == cel_op_return) {

		if (e->ce_op.ce_unary.operand->ce_type->ct_tag != cel_type_void) {
			sz += cel_vm_emit_expr(s, f, e->ce_op.ce_unary.operand, 1);
			sz += cel_vm_emit_instr_immed8(f, CEL_I_STOR, 3);
		}

		sz += cel_vm_emit_instr(f, CEL_I_RET);
		return sz;
	}

	sz = cel_vm_emit_expr(s, f, e->ce_op.ce_unary.operand, 1);

	type = va_from_type(e->ce_op.ce_unary.operand->ce_type->ct_tag);

	switch (e->ce_op.ce_unary.oper) {
	case cel_op_uni_minus:
		sz += cel_vm_emit_instr_immed8(f, CEL_I_NEG, type);
		break;

	case cel_op_negate:
		sz += cel_vm_emit_instr_immed8(f, CEL_I_NOT, type);
		break;

	case cel_op_deref:
		sz += cel_vm_emit_instr_immed8(f, CEL_I_LOADM, type);
		break;

	default:
		printf("can't emit unary for oper %d\n", e->ce_op.ce_unary.oper);
		return -1;
	}

	return sz;
}

static int32_t
cel_vm_emit_while(s, f, e)
	cel_scope_t	*s;
	cel_vm_func_t	*f;
	cel_expr_t	*e;
{
int32_t	sz = 0;

/*
 * start:
 *     <cond>
 *     brf end
 *     <body>
 *     br start
 * end:
 */
uint32_t	 patch_end;
uint16_t	 off_end, off_start;
int16_t		 i;
cel_expr_t	*stmt;

/* start: */
	off_start = f->vf_bytecodesz;

/* <cond> */
	sz += cel_vm_emit_expr(s, f, e->ce_op.ce_while->wh_condition, 1);

/* brf end */
	off_end = sz;
	sz += cel_vm_emit_instr(f, CEL_I_BRF);
	patch_end = f->vf_bytecodesz;
	sz += cel_vm_emit_immed16(f, 0);

/* <body> */
	CEL_TAILQ_FOREACH(stmt, &e->ce_op.ce_while->wh_exprs, ce_entry)
		sz += cel_vm_emit_expr(s, f, stmt, 0);

/* br start */
	i = (off_start - f->vf_bytecodesz);
	sz += cel_vm_emit_instr(f, CEL_I_BR);
	sz += cel_vm_emit_immed16(f, i);

/* end: */
	i = (sz - off_end);
	f->vf_bytecode[patch_end + 0] = (i >> 8);
	f->vf_bytecode[patch_end + 1] = (i & 0xFF);

	return sz;
}

static int32_t
cel_vm_emit_and(s, f, e)
	cel_scope_t	*s;
	cel_vm_func_t	*f;
	cel_expr_t	*e;
{
int32_t	sz = 0;

/*
 *     <left>
 *     brf false
 *     <right>
 *     brf false
 *     push 1
 *     br end
 * false:
 *     push 0
 * end:
 */
uint32_t patch_false_1,
	 patch_false_2,
	 patch_end;
uint32_t off_false_1,
	 off_false_2,
	 off_end;
uint16_t	i;

/* <left> */
	sz += cel_vm_emit_expr(s, f, e->ce_op.ce_binary.left, 1);
/* brf false */
	off_false_1 = sz;
	sz += cel_vm_emit_instr(f, CEL_I_BRF);
	patch_false_1 = f->vf_bytecodesz;
	sz += cel_vm_emit_immed16(f, 0);
/* <right> */
	sz += cel_vm_emit_expr(s, f, e->ce_op.ce_binary.right, 1);
/* brf false */
	off_false_2 = sz;
	sz += cel_vm_emit_instr(f, CEL_I_BRF);
	patch_false_2 = f->vf_bytecodesz;
	sz += cel_vm_emit_immed16(f, 0);
/* push 1 */
	sz += cel_vm_emit_loadi32(f, 1);
/* br end */
	off_end = sz;
	sz += cel_vm_emit_instr(f, CEL_I_BR);
	patch_end = f->vf_bytecodesz;
	sz += cel_vm_emit_immed16(f, 0);
/* false: */
	i = (sz - off_false_1);
	f->vf_bytecode[patch_false_1 + 0] = (i >> 8);
	f->vf_bytecode[patch_false_1 + 1] = (i & 0xFF);
	i = (sz - off_false_2);
	f->vf_bytecode[patch_false_2 + 0] = (i >> 8);
	f->vf_bytecode[patch_false_2 + 1] = (i & 0xFF);
/* push 0 */
	sz += cel_vm_emit_loadi32(f, 0);
/* end: */
	i = (sz - off_end);
	f->vf_bytecode[patch_end + 0] = (i >> 8);
	f->vf_bytecode[patch_end + 1] = (i & 0xFF);

	return sz;
}

static int32_t
cel_vm_emit_or(s, f, e)
	cel_scope_t	*s;
	cel_vm_func_t	*f;
	cel_expr_t	*e;
{
int32_t	sz = 0;

/*
 *     <left>
 *     brt true
 *     <right>
 *     brt true
 *     push 0
 *     br end
 * true:
 *     push 1
 * end:
 */
uint32_t patch_true_1,
	 patch_true_2,
	 patch_end;
uint32_t off_true_1,
	 off_true_2,
	 off_end;
uint16_t	i;

/* <left> */
	sz += cel_vm_emit_expr(s, f, e->ce_op.ce_binary.left, 1);
/* brt true */
	off_true_1 = sz;
	sz += cel_vm_emit_instr(f, CEL_I_BRT);
	patch_true_1 = f->vf_bytecodesz;
	sz += cel_vm_emit_immed16(f, 0);
/* <right> */
	sz += cel_vm_emit_expr(s, f, e->ce_op.ce_binary.right, 1);
/* brt true */
	off_true_2 = sz;
	sz += cel_vm_emit_instr(f, CEL_I_BRT);
	patch_true_2 = f->vf_bytecodesz;
	sz += cel_vm_emit_immed16(f, 0);
/* push 0 */
	sz += cel_vm_emit_loadi32(f, 0);
/* br end */
	off_end = sz;
	sz += cel_vm_emit_instr(f, CEL_I_BR);
	patch_end = f->vf_bytecodesz;
	sz += cel_vm_emit_immed16(f, 0);
/* true: */
	i = (sz - off_true_1);
	f->vf_bytecode[patch_true_1 + 0] = (i >> 8);
	f->vf_bytecode[patch_true_1 + 1] = (i & 0xFF);
	i = (sz - off_true_2);
	f->vf_bytecode[patch_true_2 + 0] = (i >> 8);
	f->vf_bytecode[patch_true_2 + 1] = (i & 0xFF);
/* push 1 */
	sz += cel_vm_emit_loadi32(f, 1);
/* end: */
	i = (sz - off_end);
	f->vf_bytecode[patch_end + 0] = (i >> 8);
	f->vf_bytecode[patch_end + 1] = (i & 0xFF);

	return sz;
}

static int32_t
cel_vm_emit_binary(s, f, e)
	cel_scope_t	*s;
	cel_vm_func_t	*f;
	cel_expr_t	*e;
{
int	op, type;
int32_t	sz = 0;

	switch (e->ce_op.ce_binary.oper) {
	case cel_op_assign:	return cel_vm_emit_assign(s, f, e);
	case cel_op_incr:
	case cel_op_decr:
	case cel_op_multn:
	case cel_op_divn:
				return cel_vm_emit_incr(s, f, e);
	default:		break;
	}

	if (e->ce_op.ce_binary.oper == cel_op_or)
		return cel_vm_emit_or(s, f, e);
	if (e->ce_op.ce_binary.oper == cel_op_and)
		return cel_vm_emit_and(s, f, e);

	sz += cel_vm_emit_expr(s, f, e->ce_op.ce_binary.left, 1);
	sz += cel_vm_emit_expr(s, f, e->ce_op.ce_binary.right, 1);

	type = va_from_type(e->ce_op.ce_binary.left->ce_type->ct_tag);

	switch (e->ce_op.ce_binary.oper) {
	case cel_op_plus:	op = CEL_I_ADD; break;
	case cel_op_minus:	op = CEL_I_SUB; break;
	case cel_op_mult:	op = CEL_I_MUL; break;
	case cel_op_div:	op = CEL_I_DIV; break;
	case cel_op_le:		op = CEL_I_TLE; break;
	case cel_op_lt:		op = CEL_I_TLT; break;
	case cel_op_ge:		op = CEL_I_TGE; break;
	case cel_op_gt:		op = CEL_I_TGT; break;
	case cel_op_eq:		op = CEL_I_TEQ; break;
	case cel_op_neq:	op = CEL_I_TNE; break;

	default:
		printf("can't emit binary type %d\n", e->ce_op.ce_binary.oper);
		return -1;
	}

	sz += cel_vm_emit_instr_immed8(f, op, type);
	return sz;
}

static int32_t
cel_vm_add_bytecode(f, b, sz)
	cel_vm_func_t	*f;
	uint8_t		*b;
	size_t		 sz;
{
size_t	osz = f->vf_bytecodesz;
	f->vf_bytecode = realloc(f->vf_bytecode, f->vf_bytecodesz + sz);
	memcpy(f->vf_bytecode + osz, b, sz);
	f->vf_bytecodesz += sz;
	return sz;
}

static int32_t
cel_vm_emit_instr(f, i)
	cel_vm_func_t	*f;
{
uint8_t	bc[] = { i };
	return cel_vm_add_bytecode(f, bc, 1);
}

static int32_t
cel_vm_emit_immed8(f, i)
	cel_vm_func_t	*f;
	uint32_t	 i;
{
uint8_t	bc[] = { i };
	return cel_vm_add_bytecode(f, bc, sizeof(bc));
}

static int32_t
cel_vm_emit_instr_immed16(f, i, v)
	cel_vm_func_t	*f;
{
int32_t	sz = 0;
	sz += cel_vm_emit_instr(f, i);
	sz += cel_vm_emit_immed16(f, v);
	return sz;
}

static int32_t
cel_vm_emit_instr_immed8(f, i, v)
	cel_vm_func_t	*f;
{
int32_t	sz = 0;
	sz += cel_vm_emit_instr(f, i);
	sz += cel_vm_emit_immed8(f, v);
	return sz;
}

static int32_t
cel_vm_emit_immedsf(f, i)
	cel_vm_func_t	*f;
	float		 i;
{
uint8_t	*bc = (uint8_t *) &i;
	return cel_vm_add_bytecode(f, bc, sizeof(i));
}

static int32_t
cel_vm_emit_immeddf(f, i)
	cel_vm_func_t	*f;
	double		 i;
{
uint8_t	*bc = (uint8_t *) &i;
	return cel_vm_add_bytecode(f, bc, sizeof(i));
}

static int32_t
cel_vm_emit_immedqf(f, i)
	cel_vm_func_t	*f;
	long double	 i;
{
uint8_t	*bc = (uint8_t *) &i;
	return cel_vm_add_bytecode(f, bc, sizeof(i));
}

static int32_t
cel_vm_emit_immed16(f, i)
	cel_vm_func_t	*f;
	uint32_t	 i;
{
uint8_t	bc[] = {
		(i >>  8) & 0xff,
		(i      ) & 0xff
};
	return cel_vm_add_bytecode(f, bc, sizeof(bc));
}

static int32_t
cel_vm_emit_immed32(f, i)
	cel_vm_func_t	*f;
	uint32_t	 i;
{
uint8_t	bc[] = {
		(i >> 24) & 0xff,
		(i >> 16) & 0xff,
		(i >>  8) & 0xff,
		(i      ) & 0xff
};
	return cel_vm_add_bytecode(f, bc, sizeof(bc));
}

static int32_t
cel_vm_emit_immed64(f, i)
	cel_vm_func_t	*f;
	uint64_t	 i;
{
uint8_t	bc[] = {
		(i >> 56) & 0xff,
		(i >> 48) & 0xff,
		(i >> 40) & 0xff,
		(i >> 32) & 0xff,
		(i >> 24) & 0xff,
		(i >> 16) & 0xff,
		(i >>  8) & 0xff,
		(i      ) & 0xff
};
	return cel_vm_add_bytecode(f, bc, sizeof(bc));
}

static int32_t
cel_vm_emit_loadsf(f, v)
	cel_vm_func_t	*f;
	float		 v;
{
int32_t	sz = 0;
	sz += cel_vm_emit_instr(f, CEL_I_LOADI);
	sz += cel_vm_emit_immed8(f, CEL_VA_SFLOAT);
	sz += cel_vm_emit_immedsf(f, v);
	return sz;
}

static int32_t
cel_vm_emit_loaddf(f, v)
	cel_vm_func_t	*f;
	double		 v;
{
int32_t	sz = 0;
	sz += cel_vm_emit_instr(f, CEL_I_LOADI);
	sz += cel_vm_emit_immed8(f, CEL_VA_DFLOAT);
	sz += cel_vm_emit_immeddf(f, v);
	return sz;
}

static int32_t
cel_vm_emit_loadqf(f, v)
	cel_vm_func_t	*f;
	long double	 v;
{
int32_t	sz = 0;
	sz += cel_vm_emit_instr(f, CEL_I_LOADI);
	sz += cel_vm_emit_immed8(f, CEL_VA_QFLOAT);
	sz += cel_vm_emit_immedqf(f, v);
	return sz;
}

static int32_t
cel_vm_emit_loadi32(f, i)
	cel_vm_func_t	*f;
	uint32_t	 i;
{
int32_t	sz = 0;
	sz += cel_vm_emit_instr(f, CEL_I_LOADI);
	sz += cel_vm_emit_immed8(f, CEL_VA_UINT32);
	sz += cel_vm_emit_immed32(f, i);
	return sz;
}

static int32_t
cel_vm_emit_loadi64(f, i)
	cel_vm_func_t	*f;
	uint64_t	 i;
{
int32_t	sz = 0;
	sz += cel_vm_emit_instr(f, CEL_I_LOADI);
	sz += cel_vm_emit_immed8(f, CEL_VA_UINT64);
	sz += cel_vm_emit_immed64(f, i);
	return sz;
}

static int32_t
cel_vm_emit_loadip(f, p)
	cel_vm_func_t	*f;
	void		*p;
{
int32_t sz = 0;
	sz += cel_vm_emit_instr(f, CEL_I_LOADI);
	sz += cel_vm_emit_immed8(f, CEL_VA_PTR);
#if	SIZEOF_VOIDP == 4
	sz += cel_vm_emit_immed32(f, (uint32_t) p);
#else
	sz += cel_vm_emit_immed64(f, (uint64_t) p);
#endif
	return sz;
}

static uint32_t
cel_vm_expr_to_u32(e)
	cel_expr_t	*e;
{
	switch (e->ce_type->ct_tag) {
	case cel_type_int8:	return e->ce_op.ce_int8; break;
	case cel_type_uint8:	return e->ce_op.ce_uint8; break;
	case cel_type_int16:	return e->ce_op.ce_int16; break;
	case cel_type_uint16:	return e->ce_op.ce_uint16; break;
	case cel_type_int32:	return e->ce_op.ce_int32; break;
	case cel_type_uint32:	return e->ce_op.ce_uint32; break;
		break;

	default:
		printf("can't convert tag %d to u32\n", e->ce_type->ct_tag);
		return -1;
	}
}

static uint64_t
cel_vm_expr_to_u64(e)
	cel_expr_t	*e;
{
	switch (e->ce_type->ct_tag) {
	case cel_type_int64:	return e->ce_op.ce_int64;
	case cel_type_uint64:	return e->ce_op.ce_uint64;
	default:
		printf("can't convert tag %d to u64\n", e->ce_type->ct_tag);
		return -1;
	}
}

static int32_t
cel_vm_emit_literal(s, f, e)
	cel_scope_t	*s;
	cel_vm_func_t	*f;
	cel_expr_t	*e;
{
	switch (e->ce_type->ct_tag) {
	case cel_type_int8:	return cel_vm_emit_loadi32(f, e->ce_op.ce_int8);
	case cel_type_uint8:	return cel_vm_emit_loadi32(f, e->ce_op.ce_uint8);
	case cel_type_int16:	return cel_vm_emit_loadi32(f, e->ce_op.ce_int16);
	case cel_type_uint16:	return cel_vm_emit_loadi32(f, e->ce_op.ce_uint16);
	case cel_type_int32:	return cel_vm_emit_loadi32(f, e->ce_op.ce_int32);
	case cel_type_uint32:	return cel_vm_emit_loadi32(f, e->ce_op.ce_uint32);
	case cel_type_int64:	return cel_vm_emit_loadi64(f, e->ce_op.ce_int64);
	case cel_type_uint64:	return cel_vm_emit_loadi64(f, e->ce_op.ce_uint64);
	case cel_type_bool:	return cel_vm_emit_loadi32(f, e->ce_op.ce_bool);
	case cel_type_ptr:	return cel_vm_emit_loadip(f, e->ce_op.ce_ptr);
	case cel_type_sfloat:	return cel_vm_emit_loadsf(f, e->ce_op.ce_sfloat);
	case cel_type_dfloat:	return cel_vm_emit_loaddf(f, e->ce_op.ce_dfloat);
	case cel_type_qfloat:	return cel_vm_emit_loadqf(f, e->ce_op.ce_qfloat);
	default:
		printf("can't emit literal for type %d\n", e->ce_type->ct_tag);
		return -1;
	}
}

static int32_t
cel_vm_emit_vardecl(s, f, e)
	cel_scope_t	*s;
	cel_vm_func_t	*f;
	cel_expr_t	*e;
{
ssize_t		 i;
int16_t		 varn = -1;
int32_t		 sz = 0;

/* Is this variable already in the var table? */
	for (i = 0; i < f->vf_nvars; i++)
		if (f->vf_vars[i] && strcmp(e->ce_op.ce_vardecl.name, f->vf_vars[i]) == 0)
			return -1;

/* No; add a new var ref */
	varn = f->vf_nvars;
	f->vf_vars = realloc(f->vf_vars, sizeof(char const *) * (f->vf_nvars + 1));
	f->vf_vars[varn] = e->ce_op.ce_vardecl.name;
	f->vf_nvars++;

/* If it has an initialiser, emit the init code */
	if (e->ce_op.ce_vardecl.init) {
		sz += cel_vm_emit_expr(s, f, e->ce_op.ce_vardecl.init, 1);

		/* The variable address is %r4 + (8 * varn) */
		sz += cel_vm_emit_instr(f, CEL_I_STOMR);
		sz += cel_vm_emit_immed8(f, 4);
		sz += cel_vm_emit_immed32(f, 8 * varn);
	}
	return sz;
}

static int32_t
cel_vm_emit_vaddr(s, f, e)
	cel_scope_t	*s;
	cel_vm_func_t	*f;
	cel_expr_t	*e;
{
ssize_t		 i;
int16_t		 varn = -1;
int32_t		 sz = 0;

/* Is this variable already in the var table? */
	for (i = 0; i < f->vf_nvars; i++) {
		if (f->vf_vars[i] && strcmp(e->ce_op.ce_variable, f->vf_vars[i]) == 0) {
			varn = i;
			break;
		}
	}

	if (varn == -1) {
	/* Global variable */
	cel_scope_item_t	*i;
	cel_function_t		*fu;
		i = cel_scope_find_item(s, e->ce_op.ce_variable);
		fu = i->si_ob.si_expr->ce_op.ce_function;
		sz += cel_vm_emit_loadip(f, fu->cf_bytecode->vf_bytecode);
		return sz;
	}

	/* The variable address is %r4 + (8 * varn) */
	
	/* push 8*varn */
	sz += cel_vm_emit_loadi64(f, 8 * varn);
	/* push %r4 */
	sz += cel_vm_emit_instr_immed8(f, CEL_I_LOADR, 4);
	/* add */
	sz += cel_vm_emit_instr_immed8(f, CEL_I_ADD, CEL_VA_UINT64);

	return sz;
}

static int32_t
cel_vm_emit_variable(s, f, e)
	cel_scope_t	*s;
	cel_vm_func_t	*f;
	cel_expr_t	*e;
{
ssize_t		 i;
int16_t		 varn = -1;
int32_t		 sz = 0;

/* Is this variable already in the var table? */
	for (i = 0; i < f->vf_nvars; i++) {
		if (f->vf_vars[i] && strcmp(e->ce_op.ce_variable, f->vf_vars[i]) == 0) {
			varn = i;
			break;
		}
	}

	if (varn == -1) {
	/* Global variable */
	cel_scope_item_t	*i;
	cel_function_t		*fu;
		i = cel_scope_find_item(s, e->ce_op.ce_variable);
		fu = i->si_ob.si_expr->ce_op.ce_function;
		sz += cel_vm_emit_loadip(f, fu->cf_bytecode->vf_bytecode);
		return sz;
	}

	/* The variable address is %r4 + (8 * varn) */
	sz += cel_vm_emit_instr(f, CEL_I_LOADMR);
	sz += cel_vm_emit_immed8(f, 4);
	sz += cel_vm_emit_immed32(f, 8 * varn);
	return sz;
}

static int32_t
cel_vm_emit_assign(s, f, e)
	cel_scope_t	*s;
	cel_vm_func_t	*f;
	cel_expr_t	*e;
{
ssize_t		 i;
int16_t		 varn = -1;
int32_t		 sz = 0;
int		 type;
cel_expr_t	*lhs = e->ce_op.ce_binary.left,
		*rhs = e->ce_op.ce_binary.right;

/* Emit the rhs */
	sz += cel_vm_emit_expr(s, f, rhs, 1);

	type = va_from_type(lhs->ce_type->ct_tag);

	if (lhs->ce_tag == cel_exp_variable) {
	/* Assigning to a variable - emit its address */
		for (i = 0; i < f->vf_nvars; i++) {
			if (f->vf_vars[i] && strcmp(lhs->ce_op.ce_variable, f->vf_vars[i]) == 0) {
				varn = i;
				break;
			}
		}

		if (varn == -1)
			return -1;

		sz += cel_vm_emit_instr(f, CEL_I_STOMR);
		sz += cel_vm_emit_immed8(f, 4);
		sz += cel_vm_emit_immed32(f, 8 * varn);

		sz += cel_vm_emit_instr(f, CEL_I_LOADMR);
		sz += cel_vm_emit_immed8(f, 4);
		sz += cel_vm_emit_immed32(f, 8 * varn);
	} else if (lhs->ce_tag == cel_exp_unary && lhs->ce_op.ce_unary.oper == cel_op_deref) {
	/* Assigning to a dereferenced address - emit the addr */
		sz += cel_vm_emit_expr(s, f, lhs->ce_op.ce_unary.operand, 1);
		sz += cel_vm_emit_instr_immed8(f, CEL_I_STOM, type);
		sz += cel_vm_emit_expr(s, f, lhs->ce_op.ce_unary.operand, 1);
		sz += cel_vm_emit_instr_immed8(f, CEL_I_LOADM, type);
	} else {
		printf("can't assign to %d\n", e->ce_tag);
		return -1;
	}

	return sz;
}

static int32_t
cel_vm_emit_call(s, f, e)
	cel_scope_t	*s;
	cel_vm_func_t	*f;
	cel_expr_t	*e;
{
size_t		 i;
cel_function_t	*fu;
int32_t		 sz = 0;

	fu = e->ce_op.ce_call.func->ce_op.ce_function;

/* Emit the arguments */
	for (i = e->ce_op.ce_call.args->ca_nargs; i > 0; i--)
		sz += cel_vm_emit_expr(s, f, e->ce_op.ce_call.args->ca_args[i - 1], 1);

/* Emit the function address */
	sz += cel_vm_emit_expr(s, f, e->ce_op.ce_call.func, 1);

/* Call it */
	sz += cel_vm_emit_instr(f, CEL_I_CALL);

/* Pop its arguments */
	for (i = e->ce_op.ce_call.args->ca_nargs; i > 0; i--)
		sz += cel_vm_emit_instr(f, CEL_I_POPD);
/* And its address */
	sz += cel_vm_emit_instr(f, CEL_I_POPD);

/* If it returns a value, push it */
	if (e->ce_type->ct_tag != cel_type_void)
		sz += cel_vm_emit_instr_immed8(f, CEL_I_LOADR, 3);

	return sz;
}

static int32_t
cel_vm_emit_incr(s, f, e)
	cel_scope_t	*s;
	cel_vm_func_t	*f;
	cel_expr_t	*e;
{
ssize_t		 i;
int16_t		 varn = -1;
int32_t		 sz = 0;
int		 inst;

/* Is this variable already in the var table? */
	for (i = 0; i < f->vf_nvars; i++) {
		if (f->vf_vars[i] && strcmp(e->ce_op.ce_binary.left->ce_op.ce_variable,
					    f->vf_vars[i]) == 0) {
			varn = i;
			break;
		}
	}

	if (varn == -1)
		return -1;

	switch (e->ce_op.ce_unary.oper) {
	case cel_op_incr:	inst = CEL_I_INCV; break;
	case cel_op_decr:	inst = CEL_I_DECV; break;
	case cel_op_multn:	inst = CEL_I_MULV; break;
	case cel_op_divn:	inst = CEL_I_DIVV; break;
	}

	sz += cel_vm_emit_expr(s, f, e->ce_op.ce_binary.right, 1);
	sz += cel_vm_emit_instr(f, inst);
	sz += cel_vm_emit_immed16(f, varn);
	sz += cel_vm_emit_immed8(f, va_from_type(e->ce_op.ce_unary.operand->ce_type->ct_tag));
	return sz;
}
