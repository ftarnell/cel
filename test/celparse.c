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
#include	<stdio.h>
#include	<string.h>

#include	"celcore/tokens.h"
#include	"celcore/parse.h"
#include	"celcore/scope.h"

static void
celparse_error(par, tok, s)
	cel_token_t	*tok;
	cel_parser_t	*par;
	char const	*s;
{
	fprintf(stderr, "error: %s\n", s);
	cel_token_print_context(par->cp_lex, tok, stderr);
}

static void
celparse_warn(par, tok, s)
	cel_token_t	*tok;
	cel_parser_t	*par;
	char const	*s;
{
	fprintf(stderr, "warning: %s\n", s);
	cel_token_print_context(par->cp_lex, tok, stderr);
}

int
main(argc, argv)
	char	**argv;
{
FILE	*inf;
char	 line[1024];
size_t	 buflen = 0;
char	*buf = NULL;

cel_lexer_t	lex;
cel_parser_t	*par;
cel_scope_t	*scope;

	if (!argv[1]) {
		fprintf(stderr, "usage: %s <file>\n", argv[0]);
		return 1;
	}

	if ((inf = fopen(argv[1], "r")) == NULL) {
		perror(argv[1]);
		return 1;
	}

	while (fgets(line, sizeof(line), inf)) {
		buf = realloc(buf, sizeof(char) * (buflen + strlen(line)) + 1);
		strcpy(buf + buflen, line);
		*(buf + buflen + strlen(line)) = '\0';
		buflen += strlen(line);
	}

	fclose(inf);

	scope = cel_scope_new();

	if (cel_lexer_init(&lex, buf) != 0) {
		fprintf(stderr, "%s: cannot init lexer\n", argv[1]);
		return 1;
	}

	if ((par = cel_parser_new(&lex, scope)) == NULL) {
		fprintf(stderr, "%s: cannot init parser\n", argv[1]);
		return 1;
	}

	par->cp_error = celparse_error;
	par->cp_warn = celparse_warn;

	if (cel_parse(par) != 0) {
		return 1;
	}

	return 0;
}
