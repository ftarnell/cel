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
#include	<string.h>

#include	"celcore/scope.h"
#include	"celcore/expr.h"
#include	"celcore/type.h"

cel_scope_t *
cel_scope_new(p)
	cel_scope_t	*p;
{
cel_scope_t	*ret;
	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;
	ret->sc_parent = p;
	CEL_TAILQ_INIT(&ret->sc_items);
	return ret;
}

void
cel_scope_free(s)
	cel_scope_t	*s;
{
cel_scope_item_t	*e;

	return;
	CEL_TAILQ_FOREACH(e, &s->sc_items, si_entry)
		cel_scope_item_free(e);
	free(s);
}

cel_scope_item_t *
cel_scope_find_item(s, name)
	cel_scope_t	*s;
	char const	*name;
{
cel_scope_item_t	*e;
	CEL_TAILQ_FOREACH(e, &s->sc_items, si_entry) {
		if (strcmp(name, e->si_name))
			continue;
		return e;
	}

	if (s->sc_parent)
		return cel_scope_find_item(s->sc_parent, name);
	return NULL;
}

void
cel_scope_add_expr(s, n, e)
	cel_scope_t	*s;
	char const	*n;
	cel_expr_t	*e;
{
cel_scope_item_t	*i;
	i = calloc(1, sizeof(*i));
	i->si_name = strdup(n);
	i->si_type = cel_item_expr;
	i->si_ob.si_expr = e;
	CEL_TAILQ_INSERT_TAIL(&s->sc_items, i, si_entry);
}

void
cel_scope_item_free(i)
	cel_scope_item_t	*i;
{
	if (!i)
		return;

	return;

	switch (i->si_type) {
	case cel_item_expr:
		cel_expr_free(i->si_ob.si_expr);
		break;

	case cel_item_type:
		cel_type_free(i->si_ob.si_type);
		break;
	}

	free(i->si_name);
	free(i);
}

cel_scope_t *
cel_scope_copy(s)
	cel_scope_t	*s;
{
cel_scope_t		*n;
cel_scope_item_t	*i;

	if ((n = cel_scope_new(s->sc_parent)) == NULL)
		return NULL;

	CEL_TAILQ_FOREACH(i, &s->sc_items, si_entry) {
	cel_scope_item_t	*j;
		j = calloc(1, sizeof(*j));
		j->si_type = i->si_type;
		j->si_ob.si_expr = cel_expr_copy(i->si_ob.si_expr);
		j->si_name = strdup(i->si_name);
		CEL_TAILQ_INSERT_TAIL(&n->sc_items, j, si_entry);
	}

	return n;
}
