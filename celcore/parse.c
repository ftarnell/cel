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

#define	EXPECT(t)	(par->cp_tok.ct_token == (t))
#define	CONSUME()	cel_next_token(par->cp_lex, &par->cp_tok)
#define	ACCEPT(t)	(EXPECT((t)) ? (CONSUME(), 1) : 0)

#define	ERROR(m)							\
	do {								\
		free(par->cp_error);					\
		par->cp_error = wcsdup((m));				\
		par->cp_err_token = par->cp_tok;			\
		return -1;						\
	} while (0)

static int	cel_parse_func(cel_parser_t *);
static int	cel_parse_var(cel_parser_t *);
static int	cel_parse_typedef(cel_parser_t *);
static int	cel_parse_type(cel_parser_t *);
static int	cel_parse_stmt(cel_parser_t *);
static int	cel_parse_expr(cel_parser_t *);
static int	cel_parse_expr_assign(cel_parser_t *);
static int	cel_parse_expr_or(cel_parser_t *);
static int	cel_parse_expr_and(cel_parser_t *);
static int	cel_parse_expr_xor(cel_parser_t *);
static int	cel_parse_expr_eq1(cel_parser_t *);
static int	cel_parse_expr_eq2(cel_parser_t *);
static int	cel_parse_expr_plus(cel_parser_t *);
static int	cel_parse_expr_mult(cel_parser_t *);
static int	cel_parse_expr_unary(cel_parser_t *);
static int	cel_parse_expr_post(cel_parser_t *);
static int	cel_parse_expr_value(cel_parser_t *);
static int	cel_parse_value(cel_parser_t *);
static int	cel_parse_arglist(cel_parser_t *);
static int	cel_parse_if(cel_parser_t *);

int
cel_parser_init(par, lex)
	cel_parser_t	*par;
	cel_lexer_t	*lex;
{
	memset(par, 0, sizeof(*par));
	par->cp_lex = lex;
	return 0;
}

int
cel_parse(par)
	cel_parser_t	*par;
{
/*
 * Top-level parser.  The only things which can occur at the top level are
 * functions, typedefs or variables.
 */

	CONSUME();

	for (;;) {
		if (EXPECT(T_EOT))
			return 0;

		if (cel_parse_typedef(par) == 0 ||
		    cel_parse_expr(par) == 0) {
			if (!ACCEPT(T_SEMI))
				ERROR(L"expected ';'");
			continue;
		}

		return -1;
	}

}

int
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

	if (!ACCEPT(T_TYPE))
		return -1;

/* Identifier list, colon */
	for (;;) {
		if (!ACCEPT(T_ID))
			ERROR(L"expected identifier");

		if (ACCEPT(T_COMMA))
			continue;

		if (ACCEPT(T_COLON))
			break;

		ERROR(L"expected ',' or ';'");
	}

/* Type */
	if (cel_parse_type(par) != 0)
		ERROR(L"expected type name");

/* Done - successful parse */
	return 0;
}

int
cel_parse_var(par)
	cel_parser_t	*par;
{
/*
 * A variable definition consists of a comma-separated list of identifiers,
 * a colon, a type, and a semicolon.  Example:
 *
 *	var a : int;
 *	var b, c : string[];
 */

	if (!EXPECT(T_VAR))
		return -1;
	CONSUME();

/* Identifier list, colon */
	for (;;) {
		if (!ACCEPT(T_ID))
			ERROR(L"expected identifier");

		if (ACCEPT(T_COMMA))
			continue;

		if (ACCEPT(T_COLON))
			break;

		ERROR(L"expected ',' or ':'");
	}

/* Type */
	if (cel_parse_type(par) != 0)
		ERROR(L"expected type name");

/* Done - successful parse */
	return 0;
}

int
cel_parse_type(par)
	cel_parser_t	*par;
{
/*
 * A type consists of an optional array specifier, [], followed by an
 * identifier (the type name).  At this point, we don't need to check if
 * the type name is valid;
 */

/* Optional array specifier */
	if (ACCEPT(T_LSQ)) {
		if (!ACCEPT(T_RSQ))
			ERROR(L"expected ']'");
	}

/* Identifier */
	if (!ACCEPT(T_ID))
		ERROR(L"expected identifier");
	return 0;
}

int
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

	if (!ACCEPT(T_FUNC))
		ERROR(L"expected function definition");

/* Identifier (function name) */
	if (!ACCEPT(T_ID))
		ERROR(L"expected identifier");

