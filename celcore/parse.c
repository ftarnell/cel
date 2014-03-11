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

#include	"celcore/parse.h"
#include	"celcore/tokens.h"
#include	"celcore/expr.h"
#include	"celcore/type.h"
#include	"celcore/variable.h"
#include	"celcore/function.h"
#include	"celcore/block.h"
#include	"celcore/scope.h"

#define	EXPECT(t)	(par->cp_tok.ct_token == (t))
#define	CONSUME()	cel_next_token(par->cp_lex, &par->cp_tok)
#define	ACCEPT(t)	(EXPECT((t)) ? (CONSUME(), t) : 0)

#define	ERROR_TOK(t,m)							\
	do {								\
		if (par->cp_error)					\
			par->cp_error(par, (t), (m));			\
		++par->cp_nerrs;					\
		return NULL;						\
	} while (0)

#define	ERROR(m)	ERROR_TOK(&par->cp_tok, (m))

#define	WARN(m)								\
	do {								\
		if (par->cp_warn)					\
			par->cp_warn(par, m);				\
		++par->cp_warns;					\
		return NULL;						\
	} while (0)

static cel_expr_t	*cel_parse_func(cel_parser_t *);
static cel_expr_t	*cel_parse_var(cel_parser_t *);
static cel_typedef_t	*cel_parse_typedef(cel_parser_t *);
static cel_type_t	*cel_parse_type(cel_parser_t *);
static cel_expr_t	*cel_parse_stmt(cel_parser_t *);
static cel_expr_t	*cel_parse_expr(cel_parser_t *);
static cel_expr_t	*cel_parse_expr_assign(cel_parser_t *);
static cel_expr_t	*cel_parse_expr_or(cel_parser_t *);
static cel_expr_t	*cel_parse_expr_and(cel_parser_t *);
static cel_expr_t	*cel_parse_expr_xor(cel_parser_t *);
static cel_expr_t	*cel_parse_expr_eq1(cel_parser_t *);
static cel_expr_t	*cel_parse_expr_eq2(cel_parser_t *);
static cel_expr_t	*cel_parse_expr_plus(cel_parser_t *);
static cel_expr_t	*cel_parse_expr_mult(cel_parser_t *);
static cel_expr_t	*cel_parse_expr_unary(cel_parser_t *);
static cel_expr_t	*cel_parse_expr_post(cel_parser_t *);
static cel_expr_t	*cel_parse_expr_value(cel_parser_t *);
static cel_expr_t	*cel_parse_value(cel_parser_t *);
static cel_arglist_t	*cel_parse_arglist(cel_parser_t *);
static cel_expr_t	*cel_parse_if(cel_parser_t *);

cel_parser_t *
cel_parser_new(lex, scope)
	cel_lexer_t	*lex;
	cel_scope_t	*scope;
{
cel_parser_t	*par;
	if ((par = calloc(1, sizeof(*par))) == NULL)
		return NULL;

	memset(par, 0, sizeof(*par));
	par->cp_lex = lex;
	par->cp_scope = scope;
	return par;
}

cel_expr_list_t *
cel_parse(par)
	cel_parser_t	*par;
{
/*
 * Top-level parser.  The only things which can occur at the top level are
 * functions, typedefs or variables.
 */
cel_expr_list_t	*list;
cel_expr_t	*e;
cel_token_t	 start_tok;

	list = calloc(1, sizeof(*list));
	CEL_TAILQ_INIT(list);

	CONSUME();

	for (;;) {
		start_tok = par->cp_tok;

		if ((e = cel_parse_expr(par)) == NULL)
			break;

		CEL_TAILQ_INSERT_TAIL(list, e, ce_entry);

		if (!ACCEPT(T_SEMI)) {
			/*
			 * The last statement in the file doesn't need a
			 * semicolon.
			 */
			if (EXPECT(T_EOT))
				return list;

			ERROR("expected ';'");
		}
	}

	if (EXPECT(T_EOT))
		return list;

	ERROR_TOK(&start_tok, "failed to parse statement");
}

