/*
 * CEL: The Compact Embeddable Language.
 * Copyright (c) 2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#include	<stdlib.h>

#include	"celcore/variable.h"

void
cel_vardecl_free(vd)
	cel_vardecl_t	*vd;
{
	cel_type_free(vd->cv_type);
	while (vd->cv_nnames--)
		free(vd->cv_names[vd->cv_nnames]);
}

