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
		if (cel_next_token(par->cp_lex, &par->cp_tok) != 0) {
			par->cp_error = wcsdup(L"lexer failure");
			return -1;
		}

		switch (par->cp_tok.ct_token) {
		case T_FUNC:
			if (cel_parse_func(par) != 0)
				return -1;
			break;

		case T_VAR:
			if (cel_parse_var(par) != 0)
				return -1;
			break;

		case T_TYPE:
			if (cel_parse_typedef(par) != 0)
				return -1;
			break;

		case T_EOT:
			return 0;

		default:
			par->cp_error = wcsdup(L"expected function, variable or type definition");
			par->cp_err_token = par->cp_tok;
			return -1;
		}
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

/* Identifier list, colon */
	for (;;) {
		if (cel_next_token(par->cp_lex, &par->cp_tok) != 0) {
			par->cp_error = wcsdup(L"lexer failure");
			return -1;
		}

		if (par->cp_tok.ct_token != T_ID) {
			par->cp_error = wcsdup(L"expected identifier");
			par->cp_err_token = par->cp_tok;
			return -1;
		}

		if (cel_next_token(par->cp_lex, &par->cp_tok) != 0) {
			par->cp_error = wcsdup(L"lexer failure");
			return -1;
		}

		if (par->cp_tok.ct_token == T_COMMA)
			continue;

		if (par->cp_tok.ct_token == T_COLON)
			break;

		par->cp_error = wcsdup(L"expected ',' or ':'");
		par->cp_err_token = par->cp_tok;
		return -1;
	}

/* Type */
	if (cel_parse_type(par) != 0)
		return -1;

/* Semicolon */
	if (cel_next_token(par->cp_lex, &par->cp_tok) != 0) {
		par->cp_error = wcsdup(L"lexer failure");
		return -1;
	}

	if (par->cp_tok.ct_token != T_SEMI) {
		par->cp_error = wcsdup(L"expected ';'");
		par->cp_err_token = par->cp_tok;
		return -1;
	}

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

/* Identifier list, colon */
	for (;;) {
		if (cel_next_token(par->cp_lex, &par->cp_tok) != 0) {
			par->cp_error = wcsdup(L"lexer failure");
			return -1;
		}

		if (par->cp_tok.ct_token != T_ID) {
			par->cp_error = wcsdup(L"expected identifier");
			par->cp_err_token = par->cp_tok;
			return -1;
		}

		if (cel_next_token(par->cp_lex, &par->cp_tok) != 0) {
			par->cp_error = wcsdup(L"lexer failure");
			return -1;
		}

		if (par->cp_tok.ct_token == T_COMMA)
			continue;

		if (par->cp_tok.ct_token == T_COLON)
			break;

		par->cp_error = wcsdup(L"expected ',' or ':'");
		par->cp_err_token = par->cp_tok;
		return -1;
	}

/* Type */
	if (cel_parse_type(par) != 0)
		return -1;

/* Semicolon */
	if (cel_next_token(par->cp_lex, &par->cp_tok) != 0) {
		par->cp_error = wcsdup(L"lexer failure");
		return -1;
	}

	if (par->cp_tok.ct_token != T_SEMI) {
		par->cp_error = wcsdup(L"expected ';'");
		par->cp_err_token = par->cp_tok;
		return -1;
	}

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

	if (cel_next_token(par->cp_lex, &par->cp_tok) != 0) {
		par->cp_error = wcsdup(L"lexer failure");
		return -1;
	}

/* Optional array specifier */
	if (par->cp_tok.ct_token == T_LSQ) {
		if (cel_next_token(par->cp_lex, &par->cp_tok) != 0) {
			par->cp_error = wcsdup(L"lexer failure");
			return -1;
		}

		if (par->cp_tok.ct_token != T_RSQ) {
			par->cp_error = wcsdup(L"expected ']'");
			par->cp_err_token = par->cp_tok;
			return -1;
		}

		if (cel_next_token(par->cp_lex, &par->cp_tok) != 0) {
			par->cp_error = wcsdup(L"lexer failure");
			return -1;
		}
	}

/* Identifier */
	if (par->cp_tok.ct_token != T_ID) {
		par->cp_error = wcsdup(L"expected identifier");
		par->cp_err_token = par->cp_tok;
		return -1;
	}

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

/* Identifier (function name) */
	if ((cel_next_token(par->cp_lex, &par->cp_tok) != 0)) {
		par->cp_error = wcsdup(L"lexer failure");
		return -1;
	}

	if (par->cp_tok.ct_token != T_ID) {
		par->cp_error = wcsdup(L"expected identifier");
		par->cp_err_token = par->cp_tok;
		return -1;
	}