cel_typedef_t *
cel_parse_typedef(par)
	cel_parser_t	*par;
{
/*
 * A typedef consists of a comma-separated list of identifiers, a colon, a 
 * type, and a semicolon.  Example:
 *
 *	type a : int;
 *	type b, c : string[];
 */
cel_type_t	*type = NULL;
char		*name;

	if (!ACCEPT(T_TYPE))
		return NULL;

/* Identifier list, colon */
	for (;;) {
		if (!ACCEPT(T_ID))
			ERROR("expected identifier");
		name = strdup(par->cp_tok.ct_literal);

		if (ACCEPT(T_COMMA))
			continue;

		if (ACCEPT(T_COLON))
			break;

		free(name);
		ERROR("expected ',' or ';'");
	}

/* Type */
	if ((type = cel_parse_type(par)) == 0) {
		free(name);
		ERROR("expected type name");
	}

	return cel_make_typedef(name, type);
}

struct vard {
	char		*name;
	cel_expr_t	*init;
	cel_token_t	 err_tok;
};

static void
free_varvec(v, n)
	struct vard	*v;
	size_t		 n;
{
	while (n--)
		free(v[n].name);
	free(v);
}

cel_expr_t *
cel_parse_var(par)
	cel_parser_t	*par;
{
/*
 * A variable definition consists of a comma-separated list of identifiers,
 * a colon, a type, and a semicolon.  Example:
 *
 *	var a : int;
 *	var b, c : []string;
 */
struct vard	*names = NULL;
size_t		 nnames = 0, i;
cel_type_t	*type = NULL;

	if (!EXPECT(T_VAR))
		return NULL;
	CONSUME();

/* Identifier list, colon */
	for (;;) {
	cel_expr_t	*e = NULL;
	char		*name;
	cel_token_t	 err_tok;

		if (!EXPECT(T_ID)) {
			free_varvec(names, nnames);
			ERROR("expected identifier");
		}

		if (cel_scope_find_item(par->cp_scope, par->cp_tok.ct_literal)) {
		char		err[128];
		cel_token_t	err_tok = par->cp_tok;

			snprintf(err, sizeof(err), "symbol \"%s\" already declared",
				 par->cp_tok.ct_literal);
			CONSUME();
			ERROR_TOK(&err_tok, err);
		}

		name = strdup(par->cp_tok.ct_literal);
		CONSUME();

		if (ACCEPT(T_ASSIGN)) {
			err_tok = par->cp_tok;

			if ((e = cel_parse_expr(par)) == NULL) {
				free(name);
				ERROR("expected expression");
			}
		}

		names = realloc(names, (nnames + 1) * sizeof(struct vard));
		names[nnames].name = name;
		names[nnames].init = e;
		names[nnames].err_tok = err_tok;
		nnames++;

		if (ACCEPT(T_COMMA))
			continue;
		else
			break;
	}

/* Type */
	if (ACCEPT(T_COLON)) {
		if ((type = cel_parse_type(par)) == NULL) {
			free_varvec(names, nnames);
			ERROR("expected type name");
		}
	}

	for (i = 0; i < nnames; i++) {
	cel_expr_t	*e;
	cel_type_t	*atype = type;
	cel_token_t	 err_tok = names[i].err_tok;

		if (!type && !names[i].init) {
			free_varvec(names, nnames);
			ERROR_TOK(&err_tok, "variable must have either initialiser or type");
		}
			
		if (!atype)
			atype = names[i].init->ce_type;

		e = cel_make_any(atype);
		e->ce_mutable = 1;

		if (names[i].init) {
			if (!cel_type_convertable(atype, names[i].init->ce_type)) {
			char	a1[64], a2[64];
			char	err[128];

				cel_name_type(atype, a1, sizeof(a1));
				cel_name_type(names[i].init->ce_type, a2, sizeof(a2));

				snprintf(err, sizeof(err) / sizeof(char),
					 "incompatible types in initialisation: \"%s\" := \"%s\"",
					 a1, a2);

				free_varvec(names, nnames);
				cel_expr_free(e);
				ERROR_TOK(&err_tok, err);
			}
			cel_expr_assign(e, names[i].init);
		}

		cel_scope_add_expr(par->cp_scope, names[i].name, e);
	}
	free_varvec(names, nnames);
	
	if (type)
		cel_type_free(type);
	return cel_make_void();
}

