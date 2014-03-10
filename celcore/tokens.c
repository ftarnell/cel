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
#define	ID_STARTCHARS	L"abcdefghijklmnopqrstuvwxyz"	\
			L"ABCDEFGHIJKLMNOPQRSTUVWXYZ"	\
			L"_"

/* Valid characters in an identifier */
#define	ID_CHARS	ID_STARTCHARS			\
			L"0123456789"			\
			L"$"

int
cel_lexer_init(lex, buf)
	cel_lexer_t	*lex;
	wchar_t const	*buf;
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
	wchar_t const	*token;
	int		 value;
} keywords[] = {
	{ L"func",	T_FUNC		},
	{ L"var",	T_VAR		},
	{ L"type",	T_TYPE		},
	{ L"begin",	T_BEGIN		},
	{ L"end",	T_END		},
	{ L"if",	T_IF		},
	{ L"elif",	T_ELIF		},
	{ L"else",	T_ELSE		},
	{ L"then",	T_THEN		},
	{ L"and",	T_AND		},
	{ L"or",	T_OR		},
	{ L"int",	T_INT		},
	{ L"string",	T_STRING	},
	{ L"bool",	T_BOOL		}
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
		ret->ct_literal = wcsdup(L"<end-of-file>");
		ret->ct_token = T_EOT;
		return 0;
	}

#define	TR(s,t,l)	do {					\
				ret->ct_literal = wcsdup((s));	\
				ret->ct_token = (t);		\
				lex->cl_bufp += l;		\
				lex->cl_col += l;		\
				return 0;			\
			} while (0)
#define	TK(c,s,t)	case c: TR((s),(t),1)

/* Trivial one- and two-character tokens */
	switch (*lex->cl_bufp) {
	TK('{', L"{", T_LCUR);
	TK('}', L"}", T_RCUR);
	TK('(', L"(", T_LPAR);
	TK(')', L")", T_RPAR);
	TK('[', L"[", T_LSQ);
	TK(']', L"]", T_RSQ);
	TK('=', L"=", T_EQ);
	TK(',', L",", T_COMMA);
	TK(';', L";", T_SEMI);
	TK('+', L"+", T_PLUS);
	TK('*', L"*", T_STAR);
	TK('/', L"/", T_SLASH);
	TK('^', L"^", T_CARET);
	TK('%', L"%", T_PERCENT);

	case ':':
		if (lex->cl_bufp[1] == '=')
			TR(L":=", T_ASSIGN, 2);
		TR(L":", T_COLON, 1);
		break;

	case '<':
		if (lex->cl_bufp[1] == '=')
			TR(L"<=", T_LE, 2);
		TR(L"<", T_LT, 2);
		break;

	case '>':
		if (lex->cl_bufp[1] == '=')
			TR(L">=", T_GE, 2);
		TR(L">", T_GT, 1);
		break;

	case '-':
		if (lex->cl_bufp[1] == '>')
			TR(L"->", T_ARROW, 2);
		TR(L"-", T_MINUS, 1);
		break;
	
	case '!':
		if (lex->cl_bufp[1] == '=')
			TR(L"!=", T_NEQ, 2);
		TR(L"!", T_NEGATE, 1);
		break;
	}

/* Identifiers and keywords */

	if (wcschr(ID_STARTCHARS, lex->cl_bufp[0])) {
	size_t	span;
		span = wcsspn(lex->cl_bufp + 1, ID_CHARS);

		ret->ct_literal = calloc(sizeof(wchar_t), span + 2);
		ret->ct_token = T_ID;
		wmemcpy(ret->ct_literal, lex->cl_bufp, span + 1);

		/* Check if it's a keyword */
		for (i = 0; i < sizeof(keywords) / sizeof(*keywords); i++)
			if (wcscmp(ret->ct_literal, keywords[i].token) == 0)
				ret->ct_token = keywords[i].value;

		lex->cl_bufp += span + 1;
		lex->cl_col += span + 1;
		return 0;
	}

/* Numeric literals */

	if (wcschr(L"0123456789", lex->cl_bufp[0])) {
	size_t	span;
		span = wcsspn(lex->cl_bufp + 1, L"0123456789");

		ret->ct_literal = calloc(sizeof(wchar_t), span + 2);
		ret->ct_token = T_LIT_INT;
		wmemcpy(ret->ct_literal, lex->cl_bufp, span + 1);
		lex->cl_bufp += span + 1;
		lex->cl_col += span + 1;
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
				ret->ct_literal = calloc(sizeof(wchar_t), span + 2);
				ret->ct_token = T_LIT_STR;
				wmemcpy(ret->ct_literal, lex->cl_bufp, span + 1);
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
wchar_t const	*p;
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