/* Colon */
	if (!ACCEPT(T_COLON))
		ERROR(L"expected ':'");

/* Opening bracket */
	if (!ACCEPT(T_LPAR))
		ERROR(L"expected ')'");

/* Argument list */
	for (;;) {
	/* Argument name */
		if (!ACCEPT(T_ID))
			ERROR(L"expected identifier");

	/* Colon */
		if (!ACCEPT(T_COLON))
			ERROR(L"expected ':'");

	/* Type */
		if (cel_parse_type(par) != 0)
			ERROR(L"expected type name");

	/* ')' or ',' */
		if (ACCEPT(T_RPAR))
			break;
		if (ACCEPT(T_COMMA))
			continue;

		ERROR(L"expected ',' or ')'");
	}

/* -> */
	if (!ACCEPT(T_ARROW))
		ERROR(L"expected '->'");

/* Return type */
	if (cel_parse_type(par) != 0)
		ERROR(L"expected type name");

/* Begin */
	if (!ACCEPT(T_BEGIN))
		ERROR(L"expected 'begin'");

/*
 * List of statements.
 */
	while (cel_parse_stmt(par) == 0)
		/* ... */;

	if (!ACCEPT(T_END))
		return -1;

	return 0;
}

int
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
	if (cel_parse_expr(par) == 0) {
		if (!ACCEPT(T_SEMI))
			ERROR(L"expected ';'");
		return 0;
	}

	return -1;
}

int
cel_parse_expr(par)
	cel_parser_t	*par;
{
	return cel_parse_expr_assign(par);
}

int
cel_parse_expr_assign(par)
	cel_parser_t	*par;
{
/* expr_assign --> expr_or   [":=" expr_assign] */
	if (cel_parse_expr_or(par) != 0)
		return -1;

	while (ACCEPT(T_ASSIGN)) {
		if (cel_parse_expr_assign(par) != 0)
			return -1;
	}

	return 0;
}

int
cel_parse_expr_or(par)
	cel_parser_t	*par;
{
/* expr_or     --> expr_and   { "or" expr_and } */
	if (cel_parse_expr_and(par) != 0)
		return -1;

	while (ACCEPT(T_OR)) {
		if (cel_parse_expr_and(par) != 0)
			return -1;
	}

	return 0;
}

int
cel_parse_expr_and(par)
	cel_parser_t	*par;
{
/* expr_and    --> expr_xor   { "and" expr_xor } */
	if (cel_parse_expr_xor(par) != 0)
		return -1;

	while (ACCEPT(T_AND)) {
		if (cel_parse_expr_xor(par) != 0)
			return -1;
	}

	return 0;
}

int
cel_parse_expr_xor(par)
	cel_parser_t	*par;
{
/* expr_xor    --> expr_eq1   ["^" expr_eq1] */
	if (cel_parse_expr_eq1(par) != 0)
		return -1;

	while (ACCEPT(T_CARET)) {
		if (cel_parse_expr_eq1(par) != 0)
			return -1;
	}

	return 0;
}

int
cel_parse_expr_eq1(par)
	cel_parser_t	*par;
{
/* expr_eq1    --> expr_eq2   {( "=" | "!="} expr_eq2} */
	if (cel_parse_expr_eq2(par) != 0)
		return -1;
	
	while (ACCEPT(T_EQ) || ACCEPT(T_NEQ)) {
		if (cel_parse_expr_eq2(par) != 0) {
			ERROR(L"expected expression");
			return -1;
		}
	}

	return 0;
}

int
cel_parse_expr_eq2(par)
	cel_parser_t	*par;
{
/* expr_eq2    --> expr_plus  {( ">" | ">=" | "<" | "<=") expr_plus} */
	if (cel_parse_expr_plus(par) != 0)
		return -1;

	while (ACCEPT(T_GT) || ACCEPT(T_GE) || ACCEPT(T_LT) || ACCEPT(T_LE)) {
		if (cel_parse_expr_plus(par) != 0) {
			ERROR(L"expected expression");
			return -1;
		}
	}

	return 0;
}

int
cel_parse_expr_plus(par)
	cel_parser_t	*par;
{
/* expr_plus   --> expr_mult  {( "+" | "-" ) expr_mult} */
	if (cel_parse_expr_mult(par) != 0)
		return -1;

	while (ACCEPT(T_PLUS) || ACCEPT(T_MINUS)) {
		if (cel_parse_expr_mult(par) != 0) {
			ERROR(L"expected expression");
			return -1;
		}
	}

	return 0;
}