cel_type_t *
cel_parse_type(par)
	cel_parser_t	*par;
{
/*
 * A type consists of an optional array specifier, [], followed by an
 * identifier (the type name).  At this point, we don't need to check if
 * the type name is valid;
 */

int		 array = 0;
cel_type_t	*type;

/* Optional array specifier */
	while (ACCEPT(T_LSQ)) {
		if (!ACCEPT(T_RSQ))
			ERROR("expected ']'");
		array++;
	}

/* Identifier */
	if (ACCEPT(T_INT))		type = cel_make_type(cel_type_int32);
	else if (ACCEPT(T_INT8))	type = cel_make_type(cel_type_int8);
	else if (ACCEPT(T_UINT8))	type = cel_make_type(cel_type_uint8);
	else if (ACCEPT(T_INT16))	type = cel_make_type(cel_type_int16);
	else if (ACCEPT(T_UINT16))	type = cel_make_type(cel_type_uint16);
	else if (ACCEPT(T_INT32))	type = cel_make_type(cel_type_int32);
	else if (ACCEPT(T_UINT32))	type = cel_make_type(cel_type_uint32);
	else if (ACCEPT(T_INT64))	type = cel_make_type(cel_type_int64);
	else if (ACCEPT(T_UINT64))	type = cel_make_type(cel_type_uint64);
	else if (ACCEPT(T_STRING))	type = cel_make_type(cel_type_string);
	else if (ACCEPT(T_BOOL))	type = cel_make_type(cel_type_bool);
	else
		return NULL;

	while (array--)
		type = cel_make_array(type);
	
	return type;
}

cel_expr_t *
cel_parse_func(par)
	cel_parser_t	*par;
{
/*
 * A function consists of a prototype, 'begin', a series of statements, and
 * an 'end'.
 *
 * The prototype consists of an identifier, a colon, a '(', a list of
 * comma-separated "identifier : type" pairs, a ')', a '->' and a type.
 */

cel_function_t	*func;

	if (!ACCEPT(T_FUNC))
		return NULL;

	func = calloc(1, sizeof(*func));

/* Identifier (function name) */
	if (!ACCEPT(T_ID)) {
		cel_function_free(func);
		ERROR("expected identifier");
	}

	func->cf_name = strdup(par->cp_tok.ct_literal);

/* Colon */
	if (!ACCEPT(T_COLON)) {
		cel_function_free(func);
		ERROR("expected ':'");
	}

/* Opening bracket */
	if (!ACCEPT(T_LPAR)) {
		cel_function_free(func);
		ERROR("expected ')'");
	}

/* Argument list */
	for (;;) {
	/* Argument name */
		if (!ACCEPT(T_ID)) {
			cel_function_free(func);
			ERROR("expected identifier");
		}

	/* Colon */
		if (!ACCEPT(T_COLON)) {
			cel_function_free(func);
			ERROR("expected ':'");
		}

	/* Type */
		if (cel_parse_type(par) == NULL) {
			cel_function_free(func);
			ERROR("expected type name");
		}

	/* ')' or ',' */
		if (ACCEPT(T_RPAR))
			break;
		if (ACCEPT(T_COMMA))
			continue;

		cel_function_free(func);
		ERROR("expected ',' or ')'");
	}

/* -> */
	if (!ACCEPT(T_ARROW)) {
		cel_function_free(func);
		ERROR("expected '->'");
	}

/* Return type */
	if (cel_parse_type(par) == NULL) {
		cel_function_free(func);
		ERROR("expected type name");
	}

/* Begin */
	if (!ACCEPT(T_BEGIN)) {
		cel_function_free(func);
		ERROR("expected 'begin'");
	}

/*
 * List of statements.
 */
	while (cel_parse_stmt(par) == 0) {
		if (ACCEPT(T_SEMI))
			continue;
	}

	if (!ACCEPT(T_END)) {
		cel_function_free(func);
		return NULL;
	}

	return cel_make_function(func);
}

