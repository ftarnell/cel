/*
 * CEL: The Compact Embeddable Language.
 * Copyright (c) 2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#include	"celcore/eval.h"

static cel_expr_t *
cel_eval_unary(e)
	cel_expr_t	*e;
{
	return NULL;
}

static cel_expr_t *
cel_eval_binary(e)
	cel_expr_t	*e;
{
cel_expr_t	*l, *r;

	if ((l = cel_eval(e->ce_op.ce_binary.left)) == NULL)
		return NULL;
	if ((r = cel_eval(e->ce_op.ce_binary.right)) == NULL) {
		cel_expr_free(l);
		return NULL;
	}

	switch (e->ce_op.ce_binary.oper) {
	case cel_op_plus:
		return cel_make_int32(l->ce_op.ce_int + r->ce_op.ce_int);

	}

	return NULL;
}

cel_expr_t *
cel_eval(e)
	cel_expr_t	*e;
{
	switch (e->ce_tag) {
	case cel_exp_int:
	case cel_exp_string:
	case cel_exp_bool:
		return cel_expr_copy(e);

	case cel_exp_unary:
		return cel_eval_unary(e);
	
	case cel_exp_binary:
		return cel_eval_binary(e);

/*
	cel_exp_identifier,
	cel_exp_function,
	cel_exp_vardecl,
	cel_exp_if
*/
	}

	return NULL;
}

cel_expr_t *
cel_eval_list(l)
	cel_expr_list_t	*l;
{
cel_expr_t	*e = NULL;
cel_expr_t	*ret = NULL;

	CEL_TAILQ_FOREACH(e, l, ce_entry) {
	cel_expr_t	*r;

		if ((r = cel_eval(e)) == NULL) {
			cel_expr_free(ret);
			return NULL;
		}

		cel_expr_free(ret);
		ret = r;
	}

	return ret;
}
