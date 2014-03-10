/*
 * CEL: The Compact Embeddable Language.
 * Copyright (c) 2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#include	<wchar.h>
#include	<stdlib.h>

#include	"celcore/function.h"
#include	"celcore/type.h"

void
cel_function_free(f)
	cel_function_t	*f;
{
	free(f->cf_name);
	while (f->cf_nargs--)
		cel_type_free(f->cf_args[f->cf_nargs]);
}
