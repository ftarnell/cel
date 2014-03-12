/*
 * CEL: The Compact Embeddable Language.
 * Copyright (c) 2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#ifndef	CEL_EXPR_H
#define	CEL_EXPR_H

#include	<wchar.h>
#include	<stdlib.h>

#include	"celcore/tailq.h"

struct cel_function;

typedef enum cel_expr_tag {
	cel_exp_void, 
	cel_exp_literal,
	cel_exp_unary,
	cel_exp_binary,
	cel_exp_function,
	cel_exp_vardecl,
	cel_exp_if,
	cel_exp_while,
	cel_exp_cast,
	cel_exp_call,
	cel_exp_return,
	cel_exp_variable
} cel_expr_tag_t;

typedef enum cel_bi_oper {
	cel_op_mult,
	cel_op_div,
	cel_op_plus,
	cel_op_minus,
	cel_op_lt,
	cel_op_le,
	cel_op_eq,
	cel_op_neq,
	cel_op_gt,
	cel_op_ge,
	cel_op_assign,
	cel_op_or,
	cel_op_and,
	cel_op_xor,
	cel_op_modulus,
	cel_op_incr,
	cel_op_decr,
	cel_op_multn,
	cel_op_divn,
	cel_bi_op_last
} cel_bi_oper_t;

typedef enum cel_uni_oper {
	cel_op_negate,
	cel_op_uni_minus
} cel_uni_oper_t;

struct cel_expr;
struct cel_type;

struct cel_if_list;
struct cel_arglist;

struct cel_while;

typedef struct cel_expr {
	cel_expr_tag_t	 ce_tag;
	int		 ce_mutable;
	int		 ce_const;	/* true if known at compile time */
	struct cel_type	*ce_type;

	union {
		int8_t	 ce_int8;
		uint8_t	 ce_uint8;
		int16_t	 ce_int16;
		uint16_t ce_uint16;
		int32_t	 ce_int32;
		uint32_t ce_uint32;
		int64_t	 ce_int64;
		uint64_t ce_uint64;

		int	 ce_bool;
		char	*ce_string;
		char	*ce_variable;

		struct {
			cel_bi_oper_t	 oper;
			struct cel_expr	*left, *right;
		} ce_binary;

		struct {
			cel_uni_oper_t	 oper;
			struct cel_expr	*operand;
		} ce_unary;

		struct {
			struct cel_expr		*func;
			struct cel_arglist	*args;
		} ce_call;

		struct cel_if_list	*ce_if;
		struct cel_while	*ce_while;

		struct cel_function	*ce_function;
		struct cel_vardecl 	*ce_vardecl;
	} ce_op;

	CEL_TAILQ_ENTRY(cel_expr) ce_entry;
} cel_expr_t;

typedef CEL_TAILQ_HEAD(cel_expr_list, cel_expr) cel_expr_list_t;

typedef struct cel_if_branch {
	struct cel_expr	*ib_condition;
	cel_expr_list_t	 ib_exprs;
	CEL_TAILQ_ENTRY(cel_if_branch) ib_entry;
} cel_if_branch_t;

typedef CEL_TAILQ_HEAD(cel_if_list, cel_if_branch) cel_if_list_t;

typedef struct cel_while {
	struct cel_expr	*wh_condition;
	cel_expr_list_t	 wh_exprs;
} cel_while_t;

typedef struct cel_arglist {
	int		 ca_nargs;
	cel_expr_t	**ca_args;
} cel_arglist_t;

cel_expr_t	*cel_make_expr(void);

cel_expr_t	*cel_expr_copy(cel_expr_t *);
void		 cel_expr_free(cel_expr_t *);
void		 cel_expr_print(cel_expr_t *, char *buf, size_t bufsz);

cel_expr_t	*cel_make_int8(int8_t);
cel_expr_t	*cel_make_uint8(uint8_t);
cel_expr_t	*cel_make_int16(int16_t);
cel_expr_t	*cel_make_uint16(uint16_t);
cel_expr_t	*cel_make_int32(int32_t);
cel_expr_t	*cel_make_uint32(uint32_t);
cel_expr_t	*cel_make_int64(int64_t);
cel_expr_t	*cel_make_uint64(uint64_t);
cel_expr_t	*cel_make_bool(int);
cel_expr_t	*cel_make_string(char const *);
cel_expr_t	*cel_make_variable(char const *, struct cel_type *);
cel_expr_t	*cel_make_void(void);
cel_expr_t	*cel_make_any(struct cel_type *);

cel_expr_t	*cel_make_unary(cel_uni_oper_t, cel_expr_t *);
#define	cel_make_negate(e)	cel_make_unary(cel_op_negate, (e))
#define	cel_make_uni_minus(e)	cel_make_unary(cel_op_uni_minus, (e))

cel_expr_t	*cel_make_binary(cel_bi_oper_t, cel_expr_t *, cel_expr_t *);
#define	cel_make_plus(e,f)	cel_make_binary(cel_op_plus, (e), (f))
#define	cel_make_minus(e,f)	cel_make_binary(cel_op_minus, (e), (f))
#define	cel_make_mult(e,f)	cel_make_binary(cel_op_mult, (e), (f))
#define	cel_make_div(e,f)	cel_make_binary(cel_op_div, (e), (f))
#define	cel_make_modulus(e,f)	cel_make_binary(cel_op_modulus, (e), (f))
#define	cel_make_le(e,f)	cel_make_binary(cel_op_le, (e), (f))
#define	cel_make_lt(e,f)	cel_make_binary(cel_op_lt, (e), (f))
#define	cel_make_eq(e,f)	cel_make_binary(cel_op_eq, (e), (f))
#define	cel_make_neq(e,f)	cel_make_binary(cel_op_neq, (e), (f))
#define	cel_make_ge(e,f)	cel_make_binary(cel_op_ge, (e), (f))
#define	cel_make_gt(e,f)	cel_make_binary(cel_op_gt, (e), (f))
#define	cel_make_and(e,f)	cel_make_binary(cel_op_and, (e), (f))
#define	cel_make_or(e,f)	cel_make_binary(cel_op_or, (e), (f))
#define	cel_make_assign(e,f)	cel_make_binary(cel_op_assign, (e), (f))
#define	cel_make_xor(e,f)	cel_make_binary(cel_op_xor, (e), (f))
#define	cel_make_incr(e,f)	cel_make_binary(cel_op_incr, (e), (f))
#define	cel_make_decr(e,f)	cel_make_binary(cel_op_decr, (e), (f))
#define	cel_make_multn(e,f)	cel_make_binary(cel_op_multn, (e), (f))
#define	cel_make_divn(e,f)	cel_make_binary(cel_op_divn, (e), (f))

cel_expr_t	*cel_make_function(struct cel_function *);
cel_expr_t	*cel_make_cast(cel_expr_t *, struct cel_type *);
cel_expr_t	*cel_make_return(cel_expr_t *);
cel_expr_t	*cel_make_call(cel_expr_t *, cel_arglist_t *);
cel_expr_t	*cel_promote_expr(struct cel_type *, cel_expr_t *);

cel_expr_t	*cel_expr_convert(cel_expr_t *v, struct cel_type *type);
void		 cel_expr_assign(cel_expr_t *l, cel_expr_t *r);

#endif	/* !CEL_EXPR_H */