int
cel_parse_expr_mult(par)
	cel_parser_t	*par;
{
/* expr_mult   --> expr_unary {( "*" | "/" | "%" ) expr_unary} */
	if (cel_parse_expr_unary(par) != 0)
		return -1;

	while (ACCEPT(T_STAR) || ACCEPT(T_SLASH) || ACCEPT(T_PERCENT)) {
		if (cel_parse_expr_unary(par) != 0) {
			ERROR(L"expected expression");
			return -1;
		}
	}

	return 0;
}

int
cel_parse_expr_unary(par)
	cel_parser_t	*par;
{
/* expr_unary  --> expr_post  | "-" expr_unary | "!" expr_unary */

	if (ACCEPT(T_MINUS)) {
		if (cel_parse_expr_unary(par) != 0) {
			ERROR(L"expected expression");
			return -1;
		}
	} else if (ACCEPT(T_NEGATE)) {
		if (cel_parse_expr_unary(par) != 0) {
			ERROR(L"expected expression");
			return -1;
		}
	} else {
		if (cel_parse_expr_post(par) != 0)
			return -1;
	}

	return 0;
}

int
cel_parse_expr_post(par)
	cel_parser_t	*par;
{
/* expr_post   --> expr_value [ ( "(" arglist ")" | "[" expr "]" ) ] */

	if (cel_parse_expr_value(par) != 0)
		return -1;

/* Function call */
	if (ACCEPT(T_LPAR)) {
		if (cel_parse_arglist(par) != 0)
			return -1;

		if (!ACCEPT(T_RPAR))
			ERROR(L"expected ')'");
	}

/* Array dereference */
	else if (ACCEPT(T_LSQ)) {
		if (cel_parse_expr(par) != 0) {
			ERROR(L"expected expression");
			return -1;
		}

		if (!ACCEPT(T_RSQ))
			ERROR(L"expected ']'");
	}

	return 0;
}

int cel_parse_expr_value(par)
	cel_parser_t	*par;
{
/* expr_value  --> value | "(" expr ")" | ifexpr */

	if (cel_parse_value(par) == 0)
		return 0;

	if (ACCEPT(T_LPAR)) {
		if (cel_parse_expr(par) != 0)
			ERROR(L"expected expression");

		if (!ACCEPT(T_RPAR))
			ERROR(L"expected ')'");
		return 0;
	}

/* if expression */
	if (ACCEPT(T_IF)) {
		if (cel_parse_if(par) != 0)
			return -1;
		return 0;
	}

/* Function definition */
	if (cel_parse_func(par) == 0)
		return 0;

/* Variable definition */
	if (cel_parse_var(par) == 0)
		return 0;

	return -1;
}

int
cel_parse_value(par)
	cel_parser_t	*par;
{
	if (ACCEPT(T_ID))
		return 0;

	if (ACCEPT(T_LIT_INT))
		return 0;

	if (ACCEPT(T_LIT_STR))
		return 0;

	if (ACCEPT(T_LIT_BOOL))
		return 0;

	return -1;
}

int
cel_parse_arglist(par)
	cel_parser_t	*par;
{
/*
 * An argument list is just a comma-separated list of expressions termined by
 * an RPAR.
 */

	while (cel_parse_expr(par) == 0) {
		if (ACCEPT(T_COMMA))
			continue;
	}

	return 0;
}

int
cel_parse_if(par)
	cel_parser_t	*par;
{
/*
 * An if expression begins 'if <expr> then', followed by a code block; the code
 * block can be followed by a number of 'else' or 'elif' statements also
 * followed by a code block, and finally an 'end'.
 */

/* Expression */
	if (cel_parse_expr(par) != 0)
		ERROR(L"expected expression");

/* 'then' */
	if (!ACCEPT(T_THEN))
		ERROR(L"expected 'then'");

/* list of statements */
	for (;;) {
		while (cel_parse_stmt(par) == 0)
			/* ... */
			
		if (ACCEPT(T_ELIF)) {
			if (cel_parse_expr(par) != 0)
				ERROR(L"expected expression");
			if (!ACCEPT(T_THEN))
				ERROR(L"expected 'then'");
			continue;
		}

		if (ACCEPT(T_ELSE))
			continue;

		if (ACCEPT(T_END))
			return 0;

		ERROR(L"expected statement, 'end', 'else' or 'elif'");
	}
}
