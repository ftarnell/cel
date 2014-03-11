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
cel_make_identifier(s)
	char const	*s;
{
cel_expr_t	*ret;
	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;
	ret->ce_tag = cel_exp_string;
	ret->ce_op.ce_identifier = strdup(s);
	return ret;
}

cel_expr_t *
cel_make_function(f)
	cel_function_t	*f;
{
cel_expr_t	*ret;
	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;
	ret->ce_tag = cel_exp_function;
	ret->ce_op.ce_function = f;
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

void
cel_expr_free(e)
	cel_expr_t	*e;
{
	if (!e)
		return;

	switch (e->ce_tag) {
	case cel_exp_string:
		free(e->ce_op.ce_string);
		break;

	case cel_exp_identifier:
		free(e->ce_op.ce_identifier);
		break;

	case cel_exp_unary:
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
		ret->ce_op.ce_unary.operand = cel_expr_copy(e->ce_op.ce_unary.operand);
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
	case cel_exp_vardecl:
	case cel_exp_identifier:
	case cel_exp_if:
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
	*b = 0;
	strlcat(b, "<error>", bsz);

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

	case cel_exp_unary:
	case cel_exp_binary:
	case cel_exp_function:
	case cel_exp_if:
	case cel_exp_vardecl:
	case cel_exp_identifier:
		break;
	}
}
