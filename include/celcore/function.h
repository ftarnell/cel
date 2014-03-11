/*
 * CEL: The Compact Embeddable Language.
 * Copyright (c) 2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#ifndef	CEL_FUNCTION_H
#define	CEL_FUNCTION_H

#include	"celcore/expr.h"
#include	"celcore/type.h"

struct cel_type;
struct cel_scope;

typedef struct cel_function {
	char			 *cf_name;
	struct cel_type		**cf_args;
	int			  cf_nargs;
	cel_expr_list_t		  cf_body;
	cel_type_t		 *cf_return_type;
	struct cel_scope	 *cf_scope;
	struct cel_scope	 *cf_argscope;
} cel_function_t;

void	cel_function_free(cel_function_t *);

#endif	/* !CEL_FUNCTION_H */
