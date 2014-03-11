/*
 * CEL: The Compact Embeddable Language.
 * Copyright (c) 2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#include	<string.h>
#include	<inttypes.h>
#include	<assert.h>

#include	"celcore/expr.h"
#include	"celcore/function.h"
#include	"celcore/type.h"

#define	CEL_MAKE_INT(type)						\
	cel_expr_t *							\
	cel_make_##type(type##_t i)					\
	{								\
	cel_expr_t	*ret;						\
		if ((ret = calloc(1, sizeof(*ret))) == NULL)		\
			return NULL;					\
		ret->ce_type = cel_make_type(cel_type_##type);		\
		ret->ce_tag = cel_exp_##type;				\
		ret->ce_op.ce_##type = i;				\
		return ret;						\
	}

CEL_MAKE_INT(int8)
CEL_MAKE_INT(uint8)
CEL_MAKE_INT(int16)
CEL_MAKE_INT(uint16)
CEL_MAKE_INT(int32)
CEL_MAKE_INT(uint32)
CEL_MAKE_INT(int64)
CEL_MAKE_INT(uint64)

cel_expr_t *
cel_make_bool(i)
{
cel_expr_t	*ret;
	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;
	ret->ce_type = cel_make_type(cel_type_bool);
	ret->ce_tag = cel_exp_bool;
	ret->ce_op.ce_bool = i;
	return ret;
}

cel_expr_t *
cel_make_string(s)
	char const	*s;
{
cel_expr_t	*ret;
	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;
	ret->ce_type = cel_make_type(cel_type_string);
	ret->ce_tag = cel_exp_string;
	ret->ce_op.ce_string = strdup(s);
	return ret;
}

cel_expr_t *
cel_make_variable(s, t)
	char const	*s;
	cel_type_t	*t;
{
cel_expr_t	*ret;
	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;
	ret->ce_tag = cel_exp_variable;
	ret->ce_op.ce_variable = strdup(s);
	ret->ce_type = t;
	return ret;
}

cel_expr_t *
cel_make_function(f)
	cel_function_t	*f;
{
cel_expr_t	*ret;
	assert(f);

	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;
	ret->ce_tag = cel_exp_function;
	ret->ce_op.ce_function = f;
	ret->ce_type = f->cf_type;
	return ret;
}

cel_expr_t *
cel_make_binary(op, e, f)
	cel_bi_oper_t	 op;
	cel_expr_t	*e, *f;
{
cel_expr_t	*ret;
	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;
	ret->ce_tag = cel_exp_binary;
	ret->ce_op.ce_binary.oper = op;
	ret->ce_op.ce_binary.left = e;
	ret->ce_op.ce_binary.right = f;
	return ret;
}

cel_expr_t *
cel_make_unary(op, e)
	cel_uni_oper_t	 op;
	cel_expr_t	*e;
{
cel_expr_t	*ret;
	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;
	ret->ce_tag = cel_exp_unary;
	ret->ce_op.ce_unary.oper = op;
	ret->ce_op.ce_unary.operand = e;
	return ret;
}

cel_expr_t *
cel_make_cast(e, t)
	cel_expr_t	*e;
	cel_type_t	*t;
{
cel_expr_t	*ret;
	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;
	ret->ce_tag = cel_exp_cast;
	ret->ce_type = t;
	ret->ce_op.ce_unary.operand = e;
	return ret;
}

cel_expr_t *
cel_make_return(e)
	cel_expr_t	*e;
{
cel_expr_t	*ret;
	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;
	ret->ce_tag = cel_exp_return;
	ret->ce_type = cel_make_type(cel_type_void);
	ret->ce_op.ce_unary.operand = e;
	return ret;
}

cel_expr_t *
cel_make_call(e, a)
	cel_expr_t	*e;
	cel_arglist_t	*a;
{
cel_expr_t	*ret;
	assert(e);

	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;

	ret->ce_tag = cel_exp_call;
	ret->ce_type = e->ce_type;
	ret->ce_op.ce_call.func = e;
	ret->ce_op.ce_call.args = a;
	return ret;
}

void
cel_expr_free(e)
	cel_expr_t	*e;
{
	if (!e)
		return;

	return;

	switch (e->ce_tag) {
	case cel_exp_string:
		free(e->ce_op.ce_string);
		break;

	case cel_exp_variable:
		free(e->ce_op.ce_variable);
		break;

	case cel_exp_unary:
	case cel_exp_cast:
	case cel_exp_return:
		cel_expr_free(e->ce_op.ce_unary.operand);
		break;

	case cel_exp_binary:
		cel_expr_free(e->ce_op.ce_binary.left);
		cel_expr_free(e->ce_op.ce_binary.right);
		break;

	case cel_exp_int8:
	case cel_exp_uint8:
	case cel_exp_int16:
	case cel_exp_uint16:
	case cel_exp_int32:
	case cel_exp_uint32:
	case cel_exp_int64:
	case cel_exp_uint64:
	case cel_exp_bool:
	case cel_exp_if:
	case cel_exp_function:
	case cel_exp_vardecl:
	case cel_exp_void:
	case cel_exp_call:
		break;
	}
}

cel_expr_t *
cel_expr_copy(e)
	cel_expr_t	*e;
{
cel_expr_t	*ret;
	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;

	ret->ce_tag = e->ce_tag;
	ret->ce_type = e->ce_type;

	switch (ret->ce_tag) {
	case cel_exp_int8:	ret->ce_op.ce_int8 = e->ce_op.ce_int8; break;
	case cel_exp_uint8:	ret->ce_op.ce_uint8 = e->ce_op.ce_uint8; break;
	case cel_exp_int16:	ret->ce_op.ce_int16 = e->ce_op.ce_int16; break;
	case cel_exp_uint16:	ret->ce_op.ce_uint16 = e->ce_op.ce_uint16; break;
	case cel_exp_int32:	ret->ce_op.ce_int32 = e->ce_op.ce_int32; break;
	case cel_exp_uint32:	ret->ce_op.ce_uint32 = e->ce_op.ce_uint32; break;
	case cel_exp_int64:	ret->ce_op.ce_int64 = e->ce_op.ce_int64; break;
	case cel_exp_uint64:	ret->ce_op.ce_uint64 = e->ce_op.ce_uint64; break;

	case cel_exp_bool:
		ret->ce_op.ce_bool = e->ce_op.ce_bool;
		break;

	case cel_exp_unary:
		ret->ce_op.ce_unary.oper = e->ce_op.ce_unary.oper;
	case cel_exp_return:
	case cel_exp_cast:
		ret->ce_op.ce_unary.operand = cel_expr_copy(e->ce_op.ce_unary.operand);
		break;

	case cel_exp_call:
		ret->ce_op.ce_call.func = e->ce_op.ce_call.func;
		ret->ce_op.ce_call.args = e->ce_op.ce_call.args;
		break;

	case cel_exp_binary:
		ret->ce_op.ce_binary.oper = e->ce_op.ce_binary.oper;
		ret->ce_op.ce_binary.left = cel_expr_copy(e->ce_op.ce_binary.left);
		ret->ce_op.ce_binary.right = cel_expr_copy(e->ce_op.ce_binary.right);
		break;

	case cel_exp_string:
		ret->ce_op.ce_string = strdup(e->ce_op.ce_string);
		break;

	case cel_exp_function:
		ret->ce_op.ce_function = e->ce_op.ce_function;
		break;

	case cel_exp_variable:
		ret->ce_op.ce_variable = strdup(e->ce_op.ce_variable);
		break;

	case cel_exp_vardecl:
	case cel_exp_if:
	case cel_exp_void:
		break;
	}

	return ret;
}

void
cel_expr_print(e, b, bsz)
	cel_expr_t	*e;
	char		*b;
	size_t		 bsz;
{
char	t[64];

	strlcpy(b, "<error>", bsz);

	switch (e->ce_tag) {
	case cel_exp_int8:	snprintf(b, bsz, "%"PRId8, e->ce_op.ce_int8); break;
	case cel_exp_uint8:	snprintf(b, bsz, "%"PRIu8, e->ce_op.ce_uint8); break;
	case cel_exp_int16:	snprintf(b, bsz, "%"PRId16, e->ce_op.ce_int16); break;
	case cel_exp_uint16:	snprintf(b, bsz, "%"PRIu16, e->ce_op.ce_uint16); break;
	case cel_exp_int32:	snprintf(b, bsz, "%"PRId32, e->ce_op.ce_int32); break;
	case cel_exp_uint32:	snprintf(b, bsz, "%"PRIu32, e->ce_op.ce_uint32); break;
	case cel_exp_int64:	snprintf(b, bsz, "%"PRId64, e->ce_op.ce_int64); break;
	case cel_exp_uint64:	snprintf(b, bsz, "%"PRIu64, e->ce_op.ce_uint64); break;

	case cel_exp_string:
		snprintf(b, bsz, "\"%s\"", e->ce_op.ce_string);
		break;

	case cel_exp_bool:
		strlcpy(b, e->ce_op.ce_bool ? "true" : "false", bsz);
		break;

	case cel_exp_void:
		strlcpy(b, "<void>", bsz);
		break;

	case cel_exp_call:
		strlcpy(b, "<function call>", bsz);
		break;

	case cel_exp_cast:
		cel_expr_print(e->ce_op.ce_unary.operand, b, bsz);
		strlcat(b, " as ", bsz);
		cel_name_type(e->ce_type, t, sizeof(t));
		strlcat(b, t, bsz);
		break;

	case cel_exp_return:
		strlcpy(b, "<return ", bsz);
		cel_expr_print(e->ce_op.ce_unary.operand, t, sizeof(t));
		strlcat(b, t, bsz);
		strlcat(b, " as ", bsz);
		cel_name_type(e->ce_op.ce_unary.operand->ce_type, t, sizeof(t));
		strlcat(b, t, bsz);
		strlcat(b, ">", bsz);
		break;

	case cel_exp_variable:
		strlcpy(b, "<variable ", bsz);
		strlcat(b, e->ce_op.ce_variable, bsz);
		strlcat(b, ">", bsz);
		break;

	case cel_exp_function:
		snprintf(b, bsz, "<function @ %p>", e->ce_op.ce_function);
		break;

	case cel_exp_unary:
		strlcpy(b, "<unary op ", bsz);
		cel_name_type(e->ce_op.ce_unary.operand->ce_type, t, sizeof(t));
		strlcat(b, t, bsz);
		strlcat(b, ">", bsz);
		break;

	case cel_exp_binary:
		strlcpy(b, "<binary op ", bsz);
		cel_name_type(e->ce_op.ce_binary.left->ce_type, t, sizeof(t));
		strlcat(b, t, bsz);
		strlcat(b, ", ", bsz);
		cel_name_type(e->ce_op.ce_binary.right->ce_type, t, sizeof(t));
		strlcat(b, t, bsz);
		strlcat(b, ">", bsz);
		break;

	case cel_exp_if:
		strlcpy(b, "<if expression>", bsz);
		break;

	case cel_exp_vardecl:
		break;
	}
}

cel_expr_t *
cel_make_any(t)
	cel_type_t	*t;
{
	switch (t->ct_tag) {
	case cel_type_int8:	return cel_make_int8(0);
	case cel_type_uint8:	return cel_make_uint8(0);
	case cel_type_int16:	return cel_make_int16(0);
	case cel_type_uint16:	return cel_make_uint16(0);
	case cel_type_int32:	return cel_make_int32(0);
	case cel_type_uint32:	return cel_make_uint32(0);
	case cel_type_int64:	return cel_make_int64(0);
	case cel_type_uint64:	return cel_make_uint64(0);
	case cel_type_bool:	return cel_make_bool(0);
	case cel_type_string:	return cel_make_string("");
	case cel_type_void:	return cel_make_void();
	case cel_type_array:
	case cel_type_function:
		return NULL;
	}

	return NULL;
}

cel_expr_t *
cel_make_void()
{
cel_expr_t	*ret;
	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;
	ret->ce_tag = cel_exp_void;
	ret->ce_type = cel_make_type(cel_type_void);
	return ret;
}

void
cel_expr_assign(l, r)
	cel_expr_t	*l, *r;
{
	assert(l->ce_type->ct_tag == r->ce_type->ct_tag);

	switch (l->ce_type->ct_tag) {
	case cel_type_int8:
	case cel_type_uint8:
	case cel_type_int16:
	case cel_type_uint16:
	case cel_type_int32:
	case cel_type_uint32:
	case cel_type_int64:
	case cel_type_uint64:
	case cel_type_bool:
		l->ce_op.ce_int64 = r->ce_op.ce_int64;
		return;

	case cel_type_string:
		free(l->ce_op.ce_string);
		l->ce_op.ce_string = strdup(r->ce_op.ce_string);
		return;

	default:
		abort();
		;
	}
}

cel_expr_t *
cel_expr_convert(v, t)
	cel_expr_t	*v;
	cel_type_t	*t;
{
	if (v->ce_type->ct_tag == t->ct_tag)
		return cel_expr_copy(v);

	switch (t->ct_tag) {
	case cel_type_int8:
		switch (v->ce_type->ct_tag) {
		case cel_type_int8:	return cel_make_int8(v->ce_op.ce_int8);
		case cel_type_uint8:	return cel_make_int8(v->ce_op.ce_int8);
		case cel_type_int16:	return cel_make_int8(v->ce_op.ce_int16);
		case cel_type_uint16:	return cel_make_int8(v->ce_op.ce_int16);
		case cel_type_int32:	return cel_make_int8(v->ce_op.ce_int32);
		case cel_type_uint32:	return cel_make_int8(v->ce_op.ce_int32);
		case cel_type_int64:	return cel_make_int8(v->ce_op.ce_int64);
		case cel_type_uint64:	return cel_make_int8(v->ce_op.ce_int64);
		default:		return NULL;
		}

	case cel_type_uint8:
		switch (v->ce_type->ct_tag) {
		case cel_type_int8:	return cel_make_uint8(v->ce_op.ce_int8);
		case cel_type_uint8:	return cel_make_uint8(v->ce_op.ce_int8);
		case cel_type_int16:	return cel_make_uint8(v->ce_op.ce_int16);
		case cel_type_uint16:	return cel_make_uint8(v->ce_op.ce_int16);
		case cel_type_int32:	return cel_make_uint8(v->ce_op.ce_int32);
		case cel_type_uint32:	return cel_make_uint8(v->ce_op.ce_int32);
		case cel_type_int64:	return cel_make_uint8(v->ce_op.ce_int64);
		case cel_type_uint64:	return cel_make_uint8(v->ce_op.ce_int64);
		default:		return NULL;
		}

	case cel_type_int16:
		switch (v->ce_type->ct_tag) {
		case cel_type_int8:	return cel_make_int16(v->ce_op.ce_int8);
		case cel_type_uint8:	return cel_make_int16(v->ce_op.ce_int8);
		case cel_type_int16:	return cel_make_int16(v->ce_op.ce_int16);
		case cel_type_uint16:	return cel_make_int16(v->ce_op.ce_int16);
		case cel_type_int32:	return cel_make_int16(v->ce_op.ce_int32);
		case cel_type_uint32:	return cel_make_int16(v->ce_op.ce_int32);
		case cel_type_int64:	return cel_make_int16(v->ce_op.ce_int64);
		case cel_type_uint64:	return cel_make_int16(v->ce_op.ce_int64);
		default:		return NULL;
		}

	case cel_type_uint16:
		switch (v->ce_type->ct_tag) {
		case cel_type_int8:	return cel_make_uint16(v->ce_op.ce_int8);
		case cel_type_uint8:	return cel_make_uint16(v->ce_op.ce_int8);
		case cel_type_int16:	return cel_make_uint16(v->ce_op.ce_int16);
		case cel_type_uint16:	return cel_make_uint16(v->ce_op.ce_int16);
		case cel_type_int32:	return cel_make_uint16(v->ce_op.ce_int32);
		case cel_type_uint32:	return cel_make_uint16(v->ce_op.ce_int32);
		case cel_type_int64:	return cel_make_uint16(v->ce_op.ce_int64);
		case cel_type_uint64:	return cel_make_uint16(v->ce_op.ce_int64);
		default:		return NULL;
		}

	case cel_type_int32:
		switch (v->ce_type->ct_tag) {
		case cel_type_int8:	return cel_make_int32(v->ce_op.ce_int8);
		case cel_type_uint8:	return cel_make_int32(v->ce_op.ce_int8);
		case cel_type_int16:	return cel_make_int32(v->ce_op.ce_int16);
		case cel_type_uint16:	return cel_make_int32(v->ce_op.ce_int16);
		case cel_type_int32:	return cel_make_int32(v->ce_op.ce_int32);
		case cel_type_uint32:	return cel_make_int32(v->ce_op.ce_int32);
		case cel_type_int64:	return cel_make_int32(v->ce_op.ce_int64);
		case cel_type_uint64:	return cel_make_int32(v->ce_op.ce_int64);
		default:		return NULL;
		}

	case cel_type_uint32:
		switch (v->ce_type->ct_tag) {
		case cel_type_int8:	return cel_make_uint32(v->ce_op.ce_int8);
		case cel_type_uint8:	return cel_make_uint32(v->ce_op.ce_int8);
		case cel_type_int16:	return cel_make_uint32(v->ce_op.ce_int16);
		case cel_type_uint16:	return cel_make_uint32(v->ce_op.ce_int16);
		case cel_type_int32:	return cel_make_uint32(v->ce_op.ce_int32);
		case cel_type_uint32:	return cel_make_uint32(v->ce_op.ce_int32);
		case cel_type_int64:	return cel_make_uint32(v->ce_op.ce_int64);
		case cel_type_uint64:	return cel_make_uint32(v->ce_op.ce_int64);
		default:		return NULL;
		}

	case cel_type_int64:
		switch (v->ce_type->ct_tag) {
		case cel_type_int8:	return cel_make_int64(v->ce_op.ce_int8);
		case cel_type_uint8:	return cel_make_int64(v->ce_op.ce_int8);
		case cel_type_int16:	return cel_make_int64(v->ce_op.ce_int16);
		case cel_type_uint16:	return cel_make_int64(v->ce_op.ce_int16);
		case cel_type_int32:	return cel_make_int64(v->ce_op.ce_int32);
		case cel_type_uint32:	return cel_make_int64(v->ce_op.ce_int32);
		case cel_type_int64:	return cel_make_int64(v->ce_op.ce_int64);
		case cel_type_uint64:	return cel_make_int64(v->ce_op.ce_int64);
		default:		return NULL;
		}

	case cel_type_uint64:
		switch (v->ce_type->ct_tag) {
		case cel_type_int8:	return cel_make_uint64(v->ce_op.ce_int8);
		case cel_type_uint8:	return cel_make_uint64(v->ce_op.ce_int8);
		case cel_type_int16:	return cel_make_uint64(v->ce_op.ce_int16);
		case cel_type_uint16:	return cel_make_uint64(v->ce_op.ce_int16);
		case cel_type_int32:	return cel_make_uint64(v->ce_op.ce_int32);
		case cel_type_uint32:	return cel_make_uint64(v->ce_op.ce_int32);
		case cel_type_int64:	return cel_make_uint64(v->ce_op.ce_int64);
		case cel_type_uint64:	return cel_make_uint64(v->ce_op.ce_int64);
		default:		return NULL;
		}

	default:
		return NULL;
	}

	return NULL;
}
