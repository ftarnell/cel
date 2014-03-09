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

#define	GET_TOKEN							\
	do {								\
		if ((cel_next_token(par->cp_lex, &par->cp_tok) != 0)) {	\
			par->cp_error = wcsdup(L"lexer failure");	\
			return -1;					\
		}							\
	} while (0)

#define	ERROR(m)							\
	do {								\
		free(par->cp_error);					\
		par->cp_error = wcsdup((m));				\
		par->cp_err_token = par->cp_tok;			\
		return -1;						\
	} while (0)

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

	for (;;) {
		GET_TOKEN;

		if (par->cp_tok.ct_token == T_EOT)
			return 0;

		if (cel_parse_var(par) == 0 ||
		    cel_parse_func(par) == 0 ||
		    cel_parse_typedef(par) == 0 ||
		    cel_parse_expr(par) == 0) {
			if (par->cp_tok.ct_token != T_SEMI) {
				ERROR(L"expected ';'");
				return -1;
			}
			continue;
		}

		ERROR(L"expected statement");
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

	if (par->cp_tok.ct_token != T_TYPE)
		ERROR(L"expected type definition");

/* Identifier list, colon */
	for (;;) {
		GET_TOKEN;

		if (par->cp_tok.ct_token != T_ID)
			ERROR(L"expected identifier");

		GET_TOKEN;

		if (par->cp_tok.ct_token == T_COMMA)
			continue;

		if (par->cp_tok.ct_token == T_COLON) {
			GET_TOKEN;
			break;
		}

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

	if (par->cp_tok.ct_token != T_VAR)
		ERROR(L"expected variable definition");

/* Identifier list, colon */
	for (;;) {
		GET_TOKEN;

		if (par->cp_tok.ct_token != T_ID)
			ERROR(L"expected identifier");

		GET_TOKEN;

		if (par->cp_tok.ct_token == T_COMMA)
			continue;

		if (par->cp_tok.ct_token == T_COLON) {
			GET_TOKEN;
			break;
		}

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
	if (par->cp_tok.ct_token == T_LSQ) {
		GET_TOKEN;

		if (par->cp_tok.ct_token != T_RSQ)
			ERROR(L"expected ']'");

		GET_TOKEN;
	}

/* Identifier */
	if (par->cp_tok.ct_token != T_ID)
		ERROR(L"expected identifier");

	GET_TOKEN;
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

	if (par->cp_tok.ct_token != T_FUNC)
		ERROR(L"expected function definition");

/* Identifier (function name) */
	GET_TOKEN;

	if (par->cp_tok.ct_token != T_ID)
		ERROR(L"expected identifier");

/* Colon */
	GET_TOKEN;

	if (par->cp_tok.ct_token != T_COLON)
		ERROR(L"expected ':'");

/* Opening bracket */
	GET_TOKEN;

	if (par->cp_tok.ct_token != T_LPAR)
		ERROR(L"expected ')'");

/* Argument list */
	for (;;) {
	/* Argument name */
		GET_TOKEN;

		if (par->cp_tok.ct_token != T_ID)
			ERROR(L"expected identifier");

	/* Colon */
		GET_TOKEN;

		if (par->cp_tok.ct_token != T_COLON)
			ERROR(L"expected ':'");

	/* Type */
		GET_TOKEN;

		if (cel_parse_type(par) != 0)
			ERROR(L"expected type name");

	/* ')' or ',' */
		if (par->cp_tok.ct_token == T_RPAR)
			break;
		if (par->cp_tok.ct_token == T_COMMA)
			continue;

		ERROR(L"expected ',' or ')'");
	}

/* -> */
	GET_TOKEN;

	if (par->cp_tok.ct_token != T_ARROW)
		ERROR(L"expected '->'");

	GET_TOKEN;

/* Return type */
	if (cel_parse_type(par) != 0)
		ERROR(L"expected type name");

/* Begin */
	if (par->cp_tok.ct_token != T_BEGIN)
		ERROR(L"expected 'begin'");

/*
 * List of statements.
 */
	GET_TOKEN;

	while (cel_parse_stmt(par) == 0)
		/* ... */;

	if (par->cp_tok.ct_token != T_END)
		ERROR(L"expected statement or 'end'");

	GET_TOKEN;

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
	if (cel_parse_var(par) == 0 ||
	    cel_parse_func(par) == 0 ||
	    cel_parse_expr(par) == 0) {
		if (par->cp_tok.ct_token != T_SEMI)
			ERROR(L"expected ';'");

		GET_TOKEN;
		return 0;
	}

	ERROR(L"expected statement");
}

int
cel_parse_expr(par)
	cel_parser_t	*par;
{
/* A bracket introduces a nested expression. */
	if (par->cp_tok.ct_token == T_LPAR) {
		GET_TOKEN;

		if (cel_parse_expr(par) != 0)
			ERROR(L"expected expression");

		if (par->cp_tok.ct_token != T_RPAR)
			ERROR(L"expected ')'");

		return 0;
	}

/* if expression */
	if (par->cp_tok.ct_token == T_IF)
		return cel_parse_if(par);

/* First term in an expression should be a value */
	if (cel_parse_value(par) != 0)
		ERROR(L"expected value");

/* Optionally followed by an operator or a function call */
	GET_TOKEN;

/* LPAR indicates a function call. */
	if (par->cp_tok.ct_token == T_LPAR)
		return cel_parse_arglist(par);

/* LSQ indicates array dereference */
	if (par->cp_tok.ct_token == T_LSQ) {
		GET_TOKEN;
		if (cel_parse_expr(par) != 0)
			ERROR(L"expected expression");

		if (par->cp_tok.ct_token != T_RSQ)
			ERROR(L"expected ']'");

		GET_TOKEN;
		return 0;
	}

/* Otherwise, it must be an operator */
	switch (par->cp_tok.ct_token) {
	case T_PLUS:
	case T_MINUS:
	case T_SLASH:
	case T_STAR:
	case T_ASSIGN:
	case T_LE:
	case T_LT:
	case T_GE:
	case T_GT:
	case T_EQ:
		break;

	default:
		return 0;
	}

/* Followed by another expression */
	GET_TOKEN;

	return cel_parse_expr(par);
}

int
cel_parse_value(par)
	cel_parser_t	*par;
{
	switch (par->cp_tok.ct_token) {
	case T_ID:
	case T_LIT_INT:
	case T_LIT_STR:
	case T_LIT_BOOL:
		return 0;

	default:
		ERROR(L"expected identifier or literal");
	}
}

int
cel_parse_arglist(par)
	cel_parser_t	*par;
{
/*
 * An argument list is just a comma-separated list of expressions termined by
 * an RPAR.
 */

	GET_TOKEN;

	for (;;) {
		if (par->cp_tok.ct_token == T_RPAR) {
			GET_TOKEN;
			return 0;
		}

		if (cel_parse_expr(par) != 0)
			ERROR(L"expected expression or ')'");

		if (par->cp_tok.ct_token == T_COMMA) {
			GET_TOKEN;
			continue;
		}
		break;
	}

	if (par->cp_tok.ct_token == T_RPAR) {
		GET_TOKEN;
		return 0;
	}

	ERROR(L"expected ',' or ')'");
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
	GET_TOKEN;

/* Expression */
	if (cel_parse_expr(par) != 0)
		ERROR(L"expected expression");

/* 'then' */
	if (par->cp_tok.ct_token != T_THEN)
		ERROR(L"expected 'then'");

/* list of statements */
	for (;;) {
		GET_TOKEN;
		while (cel_parse_stmt(par) == 0)
			/* ... */;

		if (par->cp_tok.ct_token == T_ELIF) {
			GET_TOKEN;

			if (cel_parse_expr(par) != 0)
				ERROR(L"expected expression");

			if (par->cp_tok.ct_token != T_THEN)
				ERROR(L"expected 'then'");

			continue;
		}

		if (par->cp_tok.ct_token == T_ELSE)
			continue;

		if (par->cp_tok.ct_token == T_END) {
			GET_TOKEN;
			return 0;
		}

		ERROR(L"expected statement, 'end', 'else' or 'elif'");
	}
}