cel_expr_t *
cel_parse_stmt(par)
	cel_parser_t	*par;
{
/*
 * Statement handling.  The definition of 'statement' is fairly wide and
 * includes anything that can occur within a function, including variable
 * definitions, control constructs (if) and function calls.  'end' is
 * handled as a special type of statement; we return '2' to distinguish
 * that, as it relates to parsing rather than semantics.
 */
cel_expr_t	*e;

	if (e = cel_parse_expr(par))
		return e;


	return NULL;
}

cel_expr_t *
cel_parse_expr(par)
	cel_parser_t	*par;
{
	return cel_parse_expr_assign(par);
}

cel_expr_t *
cel_parse_expr_assign(par)
	cel_parser_t	*par;
{
cel_expr_t	*e, *f;
cel_token_t	 lv_tok, op_tok;
int		 op;

	lv_tok = par->cp_tok;

	if ((e = cel_parse_expr_or(par)) == NULL)
		return NULL;

	for (;;) {
	cel_type_t	*type;
		op_tok = par->cp_tok;

		if (!(op = ACCEPT(T_ASSIGN)))
			break;

		if ((f = cel_parse_expr_assign(par)) == NULL) {
			cel_expr_free(e);
			ERROR("expected expression");
		}

		if (!e->ce_mutable)
			ERROR_TOK(&op_tok, "assignment to read-only location");

		if (!cel_type_convertable(e->ce_type, f->ce_type)) {
		char	a1[64], a2[64];
		char	err[128];

			cel_name_type(e->ce_type, a1, sizeof(a1) / sizeof(char));
			cel_name_type(f->ce_type, a2, sizeof(a2) / sizeof(char));

			snprintf(err, sizeof(err) / sizeof(char),
				 "incompatible types in assignment: \"%s\" := \"%s\"",
				 a1, a2);

			ERROR_TOK(&op_tok, err);
		}

		type = e->ce_type;
		e = cel_make_assign(e, f);
		e->ce_type = type;
	}

	return e;
}

cel_expr_t *
cel_parse_expr_or(par)
	cel_parser_t	*par;
{
cel_expr_t	*e, *f;
cel_token_t	 op_tok;
int		 op;

	if ((e = cel_parse_expr_and(par)) == NULL)
		return NULL;

	for (;;) {
	cel_type_t	*type;

		op_tok = par->cp_tok;

		if (!(op = ACCEPT(T_OR)))
			break;

		if ((f = cel_parse_expr_and(par)) == NULL) {
			cel_expr_free(e);
			ERROR("expected expression");
		}

		if ((type = cel_derive_binary_type(cel_op_or, e->ce_type, f->ce_type)) == NULL) {
		char	a1[64], a2[64];
		char	err[128];

			cel_name_type(e->ce_type, a1, sizeof(a1) / sizeof(char));
			cel_name_type(f->ce_type, a2, sizeof(a2) / sizeof(char));

			snprintf(err, sizeof(err) / sizeof(char),
				 "incompatible types in expression: \"%s\", \"%s\"",
				 a1, a2);

			cel_expr_free(e);
			cel_expr_free(f);
			ERROR_TOK(&op_tok, err);
		}

		e = cel_make_or(e, f);
		e->ce_type = type;
	}

	return e;
}

cel_expr_t *
cel_parse_expr_and(par)
	cel_parser_t	*par;
{
cel_expr_t	*e, *f;
cel_token_t	 op_tok;
int		 op;

	if ((e = cel_parse_expr_xor(par)) == NULL)
		return NULL;

	for (;;) {
	cel_type_t	*type;

		op_tok = par->cp_tok;

		if (!(op = ACCEPT(T_AND)))
			break;

		if ((f = cel_parse_expr_xor(par)) == NULL) {
			cel_expr_free(e);
			ERROR("expected expression");
		}

		if ((type = cel_derive_binary_type(cel_op_and, e->ce_type, f->ce_type)) == NULL) {
		char	a1[64], a2[64];
		char	err[128];

			cel_name_type(e->ce_type, a1, sizeof(a1) / sizeof(char));
			cel_name_type(f->ce_type, a2, sizeof(a2) / sizeof(char));

			snprintf(err, sizeof(err) / sizeof(char),
				 "incompatible types in expression: \"%s\", \"%s\"",
				 a1, a2);

			cel_expr_free(e);
			cel_expr_free(f);
			ERROR_TOK(&op_tok, err);
		}

		e = cel_make_and(e, f);
		e->ce_type = type;
	}

	return e;
}

