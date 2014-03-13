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
#include	<string.h>

#include	"celcore/expr.h"
#include	"celcore/type.h"

#include	"celvm/vm.h"
#include	"celvm/instr.h"

static int32_t	cel_vm_add_bytecode(cel_vm_func_t *, uint8_t *, size_t);
static int32_t	cel_vm_emit_expr(cel_vm_func_t *, cel_expr_t *);
static int32_t	cel_vm_emit_instr(cel_vm_func_t *, int);
static int32_t	cel_vm_emit_binary(cel_vm_func_t *, cel_expr_t *);
static int32_t	cel_vm_emit_unary(cel_vm_func_t *, cel_expr_t *);
static int32_t	cel_vm_emit_literal(cel_vm_func_t *, cel_expr_t *);
static int32_t	cel_vm_emit_return(cel_vm_func_t *, cel_expr_t *);
static int32_t	cel_vm_emit_immed16(cel_vm_func_t *, uint32_t);
static int32_t	cel_vm_emit_immed32(cel_vm_func_t *, uint32_t);
static int32_t	cel_vm_emit_immed64(cel_vm_func_t *, uint64_t);
static int32_t	cel_vm_emit_loadi16(cel_vm_func_t *, uint32_t);
static int32_t	cel_vm_emit_loadi32(cel_vm_func_t *, uint32_t);
static int32_t	cel_vm_emit_loadi64(cel_vm_func_t *, uint64_t);
static uint32_t	cel_vm_expr_to_u32(cel_expr_t *e);
static uint64_t cel_vm_expr_to_u64(cel_expr_t *e);

cel_vm_func_t *
cel_vm_func_compile(el)
	cel_expr_list_t	*el;
{
cel_expr_t	*e;
cel_vm_func_t	*ret;

	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;

	CEL_TAILQ_FOREACH(e, el, ce_entry) {
		cel_vm_emit_expr(ret, e);
	}

	return ret;
}

static int32_t
cel_vm_emit_expr(f, e)
	cel_vm_func_t	*f;
	cel_expr_t	*e;
{
	switch (e->ce_tag) {
	case cel_exp_return:	return cel_vm_emit_return(f, e);
	case cel_exp_binary:	return cel_vm_emit_binary(f, e);
	case cel_exp_unary:	return cel_vm_emit_unary(f, e);
	case cel_exp_literal:	return cel_vm_emit_literal(f, e);

	default:
		printf("can't compile expr type %d\n", e->ce_tag);
		abort();
	}
}

static int32_t
cel_vm_emit_unary(f, e)
	cel_vm_func_t	*f;
	cel_expr_t	*e;
{
int	is64 = 0;
int32_t	sz;

	if (e->ce_op.ce_unary.operand->ce_type->ct_tag == cel_type_int64)
		is64 = 1;

	sz = cel_vm_emit_expr(f, e->ce_op.ce_unary.operand);

	switch (e->ce_op.ce_unary.oper) {
	case cel_op_uni_minus:
		sz += cel_vm_emit_instr(f, is64 ? CEL_I_NEG8 : CEL_I_NEG4);
		break;

	case cel_op_negate:
		sz += cel_vm_emit_instr(f, is64 ? CEL_I_NOT8 : CEL_I_NOT4);
		break;

	default:
		printf("can't emit unary for oper %d\n", e->ce_op.ce_unary.oper);
		abort();
	}

	return sz;
}

static int32_t
cel_vm_emit_and(f, e)
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
uint8_t	 off_false_1,
	 off_false_2,
	 off_end;
uint16_t	i;

/* <left> */
	sz += cel_vm_emit_expr(f, e->ce_op.ce_binary.left);
/* brf false */
	off_false_1 = sz;
	sz += cel_vm_emit_instr(f, CEL_I_BRF);
	patch_false_1 = f->vf_bytecodesz;
	sz += cel_vm_emit_immed16(f, 0);
/* <right> */
	sz += cel_vm_emit_expr(f, e->ce_op.ce_binary.right);
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
cel_vm_emit_or(f, e)
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
uint8_t	 off_true_1,
	 off_true_2,
	 off_end;
uint16_t	i;

/* <left> */
	sz += cel_vm_emit_expr(f, e->ce_op.ce_binary.left);
