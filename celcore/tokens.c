/*
 * CEL: The Compact Embeddable Language.
 * Copyright (c) 2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#include	<string.h>
#include	<stdlib.h>
#include	<wchar.h>
#include	<wctype.h>

#include	"celcore/tokens.h"

/* Valid characters to begin an identifier */
#define	ID_STARTCHARS	"abcdefghijklmnopqrstuvwxyz"	\
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"	\
			"_"

/* Valid characters in an identifier */
#define	ID_CHARS	ID_STARTCHARS			\
			"0123456789"			\
			"$"

int
cel_lexer_init(lex, buf)
	cel_lexer_t	*lex;
	char const	*buf;
{
	memset(lex, 0, sizeof(*lex));
	lex->cl_buf = lex->cl_bufp = buf;
	lex->cl_line = buf;
	lex->cl_lineno = 1;
	return 0;
}

int
cel_next_token(lex, ret)
	cel_lexer_t	*lex;
	cel_token_t	*ret;
{
size_t	i;
struct {
	char const	*token;
	int		 value;
} keywords[] = {
	{ "func",	T_FUNC		},
	{ "var",	T_VAR		},
	{ "type",	T_TYPE		},
	{ "begin",	T_BEGIN		},
	{ "end",	T_END		},
	{ "if",		T_IF		},
	{ "elif",	T_ELIF		},
	{ "else",	T_ELSE		},
	{ "then",	T_THEN		},
	{ "and",	T_AND		},
	{ "or",		T_OR		},
	{ "int",	T_INT		},
	{ "string",	T_STRING	},
	{ "bool",	T_BOOL		},
	{ "true",	T_TRUE		},
	{ "false",	T_FALSE		},
	{ "int8",	T_INT8		},
	{ "uint8",	T_UINT8		},
	{ "int16",	T_INT16		},
	{ "uint16",	T_UINT16	},
	{ "int32",	T_INT32		},
	{ "uint32",	T_UINT32	},
	{ "int64",	T_INT64		},
	{ "uint64",	T_UINT64	},
	{ "as",		T_AS		},
	{ "return",	T_RETURN	},
	{ "void",	T_VOID		},
	{ "while",	T_WHILE		},
	{ "do",		T_DO		}
};

/* Skip whitespace */
	while (iswspace(*lex->cl_bufp)) {
		if (*lex->cl_bufp == '\n') {
			lex->cl_lineno++;
			lex->cl_line = lex->cl_bufp + 1;
			lex->cl_col = 0;
		} else {
			lex->cl_col++;
		}

		lex->cl_bufp++;
	}

	ret->ct_col = lex->cl_col;
	ret->ct_line = lex->cl_line;
	ret->ct_lineno = lex->cl_lineno;

/* EOT */
	if (!*lex->cl_bufp) {
		ret->ct_literal = strdup("<end-of-file>");
		ret->ct_token = T_EOT;
		return 0;
	}

#define	TR(s,t,l)	do {					\
				ret->ct_literal = strdup((s));	\
				ret->ct_token = (t);		\
				lex->cl_bufp += l;		\
				lex->cl_col += l;		\
				return 0;			\
			} while (0)
#define	TK(c,s,t)	case c: TR((s),(t),1)

/* Trivial one- and two-character tokens */
	switch (*lex->cl_bufp) {
	TK('{', "{", T_LCUR);
	TK('}', "}", T_RCUR);
	TK('(', "(", T_LPAR);
	TK(')', ")", T_RPAR);
	TK('[', "[", T_LSQ);
	TK(']', "]", T_RSQ);
	TK('=', "=", T_EQ);
	TK(',', ",", T_COMMA);
	TK(';', ";", T_SEMI);
	TK('+', "+", T_PLUS);
	TK('*', "*", T_STAR);
	TK('/', "/", T_SLASH);
	TK('^', "^", T_CARET);
	TK('%', "%", T_PERCENT);

	case ':':
		if (lex->cl_bufp[1] == '+' && lex->cl_bufp[2] == '=')
			TR(":+=", T_INCRN, 3);
		else if (lex->cl_bufp[1] == '-' && lex->cl_bufp[2] == '=')
			TR(":-=", T_DECRN, 3);
		else if (lex->cl_bufp[1] == '*' && lex->cl_bufp[2] == '=')
			TR(":*=", T_MULTN, 3);
		else if (lex->cl_bufp[1] == '/' && lex->cl_bufp[2] == '=')
			TR(":/=", T_DIVN, 3);

		if (lex->cl_bufp[1] == '=')
			TR(":=", T_ASSIGN, 2);
		TR(":", T_COLON, 1);
		break;

	case '<':
		if (lex->cl_bufp[1] == '=')
			TR("<=", T_LE, 2);
		TR("<", T_LT, 2);
		break;

	case '>':
		if (lex->cl_bufp[1] == '=')
			TR(">=", T_GE, 2);
		TR(">", T_GT, 1);
		break;

	case '-':
		if (lex->cl_bufp[1] == '>')
			TR("->", T_ARROW, 2);
		TR("-", T_MINUS, 1);
		break;
	
	case '!':
		if (lex->cl_bufp[1] == '=')
			TR("!=", T_NEQ, 2);
		TR("!", T_NEGATE, 1);
		break;
	}

