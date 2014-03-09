/*
 * CEL: The Compact Embeddable Language.
 * Copyright (c) 2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#ifndef	CEL_PARSE_H
#define	CEL_PARSE_H

#include	"celcore/tokens.h"

typedef struct cel_parser {
	/* Our lexer */
	cel_lexer_t	*cp_lex;

	/* In case of error, the token that caused it */
	cel_token_t	 cp_err_token;

	/* Error message from failed parse */
	wchar_t		*cp_error;

	/* Current token (private) */
	cel_token_t	 cp_tok;
} cel_parser_t;

int	cel_parser_init(cel_parser_t *, cel_lexer_t *);

int	cel_parse(cel_parser_t *);
int	cel_parse_func(cel_parser_t *);
int	cel_parse_var(cel_parser_t *);
int	cel_parse_typedef(cel_parser_t *);
int	cel_parse_type(cel_parser_t *);
int	cel_parse_stmt(cel_parser_t *);
int	cel_parse_expr(cel_parser_t *);
int	cel_parse_value(cel_parser_t *);
int	cel_parse_arglist(cel_parser_t *);
int	cel_parse_if(cel_parser_t *);

#endif	/* !CEL_PARSE_H */