/* brt true */
	off_true_1 = sz;
	sz += cel_vm_emit_instr(f, CEL_I_BRT);
	patch_true_1 = f->vf_bytecodesz;
	sz += cel_vm_emit_immed16(f, 0);
/* <right> */
	sz += cel_vm_emit_expr(f, e->ce_op.ce_binary.right);
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
cel_vm_emit_binary(f, e)
	cel_vm_func_t	*f;
	cel_expr_t	*e;
{
int	is64 = 0;
int	op;
int32_t	sz = 0;

	if (e->ce_op.ce_binary.left->ce_type->ct_tag == cel_type_int64 ||
	    e->ce_op.ce_binary.left->ce_type->ct_tag == cel_type_uint64)
		is64 = 1;

	if (e->ce_op.ce_binary.oper == cel_op_or)
		return cel_vm_emit_or(f, e);
	if (e->ce_op.ce_binary.oper == cel_op_and)
		return cel_vm_emit_and(f, e);

	sz = cel_vm_emit_expr(f, e->ce_op.ce_binary.left);
	sz = cel_vm_emit_expr(f, e->ce_op.ce_binary.right);

	switch (e->ce_op.ce_binary.oper) {
	case cel_op_plus:	op = is64? CEL_I_ADD8 : CEL_I_ADD4; break;
	case cel_op_minus:	op = is64? CEL_I_SUB8 : CEL_I_SUB4; break;
	case cel_op_mult:	op = is64? CEL_I_MUL8 : CEL_I_MUL4; break;
	case cel_op_div:	op = is64? CEL_I_DIV8 : CEL_I_DIV4; break;
	case cel_op_le:		op = is64? CEL_I_TLE8 : CEL_I_TLE4; break;
	case cel_op_lt:		op = is64? CEL_I_TLT8 : CEL_I_TLT4; break;
	case cel_op_ge:		op = is64? CEL_I_TGE8 : CEL_I_TGE4; break;
	case cel_op_gt:		op = is64? CEL_I_TGT8 : CEL_I_TGT4; break;
	case cel_op_eq:		op = is64? CEL_I_TEQ8 : CEL_I_TEQ4; break;
	case cel_op_neq:	op = is64? CEL_I_TNE8 : CEL_I_TNE4; break;

	default:
		printf("can't emit binary type %d\n", e->ce_op.ce_binary.oper);
		abort();
	}

	sz = cel_vm_emit_instr(f, op);
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
cel_vm_emit_loadi32(f, i)
	cel_vm_func_t	*f;
	uint32_t	 i;
{
int32_t	sz = 0;
	sz += cel_vm_emit_instr(f, CEL_I_LOADI4);
	sz += cel_vm_emit_immed32(f, i);
	return sz;
}

static int32_t
cel_vm_emit_loadi64(f, i)
	cel_vm_func_t	*f;
	uint64_t	 i;
{
int32_t	sz = 0;
	sz += cel_vm_emit_instr(f, CEL_I_LOADI8);
	sz += cel_vm_emit_immed64(f, i);
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
		abort();
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
		abort();
	}
}

static int32_t
cel_vm_emit_literal(f, e)
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
	default:
		printf("can't emit literal for tag %d\n", e->ce_tag);
		abort();
	}
}

static int32_t
cel_vm_emit_return(f, e)
	cel_vm_func_t	*f;
	cel_expr_t	*e;
{
int32_t	sz = 0;

	sz += cel_vm_emit_expr(f, e->ce_op.ce_unary.operand);

	switch (e->ce_op.ce_unary.operand->ce_type->ct_tag) {
	case cel_type_int8:
	case cel_type_uint8:
	case cel_type_int16:
	case cel_type_uint16:
	case cel_type_int32:
	case cel_type_uint32:
	case cel_type_bool:
		sz += cel_vm_emit_instr(f, CEL_I_RET4);
		break;

	case cel_type_int64:
	case cel_type_uint64:
		sz += cel_vm_emit_instr(f, CEL_I_RET8);
		break;

	default:
		printf("can't emit return for tag %d\n", e->ce_op.ce_unary.operand->ce_type->ct_tag);
		abort();
	}

	return sz;
}