cel_expr_t *
cel_parse_expr_xor(par)
	cel_parser_t	*par;
{
cel_expr_t	*e, *f;
cel_token_t	 op_tok;
int		 op;

	if ((e = cel_parse_expr_eq1(par)) == NULL)
		return NULL;

	for (;;) {
	cel_type_t	*type;

		op_tok = par->cp_tok;

		if (!(op = ACCEPT(T_CARET)))
			break;

		if ((f = cel_parse_expr_eq1(par)) == NULL) {
			cel_expr_free(e);
			ERROR("expected expression");
		}

		if ((type = cel_derive_binary_type(cel_op_xor, e->ce_type, f->ce_type)) == NULL) {
		char	a1[64], a2[64];
		char	err[128];

			cel_name_type(e->ce_type, a1, sizeof(a1) / sizeof(char));
			cel_name_type(f->ce_type, a2, sizeof(a2) / sizeof(char));

			snprintf(err, sizeof(err) / sizeof(char),
				 "incompatible types in expression: \"%s\", \"%s\"",
				 a1, a2);

			cel_expr_free(e);
			cel_expr_free(f);
			ERROR_TOK(&op_tok, err);
		}

		e = cel_make_xor(e, f);
		e->ce_type = type;
	}

	return e;
}

cel_expr_t *
cel_parse_expr_eq1(par)
	cel_parser_t	*par;
{
cel_expr_t	*e, *f;
cel_token_t	 op_tok;
int		 op;

	if ((e = cel_parse_expr_eq2(par)) == NULL)
		return NULL;

	for (;;) {
	cel_type_t	*type;
	cel_bi_oper_t	 oper;

		op_tok = par->cp_tok;

		if (!(op = ACCEPT(T_EQ)) && !(op = ACCEPT(T_NEQ)))
			break;

		if ((f = cel_parse_expr_eq2(par)) == NULL) {
			cel_expr_free(e);
			ERROR("expected expression");
		}

		switch (op) {
		case T_EQ:	oper = cel_op_eq; break;
		case T_NEQ:	oper = cel_op_neq; break;
		}

		if ((type = cel_derive_binary_type(oper, e->ce_type, f->ce_type)) == NULL) {
		char	a1[64], a2[64];
		char	err[128];

			cel_name_type(e->ce_type, a1, sizeof(a1) / sizeof(char));
			cel_name_type(f->ce_type, a2, sizeof(a2) / sizeof(char));

			snprintf(err, sizeof(err) / sizeof(char),
				 "incompatible types in expression: \"%s\", \"%s\"",
				 a1, a2);

			cel_expr_free(e);
			cel_expr_free(f);
			ERROR_TOK(&op_tok, err);
		}

		e = cel_make_binary(oper, e, f);
		e->ce_type = type;
	}

	return e;
}

cel_expr_t *
cel_parse_expr_eq2(par)
	cel_parser_t	*par;
{
cel_expr_t	*e, *f;
cel_token_t	 op_tok;
int		 op;

	if ((e = cel_parse_expr_plus(par)) == NULL)
		return NULL;

	for (;;) {
	cel_type_t	*type;
	cel_bi_oper_t	 oper;

		op_tok = par->cp_tok;

		if (!(op = ACCEPT(T_GT)) && !(op = ACCEPT(T_GE)) &&
		    !(op = ACCEPT(T_LT)) && !(op = ACCEPT(T_LE)))
			break;

		if ((f = cel_parse_expr_plus(par)) == NULL) {
			cel_expr_free(e);
			ERROR("expected expression");
		}

		switch (op) {
		case T_GT:	oper = cel_op_gt; break;
		case T_GE:	oper = cel_op_ge; break;
		case T_LT:	oper = cel_op_lt; break;
		case T_LE:	oper = cel_op_le; break;
		}

		if ((type = cel_derive_binary_type(oper, e->ce_type, f->ce_type)) == NULL) {
		char	a1[64], a2[64];
		char	err[128];

			cel_name_type(e->ce_type, a1, sizeof(a1) / sizeof(char));
			cel_name_type(f->ce_type, a2, sizeof(a2) / sizeof(char));

			snprintf(err, sizeof(err) / sizeof(char),
				 "incompatible types in expression: \"%s\", \"%s\"",
				 a1, a2);

			cel_expr_free(e);
			cel_expr_free(f);
			ERROR_TOK(&op_tok, err);
		}

		e = cel_make_binary(oper, e, f);
		e->ce_type = type;
	}

	return e;
}

