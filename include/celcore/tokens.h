/*
 * CEL: The Compact Embeddable Language.
 * Copyright (c) 2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#ifndef	CEL_TOKENS_H
#define	CEL_TOKENS_H

#include	<stdio.h>

#define	T_ERR		(-1)
#define	T_FUNC		1
#define	T_VAR		2
#define	T_BEGIN		3
#define	T_TYPE		4
#define	T_IF		5
#define	T_END		6
#define	T_LPAR		7	/* (  */
#define	T_RPAR		8	/* )  */
#define	T_LSQ		9	/* [  */
#define	T_RSQ		10	/* ]  */
#define	T_LCUR		11	/* {  */
#define	T_RCUR		12	/* }  */
#define	T_LIT_STR	14
#define	T_COMMA		16
#define	T_COLON		17
#define	T_SEMI		18	/* ;  */
#define	T_ASSIGN	19	/* := */
#define	T_EQ		20	/* =  */
#define	T_LT		21	/* <  */
#define	T_GT		22	/* >  */
#define	T_LE		23	/* <= */
#define	T_GE		24	/* >= */
#define	T_NE		25	/* /= */
#define	T_EOT		26	/* end of file */
#define	T_ID		27	/* identifier */
#define	T_PLUS		28	/* +  */
#define	T_MINUS		29	/* -  */
#define	T_STAR		30	/* *  */
#define	T_SLASH		31	/* /  */
#define	T_ARROW		32	/* -> */
#define	T_ELIF		33
#define	T_ELSE		34
#define	T_THEN		35
#define	T_CARET		36
#define	T_PERCENT	37
#define	T_AND		38
#define	T_OR		39
#define	T_NEQ		40
#define	T_NEGATE	41
#define	T_INT		42
#define	T_STRING	43
#define	T_BOOL		44
#define	T_TRUE		45
#define	T_FALSE		46
#define	T_LIT_INT8	47
#define	T_LIT_UINT8	48
#define	T_LIT_INT16	49
#define	T_LIT_UINT16	50
#define	T_LIT_INT32	51
#define	T_LIT_UINT32	52
#define	T_LIT_INT64	53
#define	T_LIT_UINT64	54
#define	T_INT8		55
#define	T_UINT8		56
#define	T_INT16		57
#define	T_UINT16	58
#define	T_INT32		59
#define	T_UINT32	60
#define	T_INT64		61
#define	T_UINT64	62
#define	T_AS		63
#define	T_RETURN	64
#define	T_VOID		65
#define	T_WHILE		66
#define	T_DO		67
#define	T_INCRN		68 /* :+= */
#define	T_DECRN		69 /* :-= */
#define	T_MULTN		70 /* :*= */
#define	T_DIVN		71 /* :/= */
#define	T_EXTERN	72

/*
 * Lexer state.
 */
typedef struct cel_lexer {
	/* Current line in the buffer */
	int		 cl_lineno;

	/* The start of the current line */
	char const	*cl_line;

	/* Current column position in the buffer */
	int		 cl_col;

	/* The buffer passed to cel_lexer_init() (private) */
	char const	*cl_buf;

	/* Our current buffer position (private) */
	char const	*cl_bufp;
} cel_lexer_t;

/*
 * Create a new lexer in .lex to parse the buffer pointed to by .buf. The
 * lexer does not own the buffer, and the user is responsible for freeing
 * it later.
 */
int	cel_lexer_init(cel_lexer_t *lex, char const *buf);

/*
 * A single token.
 */
typedef struct cel_token {
	/* Token code (T_*) */
	int		 ct_token;

	/* The literal text comprising the token */
	char		*ct_literal;

	/* Line number in the buffer where this token began */
	int		 ct_lineno;

	/* The start of the line represented by cl_lineno */
	char const	*ct_line;

	/* Column position in the line where this token began */
	int		 ct_col;
} cel_token_t;

/*
 * Scan the next token in the buffer and place it in .ret.  If an error
 * occurs, -1 is returned and .ret is unmodified.  Otherwise, returns 0.
 */
int	cel_next_token(cel_lexer_t *, cel_token_t *ret);

/*
 * Print (to .stream) the current line associated with the provided token,
 * and underneath it a caret pointing to the specific position within the
 * line where the token occurred.
 */
void	cel_token_print_context(cel_lexer_t *, cel_token_t *, FILE *stream);

#endif	/* !CEL_TOKENS_H */
