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
#include	<unistd.h>

#include	"celcore/tokens.h"
#include	"celcore/parse.h"
#include	"celcore/type.h"
#include	"celcore/eval.h"
#include	"celcore/scope.h"
#include	"celcore/cel-config.h"

#include	"celvm/vm.h"

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

static int
icel_exec(s)
	char const	*s;
{
char		 type[64] = {}, value[128];
cel_lexer_t	 lex;
cel_parser_t	*par;
cel_expr_list_t	*program;
cel_vm_func_t	*func;
cel_vm_any_t	 ret;
cel_expr_t	*result;
cel_scope_t	*scope;

	if (cel_lexer_init(&lex, s) != 0) {
		fprintf(stderr, "icel: cannot init lexer\n");
		return 1;
	}

	scope = cel_scope_new(NULL);

	if ((par = cel_parser_new(&lex, scope)) == NULL) {
		fprintf(stderr, "icel: cannot init parser\n");
		return 1;
	}

	par->cp_error = icel_error;
	par->cp_warn = icel_warn;

	if ((program = cel_parse(par)) == NULL || par->cp_nerrs) {
		fprintf(stderr, "(parse error)\n");
		return 1;
	}

	if ((func = cel_vm_func_compile(scope, program)) == NULL) {
		fprintf(stderr, "(compile error)\n");
		return 1;
	}

	if (cel_vm_func_execute(scope, func, &ret) == -1) {
		fprintf(stderr, "(execution error)\n");
		return 1;
	}

	result = cel_make_int32(ret.i32);
	cel_name_type(result->ce_type, type, sizeof(type) / sizeof(char));
	cel_expr_print(result, value, sizeof(value) / sizeof(char));
	printf("<%s> %s\n", type, value);
	cel_expr_free(result);

	return 0;
}

int
main(argc, argv)
	char	**argv;
{
int		 i;
	
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

	while ((i = getopt(argc, argv, "")) != -1) {
		switch (i) {
		default:
			return 1;
		}
	}
	argc -= optind;
	argv += optind;

	printf("CEL %s [%s] interactive interpreter\n",
	       CEL_VERSION, CEL_HOST);

	if (argv[0])
		return icel_exec(argv[0]);

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

		icel_exec(line);
	}

	return 0;
}