cel_expr_t *
cel_parse_expr_plus(par)
	cel_parser_t	*par;
{
cel_expr_t	*e, *f;
cel_token_t	 op_tok;
int		 op;

	if ((e = cel_parse_expr_mult(par)) == NULL)
		return NULL;

	for (;;) {
	cel_type_t	*type;
	cel_bi_oper_t	 oper;

		op_tok = par->cp_tok;

		if (!(op = ACCEPT(T_PLUS)) && !(op = ACCEPT(T_MINUS)))
			break;

		if ((f = cel_parse_expr_mult(par)) == NULL) {
			cel_expr_free(e);
			ERROR("expected expression");
		}

		switch (op) {
		case T_PLUS:	oper = cel_op_plus; break;
		case T_MINUS:	oper = cel_op_minus; break;
		}

		if ((type = cel_derive_binary_type(oper, e->ce_type, f->ce_type)) == NULL) {
		char	a1[64], a2[64];
		char	err[128];

			cel_name_type(e->ce_type, a1, sizeof(a1) / sizeof(char));
			cel_name_type(f->ce_type, a2, sizeof(a2) / sizeof(char));

			snprintf(err, sizeof(err) / sizeof(char),
				 "incompatible types in expression: \"%s\", \"%s\"",
				 a1, a2);

			cel_expr_free(e);
			cel_expr_free(f);
			ERROR_TOK(&op_tok, err);
		}

		e = cel_make_binary(oper, e, f);
		e->ce_type = type;
	}

	return e;
}

cel_expr_t *
cel_parse_expr_mult(par)
	cel_parser_t	*par;
{
cel_expr_t	*e, *f;
cel_token_t	 op_tok;
int		 op;

	if ((e = cel_parse_expr_unary(par)) == NULL) {
		return NULL;
	}

	for (;;) {
	cel_type_t	*type;
	cel_bi_oper_t	 oper;

		op_tok = par->cp_tok;

		if (!(op = ACCEPT(T_STAR)) && !(op = ACCEPT(T_SLASH)) &&
		    !(op = ACCEPT(T_PERCENT)))
			break;

		if ((f = cel_parse_expr_unary(par)) == NULL) {
			cel_expr_free(e);
			ERROR("expected expression");
		}

		switch (op) {
		case T_STAR:	oper = cel_op_mult; break;
		case T_SLASH:	oper = cel_op_div; break;
		case T_PERCENT:	oper = cel_op_modulus; break;
		}

		if ((type = cel_derive_binary_type(oper, e->ce_type, f->ce_type)) == NULL) {
		char	a1[64], a2[64];
		char	err[128];

			cel_name_type(e->ce_type, a1, sizeof(a1) / sizeof(char));
			cel_name_type(f->ce_type, a2, sizeof(a2) / sizeof(char));

			snprintf(err, sizeof(err) / sizeof(char),
				 "incompatible types in expression: \"%s\", \"%s\"",
				 a1, a2);

			cel_expr_free(e);
			cel_expr_free(f);
			ERROR_TOK(&op_tok, err);
		}

		e = cel_make_binary(oper, e, f);
		e->ce_type = type;
	}

	return e;
}

