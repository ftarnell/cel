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

#define	CEL_T_ERR		(-1)
#define	CEL_T_FUNC		1
#define	CEL_T_VAR		2
#define	CEL_T_BEGIN		3
#define	CEL_T_TYPE		4
#define	CEL_T_IF		5
#define	CEL_T_END		6
#define	CEL_T_LPAR		7	/* (  */
#define	CEL_T_RPAR		8	/* )  */
#define	CEL_T_LSQ		9	/* [  */
#define	CEL_T_RSQ		10	/* ]  */
#define	CEL_T_LCUR		11	/* {  */
#define	CEL_T_RCUR		12	/* }  */
#define	CEL_T_LIT_STR		14
#define	CEL_T_COMMA		16
#define	CEL_T_COLON		17
#define	CEL_T_SEMI		18	/* ;  */
#define	CEL_T_ASSIGN		19	/* := */
#define	CEL_T_EQ		20	/* =  */
#define	CEL_T_LT		21	/* <  */
#define	CEL_T_GT		22	/* >  */
#define	CEL_T_LE		23	/* <= */
#define	CEL_T_GE		24	/* >= */
#define	CEL_T_NE		25	/* /= */
#define	CEL_T_EOT		26	/* end of file */
#define	CEL_T_ID		27	/* identifier */
#define	CEL_T_PLUS		28	/* +  */
#define	CEL_T_MINUS		29	/* -  */
#define	CEL_T_STAR		30	/* *  */
#define	CEL_T_SLASH		31	/* /  */
#define	CEL_T_ARROW		32	/* -> */
#define	CEL_T_ELIF		33
#define	CEL_T_ELSE		34
#define	CEL_T_THEN		35
#define	CEL_T_CARET		36
#define	CEL_T_PERCENT		37
#define	CEL_T_AND		38
#define	CEL_T_OR		39
#define	CEL_T_NEQ		40
#define	CEL_T_NOT		41
#define	CEL_T_INT		42
#define	CEL_T_CHAR		43
#define	CEL_T_BOOL		44
#define	CEL_T_TRUE		45
#define	CEL_T_FALSE		46
#define	CEL_T_LIT_INT8		47
#define	CEL_T_LIT_UINT8		48
#define	CEL_T_LIT_INT16		49
#define	CEL_T_LIT_UINT16	50
#define	CEL_T_LIT_INT32		51
#define	CEL_T_LIT_UINT32	52
#define	CEL_T_LIT_INT64		53
#define	CEL_T_LIT_UINT64	54
#define	CEL_T_INT8		55
#define	CEL_T_UINT8		56
#define	CEL_T_INT16		57
#define	CEL_T_UINT16		58
#define	CEL_T_INT32		59
#define	CEL_T_UINT32		60
#define	CEL_T_INT64		61
#define	CEL_T_UINT64		62
#define	CEL_T_AS		63
#define	CEL_T_RETURN		64
#define	CEL_T_VOID		65
#define	CEL_T_WHILE		66
#define	CEL_T_DO		67
#define	CEL_T_INCRN		68 /* +:= */
#define	CEL_T_DECRN		69 /* -:= */
#define	CEL_T_MULTN		70 /* *:= */
#define	CEL_T_DIVN		71 /* /:= */
#define	CEL_T_EXTERN		72
#define	CEL_T_CONST		73
#define	CEL_T_ADDR		74
#define	CEL_T_AT		75
#define	CEL_T_LSH		76
#define	CEL_T_RSH		77
#define	CEL_T_BIT_AND		78
#define	CEL_T_BIT_OR		79
#define	CEL_T_LSHN		80
#define	CEL_T_RSHN		81
#define	CEL_T_SCHAR		82
#define	CEL_T_UCHAR		83

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
	/* Token code (CEL_T_*) */
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