/* Colon */
	if ((cel_next_token(par->cp_lex, &par->cp_tok) != 0)) {
		par->cp_error = wcsdup(L"lexer failure");
		return -1;
	}

	if (par->cp_tok.ct_token != T_COLON) {
		par->cp_error = wcsdup(L"expected ':'");
		par->cp_err_token = par->cp_tok;
		return -1;
	}

/* Opening bracket */
	if ((cel_next_token(par->cp_lex, &par->cp_tok) != 0)) {
		par->cp_error = wcsdup(L"lexer failure");
		return -1;
	}

	if (par->cp_tok.ct_token != T_LPAR) {
		par->cp_error = wcsdup(L"expected ')'");
		par->cp_err_token = par->cp_tok;
		return -1;
	}

/* Argument list */
	for (;;) {
	/* Argument name */
		if ((cel_next_token(par->cp_lex, &par->cp_tok) != 0)) {
			par->cp_error = wcsdup(L"lexer failure");
			return -1;
		}

		if (par->cp_tok.ct_token != T_ID) {
			par->cp_error = wcsdup(L"expected identifier");
			par->cp_err_token = par->cp_tok;
			return -1;
		}

	/* Colon */
		if ((cel_next_token(par->cp_lex, &par->cp_tok) != 0)) {
			par->cp_error = wcsdup(L"lexer failure");
			return -1;
		}

		if (par->cp_tok.ct_token != T_COLON) {
			par->cp_error = wcsdup(L"expected ':'");
			par->cp_err_token = par->cp_tok;
			return -1;
		}

	/* Type */
		if (cel_parse_type(par) != 0)
			return -1;

	/* ')' or ',' */
		if ((cel_next_token(par->cp_lex, &par->cp_tok) != 0)) {
			par->cp_error = wcsdup(L"lexer failure");
			return -1;
		}

		if (par->cp_tok.ct_token == T_RPAR)
			break;
		if (par->cp_tok.ct_token == T_COMMA)
			continue;

		par->cp_error = wcsdup(L"expected ',' or ')'");
		par->cp_err_token = par->cp_tok;
		return -1;

	}

/* -> */
	if ((cel_next_token(par->cp_lex, &par->cp_tok) != 0)) {
		par->cp_error = wcsdup(L"lexer failure");
		return -1;
	}

	if (par->cp_tok.ct_token != T_ARROW) {
		par->cp_error = wcsdup(L"expected '->'");
		par->cp_err_token = par->cp_tok;
		return -1;
	}

/* Return type */
	if (cel_parse_type(par) != 0)
		return -1;

/* Begin */
	if ((cel_next_token(par->cp_lex, &par->cp_tok) != 0)) {
		par->cp_error = wcsdup(L"lexer failure");
		return -1;
	}

	if (par->cp_tok.ct_token != T_BEGIN) {
		par->cp_error = wcsdup(L"expected 'begin'");
		par->cp_err_token = par->cp_tok;
		return -1;
	}

/*
 * List of statements.  cel_parse_stmt() returns 2 to indicate the special
 * statement 'end', which terminates the function.
 */
	for (;;) {
	int	i;

		if ((i = cel_parse_stmt(par)) < 0)
			return -1;

		if (i == 2)
			break;
	}

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
	for (;;) {
		if ((cel_next_token(par->cp_lex, &par->cp_tok) != 0)) {
			par->cp_error = wcsdup(L"lexer failure");
			return -1;
		}

	/*
	 * Variable and function definitions are handled specially;
	 * everything else must be an expression.
	 */

		switch (par->cp_tok.ct_token) {
		case T_END:
			return 2;

		case T_VAR:
			if (cel_parse_var(par) != 0)
				return -1;
			continue;

		case T_FUNC:
			par->cp_error = wcsdup(L"nested function definitions "
					       L"are not supported");
			par->cp_err_token = par->cp_tok;
			return -1;
		}

		if (cel_parse_expr(par) != 0) {
			par->cp_error = wcsdup(L"expected statement");
			par->cp_err_token = par->cp_tok;
			return -1;
		}

		if (par->cp_tok.ct_token != T_SEMI) {
			par->cp_error = wcsdup(L"expected ';'");
			par->cp_err_token = par->cp_tok;
			return -1;
		}
	}
}

int
cel_parse_expr(par)
	cel_parser_t	*par;
{
/* A bracket introduces a nested expression. */
	if (par->cp_tok.ct_token == T_LPAR) {
		if ((cel_next_token(par->cp_lex, &par->cp_tok) != 0)) {
			par->cp_error = wcsdup(L"lexer failure");
			return -1;
		}

		if (cel_parse_expr(par) != 0)
			return -1;

		if (par->cp_tok.ct_token != T_RPAR) {
			par->cp_error = wcsdup(L"expected ')'");
			par->cp_err_token = par->cp_tok;
			return -1;
		}

		return 0;
	}

/* A numeric or string literal */
	return 0;
}
