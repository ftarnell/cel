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

#define	EXPECT(t)	(par->cp_tok.ct_token == (t))
#define	CONSUME()	cel_next_token(par->cp_lex, &par->cp_tok)
#define	ACCEPT(t)	(EXPECT((t)) ? (CONSUME(), 1) : 0)

#define	ERROR(m)							\
	do {								\
		free(par->cp_error);					\
		par->cp_error = wcsdup((m));				\
		par->cp_err_token = par->cp_tok;			\
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

int
cel_parser_init(par, lex)
	cel_parser_t	*par;
	cel_lexer_t	*lex;
{
	memset(par, 0, sizeof(*par));
	par->cp_lex = lex;
	return 0;
}

cel_block_t *
cel_parse(par)
	cel_parser_t	*par;
{
/*
 * Top-level parser.  The only things which can occur at the top level are
 * functions, typedefs or variables.
 */
cel_block_t	*block;

	CONSUME();

	block = calloc(1, sizeof(*block));

	for (;;) {
		if (EXPECT(T_EOT))
			return block;

		if (cel_parse_typedef(par) == 0 ||
		    cel_parse_expr(par) == 0) {
			if (!ACCEPT(T_SEMI))
				ERROR(L"expected ';'");
			continue;
		}

		return NULL;
	}

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
wchar_t		*name;

	if (!ACCEPT(T_TYPE))
		return NULL;

/* Identifier list, colon */
	for (;;) {
		if (!ACCEPT(T_ID))
			ERROR(L"expected identifier");
		name = wcsdup(par->cp_tok.ct_literal);

		if (ACCEPT(T_COMMA))
			continue;

		if (ACCEPT(T_COLON))
			break;

		free(name);
		ERROR(L"expected ',' or ';'");
	}

/* Type */
	if ((type = cel_parse_type(par)) == 0) {
		free(name);
		ERROR(L"expected type name");
	}

	return cel_make_typedef(name, type);
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
cel_vardecl_t	*var;

	if (!EXPECT(T_VAR))
		return NULL;
	CONSUME();

	var = calloc(1, sizeof(*var));

/* Identifier list, colon */
	for (;;) {
		if (!ACCEPT(T_ID)) {
			cel_vardecl_free(var);
			ERROR(L"expected identifier");
		}

		var->cv_names = realloc(var->cv_names,
					(var->cv_nnames + 1) * sizeof(wchar_t *));
		var->cv_names[var->cv_nnames + 1] = wcsdup(par->cp_tok.ct_literal);
		var->cv_nnames++;

		if (ACCEPT(T_COMMA))
			continue;

		if (ACCEPT(T_COLON))
			break;

		cel_vardecl_free(var);
		ERROR(L"expected ',' or ':'");
	}

/* Type */
	if ((var->cv_type = cel_parse_type(par)) == NULL) {
		cel_vardecl_free(var);
		ERROR(L"expected type name");
	}

	return cel_make_vardecl(var);
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
			ERROR(L"expected ']'");
		array++;
	}

/* Identifier */
	if (ACCEPT(T_INT))
		type = cel_make_type(cel_type_int);
	else if (ACCEPT(T_STRING))
		type = cel_make_type(cel_type_string);
	else if (ACCEPT(T_BOOL))
		type = cel_make_type(cel_type_bool);
	else
		ERROR(L"expected identifier");

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
		ERROR(L"expected function definition");

	func = calloc(1, sizeof(*func));

/* Identifier (function name) */
	if (!ACCEPT(T_ID)) {
		cel_function_free(func);
		ERROR(L"expected identifier");
	}

	func->cf_name = wcsdup(par->cp_tok.ct_literal);

/* Colon */
	if (!ACCEPT(T_COLON)) {
		cel_function_free(func);
		ERROR(L"expected ':'");
	}

/* Opening bracket */
	if (!ACCEPT(T_LPAR)) {
		cel_function_free(func);
		ERROR(L"expected ')'");
	}

/* Argument list */
	for (;;) {
	/* Argument name */
		if (!ACCEPT(T_ID)) {
			cel_function_free(func);
			ERROR(L"expected identifier");
		}

	/* Colon */
		if (!ACCEPT(T_COLON)) {
			cel_function_free(func);
			ERROR(L"expected ':'");
		}

	/* Type */
		if (cel_parse_type(par) != 0) {
			cel_function_free(func);
			ERROR(L"expected type name");
		}

	/* ')' or ',' */
		if (ACCEPT(T_RPAR))
			break;
		if (ACCEPT(T_COMMA))
			continue;

		cel_function_free(func);
		ERROR(L"expected ',' or ')'");
	}

/* -> */
	if (!ACCEPT(T_ARROW)) {
		cel_function_free(func);
		ERROR(L"expected '->'");
	}

/* Return type */
	if (cel_parse_type(par) != 0) {
		cel_function_free(func);
		ERROR(L"expected type name");
	}

/* Begin */
	if (!ACCEPT(T_BEGIN)) {
		cel_function_free(func);
		ERROR(L"expected 'begin'");
	}

/*
 * List of statements.
 */
	while (cel_parse_stmt(par) == 0)
		/* ... */;

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

	if (e = cel_parse_expr(par)) {
		if (!ACCEPT(T_SEMI)) {
			cel_expr_free(e);
			ERROR(L"expected ';'");
		}

		return e;
	}

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
/* expr_assign --> expr_or   [":=" expr_assign] */
cel_expr_t	*e, *f;

	if ((e = cel_parse_expr_or(par)) == NULL)
		return NULL;

	while (ACCEPT(T_ASSIGN)) {
		if ((f = cel_parse_expr_assign(par)) == NULL) {
			cel_expr_free(e);
			return NULL;
		}

		e = cel_make_assign(e, f);
	}

	return 0;
}

