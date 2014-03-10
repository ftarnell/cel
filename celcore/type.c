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

#include	"celcore/type.h"

cel_type_t *
cel_make_type(t)
	cel_type_tag_t	t;
{
cel_type_t	*ret;
	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;
	ret->ct_tag = t;
	return ret;
}

cel_type_t *
cel_make_array(t)
	cel_type_t	*t;
{
cel_type_t	*ret;
	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;
	ret->ct_tag = cel_type_array;
	ret->ct_type.ct_array_type = t;
	return ret;
}

cel_typedef_t *
cel_make_typedef(name, type)
	wchar_t const	*name;
	cel_type_t	*type;
{
cel_typedef_t	*ret;
	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;
	ret->ct_type = type;
	ret->ct_name = wcsdup(name);
	return ret;
}

void
cel_type_free(t)
	cel_type_t	*t;
{
	if (t->ct_tag == cel_type_array)
		cel_type_free(t->ct_type.ct_array_type);
	free(t);
}