cel_expr_t *
cel_parse_expr_unary(par)
	cel_parser_t	*par;
{
/* expr_unary  --> expr_post  | "-" expr_unary | "!" expr_unary */
cel_expr_t	*e = NULL;

	for (;;) {
	cel_type_t	*type;
	cel_uni_oper_t	 oper;
	int		 op;
	cel_token_t	 op_tok;

		op_tok = par->cp_tok;

		if (!(op = ACCEPT(T_MINUS)) && !(op = ACCEPT(T_NEGATE)))
			break;

		if ((e = cel_parse_expr_unary(par)) == NULL) {
			cel_expr_free(e);
			ERROR("expected expression");
		}

		switch (op) {
		case T_NEGATE:	oper = cel_op_negate; break;
		case T_MINUS:	oper = cel_op_uni_minus; break;
		}

		if ((type = cel_derive_unary_type(oper, e->ce_type)) == NULL) {
		char	a1[64], err[128];

			cel_name_type(e->ce_type, a1, sizeof(a1) / sizeof(char));

			snprintf(err, sizeof(err) / sizeof(char),
				 "incompatible type in expression: \"%s\"", a1);

			cel_expr_free(e);
			ERROR_TOK(&op_tok, err);
		}

		e = cel_make_unary(oper, e);
		e->ce_type = type;
	}

	if (e)
		return e;

	return cel_parse_expr_post(par);
}

cel_expr_t *
cel_parse_expr_post(par)
	cel_parser_t	*par;
{
/* expr_post   --> expr_value [ ( "(" arglist ")" | "[" expr "]" ) ] */
cel_expr_t	*e;
int		 op;
cel_token_t	 err_tok;

	if ((e = cel_parse_expr_value(par)) == NULL)
		return NULL;

/* Function call */
	while ((op = ACCEPT(T_LPAR)) || (op = ACCEPT(T_LSQ)) ||
	       (op = ACCEPT(T_AS))) {
	cel_type_t	*t;

		switch (op) {
		case T_LPAR:
			if (cel_parse_arglist(par) == NULL) {
				cel_expr_free(e);
				return NULL;
			}

			if (!ACCEPT(T_RPAR)) {
				cel_expr_free(e);
				ERROR("expected ')'");
			}
			break;

		case T_LSQ:
			if (cel_parse_expr(par) == NULL) {
				cel_expr_free(e);
				ERROR("expected expression");
			}

			if (!ACCEPT(T_RSQ)) {
				cel_expr_free(e);
				ERROR("expected ']'");
			}
			break;

		case T_AS:
			err_tok = par->cp_tok;

			if ((t = cel_parse_type(par)) == NULL) {
				cel_expr_free(e);
				ERROR("expected type");
			}

			if (!cel_type_convertable(e->ce_type, t)) {
			char	a1[64], a2[64];
			char	err[128];

				cel_name_type(e->ce_type, a1, sizeof(a1));
				cel_name_type(t, a2, sizeof(a2));

				snprintf(err, sizeof(err) / sizeof(char),
					 "expression of type \"%s\" cannot be converted to \"%s\"",
					 a1, a2);

				cel_expr_free(e);
				ERROR_TOK(&err_tok, err);
			}

			e = cel_make_cast(e, t);
			break;
		}
	}

	return e;
}

cel_expr_t *
cel_parse_expr_value(par)
	cel_parser_t	*par;
{
/* expr_value  --> value | "(" expr ")" | ifexpr */
cel_expr_t	*e;

	if ((e = cel_parse_value(par)) != NULL)
		return e;

	if (ACCEPT(T_LPAR)) {
		if ((e = cel_parse_expr(par)) == NULL)
			ERROR("expected expression");

		if (!ACCEPT(T_RPAR)) {
			cel_expr_free(e);
			ERROR("expected ')'");
		}

		return e;
	}

/* if expression */
	if (ACCEPT(T_IF)) {
		if ((e = cel_parse_if(par)) == NULL)
			return NULL;
		return e;
	}

/* Function definition */
	if (e = cel_parse_func(par))
		return e;

/* Variable definition */
	if (e = cel_parse_var(par))
		return e;

	return NULL;
}

