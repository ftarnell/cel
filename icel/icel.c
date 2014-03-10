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
#include	<wchar.h>

#include	"celcore/cel-config.h"
#include	"celcore/tokens.h"
#include	"celcore/parse.h"
#include	"celcore/type.h"
#include	"celcore/eval.h"

static void
icel_error(par, tok, s)
	cel_token_t	*tok;
	cel_parser_t	*par;
	wchar_t const	*s;
{
	fprintf(stderr, "error: %ls\n", s);
	cel_token_print_context(par->cp_lex, tok, stderr);
}

static void
icel_warn(par, tok, s)
	cel_token_t	*tok;
	cel_parser_t	*par;
	wchar_t const	*s;
{
	fprintf(stderr, "warning: %ls\n", s);
	cel_token_print_context(par->cp_lex, tok, stderr);
}

int
main(argc, argv)
	char	**argv;
{
	printf("CEL %s [%s] interactive interpreter\n", CEL_VERSION, CEL_HOST);

	for (;;) {
	char		 line[1024];
	wchar_t		 wline[1024];
	wchar_t		 type[64], value[128];
	cel_lexer_t	 lex;
	cel_parser_t	 par;
	cel_expr_list_t	*program;
	cel_expr_t	*result;


		printf(">>> ");
		fflush(stdout);

		if (fgets(line, sizeof(line), stdin) == NULL)
			break;

		mbstowcs(wline, line, sizeof(wline) / sizeof(wchar_t) - 1);

		if (cel_lexer_init(&lex, wline) != 0) {
			fprintf(stderr, "%s: cannot init lexer\n", argv[1]);
			return 1;
		}

		if (cel_parser_init(&par, &lex) != 0) {
			fprintf(stderr, "%s: cannot init parser\n", argv[1]);
			return 1;
		}

		par.cp_error = icel_error;
		par.cp_warn = icel_warn;

		if ((program = cel_parse(&par)) == NULL || par.cp_nerrs) {
			fprintf(stderr, "(parse error)\n");
			continue;
		}

		if ((result = cel_eval_list(program)) == NULL) {
			fprintf(stderr, "(eval error)\n");
			continue;
		}

		cel_name_type(result->ce_type, type, sizeof(type) / sizeof(wchar_t));
		cel_expr_print(result, value, sizeof(value) / sizeof(wchar_t));
		printf("<%ls> %ls\n", type, value);
	}

	return 0;
}
