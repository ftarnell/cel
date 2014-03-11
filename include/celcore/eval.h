/*
 * CEL: The Compact Embeddable Language.
 * Copyright (c) 2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#ifndef	CEL_EVAL_H
#define	CEL_EVAL_H

#include	"celcore/expr.h"

struct cel_scope;

cel_expr_t	*cel_eval(struct cel_scope *, cel_expr_t *);
cel_expr_t	*cel_eval_list(struct cel_scope *, cel_expr_list_t *);

#endif	/* !CEL_EVAL_H */
