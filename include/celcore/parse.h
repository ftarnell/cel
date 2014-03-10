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
#include	"celcore/expr.h"

struct cel_parser;

typedef void (*cel_emit_error) (struct cel_parser *, cel_token_t *, wchar_t const *);
typedef void (*cel_emit_warning) (struct cel_parser *, cel_token_t *, wchar_t const *);

typedef struct cel_parser {
	/* Our lexer */
	cel_lexer_t	*cp_lex;

	cel_emit_error	 cp_error;
	cel_emit_warning cp_warn;
	int		 cp_nerrs;
	int		 cp_nwarns;

	/* Current token (private) */
	cel_token_t	 cp_tok;
} cel_parser_t;

int cel_parser_init(cel_parser_t *, cel_lexer_t *);

cel_expr_list_t	*cel_parse(cel_parser_t *);

#endif	/* !CEL_PARSE_H */
