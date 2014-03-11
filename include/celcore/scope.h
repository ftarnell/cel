/*
 * CEL: The Compact Embeddable Language.
 * Copyright (c) 2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#ifndef	CEL_SCOPE_H
#define	CEL_SCOPE_H

#include	"celcore/tailq.h"

struct cel_variable;
struct cel_function;
struct cel_type;
struct cel_expr;

typedef enum cel_scope_item_type {
	cel_item_expr,
	cel_item_type
} cel_scope_item_type_t;

typedef struct cel_scope_item {
	char			*si_name;
	cel_scope_item_type_t	 si_type;
	union {
		struct cel_expr		*si_expr;
		struct cel_type		*si_type;
	} si_ob;

	CEL_TAILQ_ENTRY(cel_scope_item) si_entry;
} cel_scope_item_t;

typedef CEL_TAILQ_HEAD(cel_scope_item_list, cel_scope_item) cel_scope_item_list_t;

typedef struct cel_scope {
	struct cel_scope	*sc_parent;
	cel_scope_item_list_t	 sc_items;
} cel_scope_t;

cel_scope_t	 *cel_scope_new(cel_scope_t *parent);
void		  cel_scope_free(cel_scope_t *);
cel_scope_item_t *cel_scope_find_item(cel_scope_t *, char const *name);
void		  cel_scope_add_expr(cel_scope_t *, char const *,
				     struct cel_expr *);

void		  cel_scope_item_free(cel_scope_item_t *);

#endif	/* !CEL_SCOPE_H */
