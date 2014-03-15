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
	{ "func",	CEL_T_FUNC		},
	{ "var",	CEL_T_VAR		},
	{ "type",	CEL_T_TYPE		},
	{ "begin",	CEL_T_BEGIN		},
	{ "end",	CEL_T_END		},
	{ "if",		CEL_T_IF		},
	{ "elif",	CEL_T_ELIF		},
	{ "else",	CEL_T_ELSE		},
	{ "then",	CEL_T_THEN		},
	{ "int",	CEL_T_INT		},
	{ "string",	CEL_T_STRING		},
	{ "bool",	CEL_T_BOOL		},
	{ "true",	CEL_T_TRUE		},
	{ "false",	CEL_T_FALSE		},
	{ "int8",	CEL_T_INT8		},
	{ "uint8",	CEL_T_UINT8		},
	{ "int16",	CEL_T_INT16		},
	{ "uint16",	CEL_T_UINT16		},
	{ "int32",	CEL_T_INT32		},
	{ "uint32",	CEL_T_UINT32		},
	{ "int64",	CEL_T_INT64		},
	{ "uint64",	CEL_T_UINT64		},
	{ "as",		CEL_T_AS		},
	{ "return",	CEL_T_RETURN		},
	{ "void",	CEL_T_VOID		},
	{ "while",	CEL_T_WHILE		},
	{ "do",		CEL_T_DO		},
	{ "extern",	CEL_T_EXTERN		},
	{ "const",	CEL_T_CONST		}
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
		ret->ct_token = CEL_T_EOT;
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
	TK('{', "{", CEL_T_LCUR);
	TK('}', "}", CEL_T_RCUR);
	TK('(', "(", CEL_T_LPAR);
	TK(')', ")", CEL_T_RPAR);
	TK('[', "[", CEL_T_LSQ);
	TK(']', "]", CEL_T_RSQ);
	TK('=', "=", CEL_T_EQ);
	TK(',', ",", CEL_T_COMMA);
	TK(';', ";", CEL_T_SEMI);
	TK('^', "^", CEL_T_CARET);
	TK('%', "%", CEL_T_PERCENT);
	TK('@', "@", CEL_T_AT);

	case '&':
		if (lex->cl_bufp[1] == '&')
			TR("&&", CEL_T_AND, 2);
		TR("&", CEL_T_BIT_AND, 1);
		break;

	case '|':
		if (lex->cl_bufp[1] == '|')
			TR("||", CEL_T_OR, 2);
		TR("|", CEL_T_BIT_OR, 1);
		break;

	case '+':
		if (lex->cl_bufp[1] == ':' && lex->cl_bufp[2] == '=')
			TR("+:=", CEL_T_INCRN, 3);
		TR("+", CEL_T_PLUS, 1);
		break;

	case '*':
		if (lex->cl_bufp[1] == ':' && lex->cl_bufp[2] == '=')
			TR("*:=", CEL_T_MULTN, 3);
		TR("*", CEL_T_STAR, 1);
		break;
	case '/':
		if (lex->cl_bufp[1] == ':' && lex->cl_bufp[2] == '=')
			TR("/:=", CEL_T_DIVN, 3);
		TR("/", CEL_T_SLASH, 1);
		break;

	case ':':
		if (lex->cl_bufp[1] == '=')
			TR(":=", CEL_T_ASSIGN, 2);
		TR(":", CEL_T_COLON, 1);
		break;

	case '<':
		if (lex->cl_bufp[1] == '=')
			TR("<=", CEL_T_LE, 2);

		if (lex->cl_bufp[1] == '<')
			if (lex->cl_bufp[2] == ':' && lex->cl_bufp[3] == '=')
				TR("<<:=", CEL_T_LSHN, 4);
			else
				TR("<<", CEL_T_LSH, 2);

		TR("<", CEL_T_LT, 2);
		break;

	case '>':
		if (lex->cl_bufp[1] == '=')
			TR(">=", CEL_T_GE, 2);

		if (lex->cl_bufp[1] == '>')
			if (lex->cl_bufp[2] == ':' && lex->cl_bufp[3] == '=')
				TR(">>:=", CEL_T_RSHN, 4);
			else
				TR(">>", CEL_T_RSH, 2);

		TR(">", CEL_T_GT, 1);
		break;

	case '-':
		if (lex->cl_bufp[1] == ':' && lex->cl_bufp[2] == '=')
			TR("-:=", CEL_T_DECRN, 3);
		else if (lex->cl_bufp[1] == '>')
			TR("->", CEL_T_ARROW, 2);
		else 
			TR("-", CEL_T_MINUS, 1);
		break;
	
	case '!':
		if (lex->cl_bufp[1] == '=')
			TR("!=", CEL_T_NEQ, 2);
		TR("!", CEL_T_NOT, 1);
		break;
	}

/* Identifiers and keywords */

	if (strchr(ID_STARTCHARS, lex->cl_bufp[0])) {
	size_t	span;
		span = strspn(lex->cl_bufp + 1, ID_CHARS);

		ret->ct_literal = calloc(sizeof(char), span + 2);
		ret->ct_token = CEL_T_ID;
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
		case 8:		ret->ct_token = signed_? CEL_T_LIT_INT8  : CEL_T_LIT_UINT8; break;
		case 16:	ret->ct_token = signed_? CEL_T_LIT_INT16 : CEL_T_LIT_UINT16; break;
		case 32:	ret->ct_token = signed_? CEL_T_LIT_INT32 : CEL_T_LIT_UINT32; break;
		case 64:	ret->ct_token = signed_? CEL_T_LIT_INT64 : CEL_T_LIT_UINT64; break;
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
				return CEL_T_ERR;

			if (bsl) {
				bsl = 0;
				continue;
			}

			bsl = 0;

			if (lex->cl_bufp[span] == '\\') {
				bsl = 1;
				continue;
			}

			if (!bsl && lex->cl_bufp[span] == '"') {
				ret->ct_literal = calloc(sizeof(char), span);
				ret->ct_token = CEL_T_LIT_STR;
				memcpy(ret->ct_literal, lex->cl_bufp + 1, span);
				ret->ct_literal[span - 1] = '\0';
				lex->cl_bufp += span + 1;
				lex->cl_col += span + 1;
				return 0;
			}
		}
	}

	return CEL_T_ERR;
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

	if (tok->ct_token == CEL_T_EOT) {
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