cel_expr_t *
cel_parse_expr_or(par)
	cel_parser_t	*par;
{
/* expr_or     --> expr_and   { "or" expr_and } */
cel_expr_t	*e, *f;

	if ((e = cel_parse_expr_and(par)) == NULL)
		return NULL;

	while (ACCEPT(T_OR)) {
		if ((f = cel_parse_expr_and(par)) == NULL) {
			cel_expr_free(e);
			return NULL;
		}

		e = cel_make_or(e, f);
	}

	return e;
}

cel_expr_t *
cel_parse_expr_and(par)
	cel_parser_t	*par;
{
/* expr_and    --> expr_xor   { "and" expr_xor } */
cel_expr_t	*e, *f;

	if ((e = cel_parse_expr_xor(par)) == NULL)
		return NULL;

	while (ACCEPT(T_AND)) {
		if ((f = cel_parse_expr_xor(par)) == NULL) {
			cel_expr_free(e);
			return NULL;
		}

		e = cel_make_and(e, f);
	}

	return e;
}

cel_expr_t *
cel_parse_expr_xor(par)
	cel_parser_t	*par;
{
/* expr_xor    --> expr_eq1   ["^" expr_eq1] */
cel_expr_t	*e, *f;

	if ((e = cel_parse_expr_eq1(par)) == NULL)
		return NULL;

	while (ACCEPT(T_CARET)) {
		if ((f = cel_parse_expr_eq1(par)) == NULL) {
			cel_expr_free(e);
			return NULL;
		}

		e = cel_make_xor(e, f);
	}

	return e;
}

cel_expr_t *
cel_parse_expr_eq1(par)
	cel_parser_t	*par;
{
/* expr_eq1    --> expr_eq2   {( "=" | "!="} expr_eq2} */
cel_expr_t	*e, *f;
int		 op;

	if ((e = cel_parse_expr_eq2(par)) == NULL)
		return NULL;
	
	while ((op = ACCEPT(T_EQ)) || (op = ACCEPT(T_NEQ))) {
		if ((f = cel_parse_expr_eq2(par)) != NULL) {
			cel_expr_free(e);
			ERROR(L"expected expression");
		}

		switch (op) {
		case T_EQ:	e = cel_make_eq(e, f); break;
		case T_NEQ:	e = cel_make_neq(e, f); break;
		}
	}

	return e;
}

cel_expr_t *
cel_parse_expr_eq2(par)
	cel_parser_t	*par;
{
/* expr_eq2    --> expr_plus  {( ">" | ">=" | "<" | "<=") expr_plus} */
cel_expr_t	*e, *f;
int		 op;

	if ((e = cel_parse_expr_plus(par)) == NULL)
		return NULL;

	while ((op = ACCEPT(T_GT)) || (op = ACCEPT(T_GE)) || 
	       (op = ACCEPT(T_LT)) || (op = ACCEPT(T_LE))) {
		if ((f = cel_parse_expr_plus(par)) == NULL) {
			cel_expr_free(e);
			ERROR(L"expected expression");
		}

		switch (op) {
		case T_GT:	e = cel_make_gt(e, f); break;
		case T_GE:	e = cel_make_ge(e, f); break;
		case T_LT:	e = cel_make_lt(e, f); break;
		case T_LE:	e = cel_make_le(e, f); break;
		}
	}

	return e;
}

