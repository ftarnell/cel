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

#include	"celcore/tokens.h"
#include	"celcore/parse.h"
#include	"celcore/type.h"
#include	"celcore/eval.h"
#include	"celcore/scope.h"
#include	"celcore/cel-config.h"

#include	"build.h"

#ifdef	HAVE_LIBEDIT
# include	<histedit.h>

char const *
icel_prompt(e)
	EditLine	*e;
{
	return ">>> ";
}
#endif

static void
icel_error(par, tok, s)
	cel_token_t	*tok;
	cel_parser_t	*par;
	char const	*s;
{
	fprintf(stderr, "error: %s\n", s);
	cel_token_print_context(par->cp_lex, tok, stderr);
}

static void
icel_warn(par, tok, s)
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
cel_scope_t	*scope;
char		 type[64], value[128];
cel_lexer_t	 lex;
cel_parser_t	*par;
cel_expr_list_t	*program;
cel_expr_t	*result;
	
#ifdef	HAVE_LIBEDIT
EditLine	*el;
History		*hist;
HistEvent	 ev;

	el = el_init(argv[0], stdin, stdout, stderr);
	el_set(el, EL_PROMPT, icel_prompt);
	el_set(el, EL_EDITOR, "emacs");
	hist = history_init();
	history(hist, &ev, H_SETSIZE, 800);
	el_set(el, EL_HIST, history, hist);
#endif

	printf("CEL %s [%s] interactive interpreter\n", CEL_VERSION, CEL_HOST);
	scope = cel_scope_new(NULL);

	if (argv[1]) {
		if (cel_lexer_init(&lex, argv[1]) != 0) {
			fprintf(stderr, "%s: cannot init lexer\n", argv[1]);
			return 1;
		}

		if ((par = cel_parser_new(&lex, scope)) == NULL) {
			fprintf(stderr, "%s: cannot init parser\n", argv[1]);
			return 1;
		}

		par->cp_error = icel_error;
		par->cp_warn = icel_warn;

		if ((program = cel_parse(par)) == NULL || par->cp_nerrs) {
			fprintf(stderr, "(parse error)\n");
			return 1;
		}

		if ((result = cel_eval_list(scope, program)) == NULL) {
			fprintf(stderr, "(eval error)\n");
			return 1;
		}

		cel_name_type(result->ce_type, type, sizeof(type) / sizeof(char));
		cel_expr_print(result, value, sizeof(value) / sizeof(char));
		printf("<%s> %s\n", type, value);

		return 0;
	}

	for (;;) {
#ifndef	HAVE_LIBEDIT
	char		 line_[1024];
#endif
	char const	*line;
#ifdef HAVE_LIBEDIT
	int		 len;

		line = el_gets(el, &len);
		if (!line) {
			fputs("\n", stdout);
			break;
		}

		if (len > 0)
			history(hist, &ev, H_ENTER, line);
#else
		printf(">>> ");
		fflush(stdout);

		if (fgets(line_, sizeof(line_), stdin) == NULL) {
			if (!feof(stdin))
				fputs("\n", stdout);
			break;
		}
		line = line_;
#endif

		if (cel_lexer_init(&lex, line) != 0) {
			fprintf(stderr, "%s: cannot init lexer\n", argv[1]);
			return 1;
		}

		if ((par = cel_parser_new(&lex, scope)) == NULL) {
			fprintf(stderr, "%s: cannot init parser\n", argv[1]);
			return 1;
		}

		par->cp_error = icel_error;
		par->cp_warn = icel_warn;

		if ((program = cel_parse(par)) == NULL || par->cp_nerrs) {
			fprintf(stderr, "(parse error)\n");
			continue;
		}

		if ((result = cel_eval_list(scope, program)) == NULL) {
			fprintf(stderr, "(eval error)\n");
			continue;
		}

		cel_name_type(result->ce_type, type, sizeof(type) / sizeof(char));
		cel_expr_print(result, value, sizeof(value) / sizeof(char));
		printf("<%s> %s\n", type, value);
	}

	return 0;
}
