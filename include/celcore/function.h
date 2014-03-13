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
#include	"celcore/cel-config.h"

struct cel_type;
struct cel_scope;
struct cel_vm_func;

typedef struct cel_function {
	char			 *cf_name;
	struct cel_type		 *cf_type;
	struct cel_type		**cf_args;
	int			  cf_nargs;
	cel_type_t		 *cf_return_type;
	int			  cf_extern;
#ifdef	CEL_HAVE_FFI
	void			 *cf_ffi;
	void			 *cf_ptr;
#endif
	struct cel_vm_func	 *cf_bytecode;
	cel_expr_list_t		  cf_body;
	struct cel_scope	 *cf_scope;
	struct cel_scope	 *cf_argscope;
} cel_function_t;

void	cel_function_free(cel_function_t *);

#endif	/* !CEL_FUNCTION_H */