cel_expr_t *
cel_parse_expr_plus(par)
	cel_parser_t	*par;
{
/* expr_plus   --> expr_mult  {( "+" | "-" ) expr_mult} */
cel_expr_t	*e, *f;
int		 op;

	if ((e = cel_parse_expr_mult(par)) == NULL)
		return NULL;

	while ((op = ACCEPT(T_PLUS)) || (op = ACCEPT(T_MINUS))) {
		if ((f = cel_parse_expr_mult(par)) == NULL) {
			cel_expr_free(e);
			ERROR(L"expected expression");
		}

		switch (op) {
		case T_EQ:	e = cel_make_eq(e, f); break;
		case T_NEQ:	e = cel_make_neq(e, f); break;
		}
	}

	return e;
}

cel_expr_t *
cel_parse_expr_mult(par)
	cel_parser_t	*par;
{
/* expr_mult   --> expr_unary {( "*" | "/" | "%" ) expr_unary} */
cel_expr_t	*e, *f;
int		 op;

	if ((e = cel_parse_expr_unary(par)) == NULL)
		return NULL;

	while ((op = ACCEPT(T_STAR)) || (op = ACCEPT(T_SLASH)) ||
	       (op = ACCEPT(T_PERCENT))) {
		if ((f = cel_parse_expr_unary(par)) == NULL) {
			cel_expr_free(e);
			ERROR(L"expected expression");
		}

		switch (op) {
		case T_STAR:	e = cel_make_mult(e, f); break;
		case T_SLASH:	e = cel_make_div(e, f); break;
		case T_PERCENT:	e = cel_make_modulus(e, f); break;
		}
	}

	return NULL;
}

cel_expr_t *
cel_parse_expr_unary(par)
	cel_parser_t	*par;
{
/* expr_unary  --> expr_post  | "-" expr_unary | "!" expr_unary */
cel_expr_t	*e;

	if (ACCEPT(T_MINUS)) {
		if ((e = cel_parse_expr_unary(par)) == NULL) {
			cel_expr_free(e);
			ERROR(L"expected expression");
		}

		return cel_make_uni_minus(e);
	} else if (ACCEPT(T_NEGATE)) {
		if ((e = cel_parse_expr_unary(par)) == NULL) {
			cel_expr_free(e);
			ERROR(L"expected expression");
		}

		return cel_make_negate(e);
	}

	return cel_parse_expr_post(par);
}

cel_expr_t *
cel_parse_expr_post(par)
	cel_parser_t	*par;
{
/* expr_post   --> expr_value [ ( "(" arglist ")" | "[" expr "]" ) ] */
cel_expr_t	*e;

	if ((e = cel_parse_expr_value(par)) == NULL)
		return NULL;

/* Function call */
	if (ACCEPT(T_LPAR)) {
		if (cel_parse_arglist(par) != 0) {
			cel_expr_free(e);
			return NULL;
		}

		if (!ACCEPT(T_RPAR)) {
			cel_expr_free(e);
			ERROR(L"expected ')'");
		}
	}

/* Array dereference */
	else if (ACCEPT(T_LSQ)) {
		if (cel_parse_expr(par) != 0) {
			cel_expr_free(e);
			ERROR(L"expected expression");
		}

		if (!ACCEPT(T_RSQ)) {
			cel_expr_free(e);
			ERROR(L"expected ']'");
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
			ERROR(L"expected expression");

		if (!ACCEPT(T_RPAR)) {
			cel_expr_free(e);
			ERROR(L"expected ')'");
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
	if (ACCEPT(T_ID))
		return cel_make_identifier(par->cp_tok.ct_literal);

	if (ACCEPT(T_LIT_INT))
		return cel_make_int(wcstol(par->cp_tok.ct_literal, NULL, 0));

	if (ACCEPT(T_LIT_STR))
		return cel_make_string(par->cp_tok.ct_literal);

	if (ACCEPT(T_LIT_BOOL))
		return cel_make_bool(
			wcscmp(par->cp_tok.ct_literal, L"true") == 0 ?
			1 : 0);

	return NULL;
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
	if (cel_parse_expr(par) != 0) {
		cel_expr_free(ret);
		ERROR(L"expected expression");
	}

/* 'then' */
	if (!ACCEPT(T_THEN)) {
		cel_expr_free(ret);
		ERROR(L"expected 'then'");
	}

/* list of statements */
	for (;;) {
		while (cel_parse_stmt(par) == 0)
			/* ... */
			
		if (ACCEPT(T_ELIF)) {
			if (cel_parse_expr(par) != 0) {
				cel_expr_free(ret);
				ERROR(L"expected expression");
			}

			if (!ACCEPT(T_THEN)) {
				cel_expr_free(ret);
				ERROR(L"expected 'then'");
			}

			continue;
		}

		if (ACCEPT(T_ELSE))
			continue;

		if (ACCEPT(T_END))
			return ret;

		cel_expr_free(ret);
		ERROR(L"expected statement, 'end', 'else' or 'elif'");
	}
}
