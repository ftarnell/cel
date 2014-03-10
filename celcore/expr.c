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
#include	"celcore/function.h"
#include	"celcore/type.h"

cel_expr_t *
cel_make_int32(i)
{
cel_expr_t	*ret;
	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;
	ret->ce_type = cel_make_type(cel_type_int32);
	ret->ce_tag = cel_exp_int;
	ret->ce_op.ce_int = i;
	return ret;
}

cel_expr_t *
cel_make_bool(i)
{
cel_expr_t	*ret;
	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;
	ret->ce_type = cel_make_type(cel_type_bool);
	ret->ce_tag = cel_exp_int;
	ret->ce_op.ce_bool = i;
	return ret;
}

cel_expr_t *
cel_make_string(s)
	wchar_t const	*s;
{
cel_expr_t	*ret;
	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;
	ret->ce_type = cel_make_type(cel_type_string);
	ret->ce_tag = cel_exp_string;
	ret->ce_op.ce_string = wcsdup(s);
	return ret;
}

cel_expr_t *
cel_make_identifier(s)
	wchar_t const	*s;
{
cel_expr_t	*ret;
	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;
	ret->ce_tag = cel_exp_string;
	ret->ce_op.ce_identifier = wcsdup(s);
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
	ret->ce_tag = cel_exp_binary;
	ret->ce_op.ce_unary.oper = op;
	ret->ce_op.ce_unary.operand = e;
	return ret;
}

void
cel_expr_free(e)
	cel_expr_t	*e;
{
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

	case cel_exp_int:
	case cel_exp_bool:
		break;
	}
}