cel_expr_t *
cel_parse_value(par)
	cel_parser_t	*par;
{
cel_expr_t	*ret = NULL;

	if (EXPECT(T_ID)) {
	cel_scope_item_t	*v;
	cel_token_t		 err_tok = par->cp_tok;

		v = cel_scope_find_item(par->cp_scope,
					par->cp_tok.ct_literal);

		if (v == NULL) {
		char	err[128];
			snprintf(err, sizeof(err), "undeclared identifier \"%s\"",
				 par->cp_tok.ct_literal);
			CONSUME();

			ERROR_TOK(&err_tok, err);
		}

		if (v->si_type != cel_item_expr) {
		char	err[128];
			snprintf(err, sizeof(err), "not a variable type: \"%s\"",
				 par->cp_tok.ct_literal);
			CONSUME();

			ERROR_TOK(&err_tok, err);
		}

		CONSUME();
		return v->si_ob.si_expr;
	} else if (EXPECT(T_LIT_INT8))
		ret = cel_make_int8(strtol(par->cp_tok.ct_literal, NULL, 0));
	else if (EXPECT(T_LIT_UINT8))
		ret = cel_make_uint8(strtol(par->cp_tok.ct_literal, NULL, 0));
	else if (EXPECT(T_LIT_INT16))
		ret = cel_make_int16(strtol(par->cp_tok.ct_literal, NULL, 0));
	else if (EXPECT(T_LIT_UINT16))
		ret = cel_make_uint16(strtol(par->cp_tok.ct_literal, NULL, 0));
	else if (EXPECT(T_LIT_INT32))
		ret = cel_make_int32(strtol(par->cp_tok.ct_literal, NULL, 0));
	else if (EXPECT(T_LIT_UINT32))
		ret = cel_make_uint32(strtol(par->cp_tok.ct_literal, NULL, 0));
	else if (EXPECT(T_LIT_INT64))
		ret = cel_make_int64(strtol(par->cp_tok.ct_literal, NULL, 0));
	else if (EXPECT(T_LIT_UINT64))
		ret = cel_make_uint64(strtol(par->cp_tok.ct_literal, NULL, 0));
	else if (EXPECT(T_LIT_STR)) {
	char	*s;
		s = strdup(par->cp_tok.ct_literal + 1);
		s[strlen(s) - 1] = 0;
		ret = cel_make_string(s);
		free(s);
	} else if (EXPECT(T_TRUE))
		ret = cel_make_bool(1);
	else if (EXPECT(T_FALSE))
		ret = cel_make_bool(0);

	if (ret)
		CONSUME();

	return ret;
}

cel_arglist_t *
cel_parse_arglist(par)
	cel_parser_t	*par;
{
/*
 * An argument list is just a comma-separated list of expressions termined by
 * an RPAR.
 */
cel_arglist_t	*al;
cel_expr_t	*e;

	al = calloc(1, sizeof(*al));

	while (e = cel_parse_expr(par)) {
		al->ca_args = realloc(al->ca_args,
				      sizeof(cel_type_t *) * (al->ca_nargs + 1));
		al->ca_args[al->ca_nargs] = e;
		al->ca_nargs++;

		if (ACCEPT(T_COMMA))
			continue;
	}

	return al;
}

cel_expr_t *
cel_parse_if(par)
	cel_parser_t	*par;
{
/*
 * An if expression begins 'if <expr> then', followed by a code block; the code
 * block can be followed by a number of 'else' or 'elif' statements also
 * followed by a code block, and finally an 'end'.
 */

cel_expr_t	*ret;

	ret = calloc(1, sizeof(*ret));
	ret->ce_tag = cel_exp_if;

/* Expression */
	if (cel_parse_expr(par) == NULL) {
		cel_expr_free(ret);
		ERROR("expected expression");
	}

/* 'then' */
	if (!ACCEPT(T_THEN)) {
		cel_expr_free(ret);
		ERROR("expected 'then'");
	}

/* list of statements */
	for (;;) {
		while (cel_parse_stmt(par) != NULL) {
			/*
			 * Each statement has to be followed by a semicolon,
			 * except for the last one in the block.
			 */
			if (ACCEPT(T_SEMI))
				continue;
		}
			
		if (ACCEPT(T_ELIF)) {
			if (cel_parse_expr(par) == NULL) {
				cel_expr_free(ret);
				ERROR("expected expression");
			}

			if (!ACCEPT(T_THEN)) {
				cel_expr_free(ret);
				ERROR("expected 'then'");
			}

			continue;
		}

		if (ACCEPT(T_ELSE))
			continue;

		if (ACCEPT(T_END))
			return ret;

		cel_expr_free(ret);
		ERROR("expected statement, 'end', 'else' or 'elif'");
	}
}