/* Identifiers and keywords */

	if (strchr(ID_STARTCHARS, lex->cl_bufp[0])) {
	size_t	span;
		span = strspn(lex->cl_bufp + 1, ID_CHARS);

		ret->ct_literal = calloc(sizeof(char), span + 2);
		ret->ct_token = T_ID;
		memcpy(ret->ct_literal, lex->cl_bufp, span + 1);

		/* Check if it's a keyword */
		for (i = 0; i < sizeof(keywords) / sizeof(*keywords); i++)
			if (strcmp(ret->ct_literal, keywords[i].token) == 0)
				ret->ct_token = keywords[i].value;

		lex->cl_bufp += span + 1;
		lex->cl_col += span + 1;
		return 0;
	}

/* Numeric literals */

	if (strchr("0123456789", lex->cl_bufp[0])) {
	size_t	span;
	int	width = 32;
	int	signed_ = 1;

		span = strspn(lex->cl_bufp, "0123456789");

		ret->ct_literal = calloc(sizeof(char), span + 1);
		memcpy(ret->ct_literal, lex->cl_bufp, span);

		/* Optional trailing 'i' or 'u'. */
		switch (lex->cl_bufp[span]) {
		case 'u':	signed_ = 0;
		case 'i':	span++;

			/* Optional trailing width */
			if (lex->cl_bufp[span] == '8')
				++span, width = 8;
			else if (lex->cl_bufp[span] == '1' && lex->cl_bufp[span + 1] == '6')
				span += 2, width = 16;
			else if (lex->cl_bufp[span] == '3' && lex->cl_bufp[span + 1] == '2')
				span += 2, width = 32;
			else if (lex->cl_bufp[span] == '6' && lex->cl_bufp[span + 1] == '4')
				span += 2, width = 64;

			break;
		}

		switch (width) {
		case 8:		ret->ct_token = signed_? T_LIT_INT8  : T_LIT_UINT8; break;
		case 16:	ret->ct_token = signed_? T_LIT_INT16 : T_LIT_UINT16; break;
		case 32:	ret->ct_token = signed_? T_LIT_INT32 : T_LIT_UINT32; break;
		case 64:	ret->ct_token = signed_? T_LIT_INT64 : T_LIT_UINT64; break;
		}

		lex->cl_bufp += span;
		lex->cl_col += span;
		return 0;
	}

/* String literals */
	if (lex->cl_bufp[0] == '"') {
	size_t	span;
	int	bsl = 0;

		for (span = 1; ; span++) {
			if (!lex->cl_bufp[span])
				return T_ERR;

			if (bsl) {
				bsl = 0;
				continue;
			}

			bsl = 0;

			if (lex->cl_bufp[span] == '\\') {
				bsl = 1;
				continue;
			}

			if (lex->cl_bufp[span] == '"') {
				ret->ct_literal = calloc(sizeof(char), span + 2);
				ret->ct_token = T_LIT_STR;
				memcpy(ret->ct_literal, lex->cl_bufp, span + 1);
				lex->cl_bufp += span + 1;
				lex->cl_col += span + 1;
				return 0;
			}
		}
	}

	return T_ERR;
}

void
cel_token_print_context(lex, tok, stream)
	cel_lexer_t	*lex;
	cel_token_t	*tok;
	FILE		*stream;
{
char const	*p;
int		 i;

	if (!tok->ct_line)
		return;

	if (tok->ct_token == T_EOT) {
		fputc('a', stream);
		fputc('t', stream);
		fputc(' ', stream);
		fputc('<', stream);
		fputc('e', stream);
		fputc('n', stream);
		fputc('d', stream);
		fputc(' ', stream);
		fputc('o', stream);
		fputc('f', stream);
		fputc(' ', stream);
		fputc('f', stream);
		fputc('i', stream);
		fputc('l', stream);
		fputc('e', stream);
		fputc('>', stream);
		fputc('\n', stream);
		return;
	}

	for (i = 8; i; i--)
		fputc(' ', stream);

	for (p = tok->ct_line; *p && *p != '\n'; p++) {
		if (*p == '\t') {
			for (i = 8; i; i--)
				fputc(' ', stream);
		} else {
			fputc(*p, stream);
		}
	}

	fputc('\n', stream);

	for (i = 8; i; i--)
		fputc(' ', stream);

	for (p = tok->ct_line, i = 0; *p && *p != '\n'; p++, i++) {
		if (i == tok->ct_col)
			break;

		if (*p == '\t') {
			for (i = 8; i; i--)
				fputc(' ', stream);
		} else {
			fputc(' ', stream);
		}
	}
	fputc('^', stream);
	fputc('\n', stream);
}
