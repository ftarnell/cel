/*
 * CEL: The Compact Embeddable Language.
 * Copyright (c) 2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#ifndef	CEL_VARIABLE_H
#define	CEL_VARIABLE_H

#include	"celcore/type.h"
#include	"celcore/expr.h"

typedef struct cel_vardecl {
	cel_type_t	*cv_type;
	wchar_t		**cv_names;
	int		 cv_nnames;
} cel_vardecl_t;

cel_expr_t	*cel_make_vardecl(cel_vardecl_t *);

void	cel_vardecl_free(cel_vardecl_t *);

#endif	/* !CEL_VARIABLE_H */
