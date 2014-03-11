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

int
main(argc, argv)
	char	**argv;
{
FILE	*inf;
char	 line[1024];
size_t	 buflen = 0;
char	*buf = NULL;

cel_lexer_t	lex;
cel_token_t	tok;

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

	if (cel_lexer_init(&lex, buf) != 0) {
		fprintf(stderr, "%s: cannot init lexer\n", argv[1]);
		return 1;
	}

	while (cel_next_token(&lex, &tok) != T_ERR) {
		if (tok.ct_token == T_EOT)
			return 0;

		fprintf(stderr, "[%s]:%d\n",
			 tok.ct_literal,
			 tok.ct_token);
	}

	fprintf(stderr, "%s: parse error\n", argv[1]);
	return 1;
}
