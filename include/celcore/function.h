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

struct cel_block;
struct cel_type;

typedef struct cel_function {
	wchar_t			 *cf_name;
	struct cel_type		**cf_args;
	int			  cf_nargs;
	struct cel_block	 *cf_body;
} cel_function_t;

void	cel_function_free(cel_function_t *);

#endif	/* !CEL_FUNCTION_H */
