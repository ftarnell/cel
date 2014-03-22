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
#include	<string.h>

#include	"celcore/cel-config.h"

#ifdef	CEL_HAVE_FFI
# ifdef CEL_HAVE_FFI_FFI_H
#  include	<ffi/ffi.h>
# else
#  include	<ffi.h>
# endif
# include	<dlfcn.h>
#endif

#include	"celcore/parse.h"
#include	"celcore/tokens.h"
#include	"celcore/expr.h"
#include	"celcore/type.h"
#include	"celcore/variable.h"
#include	"celcore/function.h"
#include	"celcore/block.h"
#include	"celcore/scope.h"
#include	"celcore/eval.h"

#include	"celvm/vm.h"

#define	EXPECT(t)	(par->cp_tok.ct_token == (t))
#define	CONSUME()	cel_next_token(par->cp_lex, &par->cp_tok)
#define	ACCEPT(t)	(EXPECT((t)) ? (CONSUME(), t) : 0)

#define	FATAL_TOK(t,m)							\
	do {								\
		if (par->cp_error)					\
			par->cp_error(par, (t), (m));			\
		++par->cp_nerrs;					\
		return NULL;						\
	} while (0)

#define	FATAL(m)	FATAL_TOK(&par->cp_tok, (m))

#define	ERROR_TOK(t,m)							\
	do {								\
		if (par->cp_error)					\
			par->cp_error(par, (t), (m));			\
		++par->cp_nerrs;					\
	} while (0)

#define	ERROR(m)	ERROR_TOK(&par->cp_tok, (m))

#define	WARN(m)								\
	do {								\
		if (par->cp_warn)					\
			par->cp_warn(par, m);				\
		++par->cp_warns;					\
	} while (0)

static cel_expr_t	*cel_parse_func		(cel_parser_t *, cel_scope_t *);
static cel_expr_t	*cel_parse_var		(cel_parser_t *, cel_scope_t *);
static cel_expr_t	*cel_parse_expr		(cel_parser_t *, cel_scope_t *, int);
static cel_typedef_t	*cel_parse_typedef	(cel_parser_t *, cel_scope_t *);
static cel_type_t	*cel_parse_type		(cel_parser_t *, cel_scope_t *);
static cel_expr_t	*cel_parse_value	(cel_parser_t *, cel_scope_t *);
static cel_expr_t	*cel_parse_post		(cel_parser_t *, cel_scope_t *, cel_expr_t *);
static cel_arglist_t	*cel_parse_arglist	(cel_parser_t *, cel_scope_t *);
static cel_expr_t	*cel_parse_if		(cel_parser_t *, cel_scope_t *);
static cel_expr_t	*cel_parse_while	(cel_parser_t *, cel_scope_t *);

cel_parser_t *
cel_parser_new(lex, scope)
	cel_lexer_t	*lex;
	cel_scope_t	*scope;
{
cel_parser_t	*par;
	if ((par = calloc(1, sizeof(*par))) == NULL)
		return NULL;

	memset(par, 0, sizeof(*par));
	par->cp_lex = lex;
	par->cp_scope = scope;
	return par;
}

int
cel_parse(par)
	cel_parser_t		*par;
{
/*
 * Top-level parser.  The only things which can occur at the top level are
 * functions, typedefs or variables.
 */
cel_expr_list_t	*list;
cel_expr_t	*e;
cel_token_t	 start_tok;
int		 i;

	list = calloc(1, sizeof(*list));
	CEL_TAILQ_INIT(list);

	CONSUME();

	for (;;) {
		start_tok = par->cp_tok;

		if (EXPECT(CEL_T_FUNC)) {
			if ((e = cel_parse_func(par, par->cp_scope)) != NULL)
				;
		} else if (EXPECT(CEL_T_VAR)) {
			if ((e = cel_parse_var(par, par->cp_scope)) != NULL)
				;
		} else {
			if (par->cp_error)
				par->cp_error(par, &start_tok, "expected function or variable definition");
			++par->cp_nerrs;
			return -1;
		}

		if (!ACCEPT(CEL_T_SEMI)) {
			/*
			 * The last statement in the file doesn't need a
			 * semicolon.
			 */
			if (EXPECT(CEL_T_EOT))
				return par->cp_nerrs ? -1 : 0;

			if (par->cp_error)
				par->cp_error(par, &start_tok, "expected ';'");
			++par->cp_nerrs;
			return -1;
		}

		if (EXPECT(CEL_T_EOT))
			return par->cp_nerrs ? -1 : 0;
	}

	if (EXPECT(CEL_T_EOT))
		return par->cp_nerrs ? -1 : 0;

	if (par->cp_error)
		par->cp_error(par, &start_tok, "failed to parse statement");
	++par->cp_nerrs;
	return -1;
}

cel_expr_t *
cel_parse_one(par)
	cel_parser_t	*par;
{
cel_expr_t	*ret;

	CONSUME();
	ret = cel_parse_expr(par, par->cp_scope, 0);
	return ret;
}

cel_typedef_t *
cel_parse_typedef(par, sc)
	cel_scope_t	*sc;
	cel_parser_t	*par;
{
/*
 * A typedef consists of a comma-separated list of identifiers, a colon, a 
 * type, and a semicolon.  Example:
 *
 *	type a : int;
 *	type b, c : string[];
 */
cel_type_t	*type = NULL;
char		*name;

	if (!ACCEPT(CEL_T_TYPE))
		return NULL;

/* Identifier list, colon */
	for (;;) {
		if (!ACCEPT(CEL_T_ID))
			FATAL("expected identifier");
		name = strdup(par->cp_tok.ct_literal);

		if (ACCEPT(CEL_T_COMMA))
			continue;

		if (ACCEPT(CEL_T_COLON))
			break;

		free(name);
		FATAL("expected ',' or ';'");
	}

/* Type */
	if ((type = cel_parse_type(par, sc)) == 0) {
		free(name);
		FATAL("expected type name");
	}

	return cel_make_typedef(name, type);
}

struct vard {
	char		*name;
	cel_expr_t	*init;
	cel_token_t	 err_tok;
};

static void
free_varvec(v, n)
	struct vard	*v;
	size_t		 n;
{
	while (n--)
		free(v[n].name);
	free(v);
}

static struct biop {
	int		tok;
	cel_bi_oper_t	op;
	int		prec;
	enum {l,r}	assoc;
} biops[] = {
	{ CEL_T_STAR,		cel_op_mult,	9, l },
	{ CEL_T_SLASH,		cel_op_div,	9, l },
	{ CEL_T_PERCENT,	cel_op_modulus,	9, l },
	{ CEL_T_PLUS,		cel_op_plus,	8, l },
	{ CEL_T_MINUS,		cel_op_minus,   8, l },
	{ CEL_T_LSH,		cel_op_lshift,	7, l },
	{ CEL_T_RSH,		cel_op_rshift,	7, l },
	{ CEL_T_LT,		cel_op_lt,	6, l },
	{ CEL_T_LE,		cel_op_le,	6, l },
	{ CEL_T_GT,		cel_op_gt,	6, l },
	{ CEL_T_GE,		cel_op_ge,	6, l },
	{ CEL_T_EQ,		cel_op_eq,	5, l },
	{ CEL_T_NEQ,		cel_op_neq,	5, l },
	{ CEL_T_BIT_AND,	cel_op_bit_and,	4, l },
	{ CEL_T_NOT,		cel_op_bit_not,	4, l },
	{ CEL_T_BIT_OR,		cel_op_bit_or,	4, l },
	{ CEL_T_AND,		cel_op_and,	3, l },
	{ CEL_T_OR,		cel_op_or,	2, l },
	{ CEL_T_ASSIGN,		cel_op_assign,	1, r },
	{ CEL_T_INCRN,		cel_op_incr,	1, r },
	{ CEL_T_DECRN,		cel_op_decr,	1, r },
	{ CEL_T_MULTN,		cel_op_multn,	1, r },
	{ CEL_T_DIVN,		cel_op_divn,	1, r },
	{ CEL_T_LSHN,		cel_op_lshiftn,	1, r },
	{ CEL_T_RSHN,		cel_op_rshiftn,	1, r },
};

static struct biop *
cel_find_biop(tok)
{
size_t	i;
	for (i = 0; i < sizeof(biops) / sizeof(*biops); i++)
		if (biops[i].tok == tok)
			return &biops[i];
	return NULL;
}

static struct uniop {
	int		tok;
	cel_uni_oper_t	op;
	int		prec;
} uniops[] = {
	{ CEL_T_MINUS,		cel_op_uni_minus,	10 },
	{ CEL_T_NOT,		cel_op_negate,		10 },
	{ CEL_T_RETURN,		cel_op_return,		0 },
};

cel_expr_t *
cel_parse_expr(par, sc, prec)
	cel_parser_t	*par;
	cel_scope_t	*sc;
{
cel_expr_t	*t, *t1;
struct biop	*b;
cel_token_t	 op_tok;

	if ((t = cel_parse_value(par, sc)) == NULL)
		return NULL;

	t = cel_parse_post(par, sc, t);

	while ((b = cel_find_biop(par->cp_tok.ct_token)) && b->prec >= prec) {
	int		 q;
	cel_type_t	*type;

		op_tok = par->cp_tok;
		CONSUME();

		q = b->assoc == r ? b->prec : 1+b->prec;
		if ((t1 = cel_parse_expr(par, sc, q)) == NULL)
			return NULL;

		if ((type = cel_derive_binary_type(b->op, t->ce_type, t1->ce_type)) == NULL) {
		char	a1[64] = {}, a2[64] = {};
		char	err[128];

			cel_name_type(t->ce_type, a1, sizeof(a1) / sizeof(char));
			cel_name_type(t1->ce_type, a2, sizeof(a2) / sizeof(char));

			snprintf(err, sizeof(err) / sizeof(char),
				 "incompatible types in expression: \"%s\", \"%s\"",
				 a1, a2);

			cel_expr_free(t);
			cel_expr_free(t1);
			ERROR_TOK(&op_tok, err);
		}

		if ((t = cel_make_binary(b->op, t, t1)) == NULL)
			return NULL;
	}
	return t;
}

cel_expr_t *
cel_parse_value(par, sc)
	cel_parser_t	*par;
	cel_scope_t	*sc;
{
cel_expr_t	*e = NULL;
int		 i = 0;

	for (i = 0; i < sizeof(uniops) / sizeof(*uniops); i++) {
	cel_expr_t	*t;
		if (par->cp_tok.ct_token == uniops[i].tok) {
			CONSUME();
			if ((t = cel_parse_expr(par, sc, uniops[i].prec)) == NULL)
				FATAL("expected expression");
			return cel_make_unary(uniops[i].op, t);
		}
	}

	if (ACCEPT(CEL_T_LPAR)) {
	cel_expr_t	*t;
		t = cel_parse_expr(par, sc, 0);
		if (!ACCEPT(CEL_T_RPAR))
			FATAL("expected ')'");
		return t;
	}

/* if expression */
	if (ACCEPT(CEL_T_IF)) {
		if ((e = cel_parse_if(par, sc)) == NULL)
			return NULL;
		return e;
	}

/* while expression */
	if (ACCEPT(CEL_T_WHILE)) {
		if ((e = cel_parse_while(par, sc)) == NULL)
			return NULL;
		return e;
	}

/* Function definition */
	if (e = cel_parse_func(par, sc))
		return e;

/* Variable definition */
	if (e = cel_parse_var(par, sc))
		return e;

	if (EXPECT(CEL_T_ID)) {
	cel_scope_item_t	*v;
	cel_token_t		 err_tok = par->cp_tok;

		if ((v = cel_scope_find_item(sc, par->cp_tok.ct_literal)) == NULL)
			v = cel_scope_find_item(par->cp_scope, par->cp_tok.ct_literal);

		if (v == NULL) {
		char	err[128];
			snprintf(err, sizeof(err), "undeclared identifier \"%s\"",
				 par->cp_tok.ct_literal);
			CONSUME();

			ERROR_TOK(&err_tok, err);
			return cel_make_any(cel_make_type(cel_type_void));
		} else {
			e = cel_make_variable(v->si_name, v->si_ob.si_expr->ce_type);
			e->ce_mutable = v->si_ob.si_expr->ce_mutable;
		}
	} else if (EXPECT(CEL_T_LIT_INT8))
		e = cel_make_int8(strtol(par->cp_tok.ct_literal, NULL, 0));
	else if (EXPECT(CEL_T_LIT_UINT8))
		e = cel_make_uint8(strtol(par->cp_tok.ct_literal, NULL, 0));
	else if (EXPECT(CEL_T_LIT_INT16))
		e = cel_make_int16(strtol(par->cp_tok.ct_literal, NULL, 0));
	else if (EXPECT(CEL_T_LIT_UINT16))
		e = cel_make_uint16(strtol(par->cp_tok.ct_literal, NULL, 0));
	else if (EXPECT(CEL_T_LIT_INT32))
		e = cel_make_int32(strtol(par->cp_tok.ct_literal, NULL, 0));
	else if (EXPECT(CEL_T_LIT_UINT32))
		e = cel_make_uint32(strtol(par->cp_tok.ct_literal, NULL, 0));
	else if (EXPECT(CEL_T_LIT_INT64))
		e = cel_make_int64(strtol(par->cp_tok.ct_literal, NULL, 0));
	else if (EXPECT(CEL_T_LIT_UINT64))
		e = cel_make_uint64(strtol(par->cp_tok.ct_literal, NULL, 0));
	else if (EXPECT(CEL_T_LIT_FLOAT))
		e = cel_make_dfloat(strtof(par->cp_tok.ct_literal, NULL));
	else if (EXPECT(CEL_T_LIT_STR)) {
		e = cel_make_string(par->cp_tok.ct_literal);
	} else if (EXPECT(CEL_T_TRUE))
		e = cel_make_bool(1);
	else if (EXPECT(CEL_T_FALSE))
		e = cel_make_bool(0);

	if (e)
		CONSUME();

	return e;
}

cel_expr_t *
cel_parse_post(par, sc, e)
	cel_parser_t	*par;
	cel_scope_t	*sc;
	cel_expr_t	*e;
{
int		 op;
cel_token_t	 err_tok;

/* Function call */
	err_tok = par->cp_tok;
	while ((op = ACCEPT(CEL_T_LPAR)) || (op = ACCEPT(CEL_T_LSQ)) ||
	       (op = ACCEPT(CEL_T_AS)) || (op = ACCEPT(CEL_T_AT)) ||
	       (op = ACCEPT(CEL_T_CARET))) {
	cel_type_t	*t;
	cel_arglist_t	*args;

		switch (op) {
		case CEL_T_CARET:
			if (e->ce_type->ct_tag != cel_type_ptr)
				ERROR_TOK(&err_tok, "not a pointer type");
			else
				e = cel_make_deref(e);
			break;

		case CEL_T_AT:
			if (e->ce_tag != cel_exp_variable)
				ERROR_TOK(&err_tok, "cannot take address of temporary");
			t = cel_make_ptr(e->ce_type);
			e = cel_make_unary(cel_op_addr, e);
			e->ce_type = t;
			break;

		case CEL_T_LPAR:
			if ((args = cel_parse_arglist(par, sc)) == NULL) {
				cel_expr_free(e);
				return NULL;
			}

			if (!ACCEPT(CEL_T_RPAR)) {
				cel_expr_free(e);
				FATAL("expected ')' or argument list");
			}

			if (e->ce_type->ct_tag != cel_type_function)
				ERROR_TOK(&err_tok, "only functions can be called");
			else
				e = cel_make_call(e, args);
			break;

		case CEL_T_LSQ:
			if (cel_parse_expr(par, sc, 0) == NULL) {
				cel_expr_free(e);
				FATAL("expected expression");
			}

			if (!ACCEPT(CEL_T_RSQ)) {
				cel_expr_free(e);
				FATAL("expected ']'");
			}
			break;

		case CEL_T_AS:
			err_tok = par->cp_tok;

			if ((t = cel_parse_type(par, sc)) == NULL) {
				cel_expr_free(e);
				FATAL("expected type");
			}

			if (!cel_type_convertable(e->ce_type, t)) {
			char	a1[64], a2[64];
			char	err[128];

				cel_name_type(e->ce_type, a1, sizeof(a1));
				cel_name_type(t, a2, sizeof(a2));

				snprintf(err, sizeof(err) / sizeof(char),
					 "expression of type \"%s\" cannot be converted to \"%s\"",
					 a1, a2);

				cel_expr_free(e);
				ERROR_TOK(&err_tok, err);
			}

			e = cel_make_cast(e, t);
			break;
		}
	}

	return e;
}

cel_arglist_t *
cel_parse_arglist(par, sc)
	cel_parser_t	*par;
	cel_scope_t	*sc;
{
/*
 * An argument list is just a comma-separated list of expressions termined by
 * an RPAR.
 */
cel_arglist_t	*al;
cel_expr_t	*e;

	al = calloc(1, sizeof(*al));

	while (e = cel_parse_expr(par, sc, 0)) {
		al->ca_args = realloc(al->ca_args,
				      sizeof(cel_type_t *) * (al->ca_nargs + 1));
		al->ca_args[al->ca_nargs] = e;
		al->ca_nargs++;

		if (ACCEPT(CEL_T_COMMA))
			continue;
	}

	return al;
}

cel_expr_t *
cel_parse_var(par, sc)
	cel_scope_t	*sc;
	cel_parser_t	*par;
{
/*
 * A variable definition consists of a comma-separated list of identifiers,
 * a colon, a type, and a semicolon.  Example:
 *
 *	var a : int;
 *	var b, c : []string;
 */
struct vard	*names = NULL;
size_t		 nnames = 0, i;
cel_type_t	*type = NULL;
char		*extern_ = NULL;
int		 const_ = 0;

	if (!EXPECT(CEL_T_VAR))
		return NULL;
	CONSUME();

/* Attribute list */
	if (ACCEPT(CEL_T_LPAR)) {
		for (;;) {
			if (ACCEPT(CEL_T_RPAR))
				break;

			if (ACCEPT(CEL_T_CONST)) {
				const_ = 1;
				continue;
			}

			if (ACCEPT(CEL_T_EXTERN)) {
				if (EXPECT(CEL_T_LIT_STR)) {
					extern_ = strdup(par->cp_tok.ct_literal);
					CONSUME();
					continue;
				} else {
					ERROR("expected string");
					continue;
				}
			} 

			ERROR("expected 'extern' or ')'");
		}
	}

/* Identifier list, colon */
	for (;;) {
	cel_expr_t	*e = NULL;
	char		*name;
	cel_token_t	 err_tok;

		err_tok = par->cp_tok;

		if (!EXPECT(CEL_T_ID)) {
			free_varvec(names, nnames);
			FATAL("expected identifier");
		}

		if (cel_scope_find_item(sc, par->cp_tok.ct_literal)) {
		char		err[128];
		cel_token_t	err_tok = par->cp_tok;

			snprintf(err, sizeof(err), "symbol \"%s\" already declared",
				 par->cp_tok.ct_literal);
			CONSUME();
			ERROR_TOK(&err_tok, err);
		}

		name = strdup(par->cp_tok.ct_literal);
		CONSUME();

		if (ACCEPT(CEL_T_ASSIGN)) {
			err_tok = par->cp_tok;

			if ((e = cel_parse_expr(par, sc, 0)) == NULL) {
				free(name);
				FATAL("expected expression");
			}
		}

		names = realloc(names, (nnames + 1) * sizeof(struct vard));
		names[nnames].name = name;
		names[nnames].init = e;
		names[nnames].err_tok = err_tok;
		nnames++;

		if (ACCEPT(CEL_T_COMMA))
			continue;
		else
			break;
	}

/* Type */
	if (ACCEPT(CEL_T_COLON)) {
		if ((type = cel_parse_type(par, sc)) == NULL) {
			free_varvec(names, nnames);
			FATAL("expected type name");
			return NULL;
		}
	}

	for (i = 0; i < nnames; i++) {
	cel_expr_t	*e;
	cel_token_t	 err_tok = names[i].err_tok;

		if (!type && !names[i].init) {
			free_varvec(names, nnames);
			ERROR_TOK(&err_tok, "variable must have either initialiser or type");
		}

		if (names[i].init) {
			if (type && !cel_type_convertable(type, names[i].init->ce_type)) {
			char	a1[64] = {}, a2[64] = {};
			char	err[128];

				cel_name_type(type, a1, sizeof(a1));
				cel_name_type(names[i].init->ce_type, a2, sizeof(a2));

				snprintf(err, sizeof(err) / sizeof(char),
					 "incompatible types in initialisation: \"%s\" := \"%s\"",
					 a1, a2);

				ERROR_TOK(&err_tok, err);
			}
			e = cel_make_vardecl(names[i].name, type ? type : names[i].init->ce_type, names[i].init);
		} else {
			e = cel_make_vardecl(names[i].name, type, NULL);
		}
		e->ce_mutable = !const_;
		cel_scope_add_vardecl(sc, names[i].name, e);
		return e;

		if (extern_) {
#if 0
			/* hmm */
			if ((e->ce_op.ce_uint8 = dlsym(RTLD_SELF, extern_)) == NULL) {
			char	err[128];
				snprintf(err, sizeof(err), "undefined external variable \"%s\"",
					 extern_);
				ERROR(err);
			}
#endif
		}
	}

	free_varvec(names, nnames);

	return NULL;
}

cel_type_t *
cel_parse_type(par, sc)
	cel_scope_t	*sc;
	cel_parser_t	*par;
{
/*
 * A type consists of an optional array specifier, [], followed by an
 * identifier (the type name).  At this point, we don't need to check if
 * the type name is valid;
 */

int		 ptr = 0;
cel_type_t	*type;
int		 op;

/* Optional array specifier */
	while (op = ACCEPT(CEL_T_CARET)) {
#if 0
		if (!ACCEPT(CEL_T_RSQ))
			FATAL("expected ']'");
#endif
		ptr++;
	}

/* Identifier */
	if (ACCEPT(CEL_T_INT))		type = cel_make_type(cel_type_int32);
	else if (ACCEPT(CEL_T_INT8))	type = cel_make_type(cel_type_int8);
	else if (ACCEPT(CEL_T_UINT8))	type = cel_make_type(cel_type_uint8);
	else if (ACCEPT(CEL_T_INT16))	type = cel_make_type(cel_type_int16);
	else if (ACCEPT(CEL_T_UINT16))	type = cel_make_type(cel_type_uint16);
	else if (ACCEPT(CEL_T_INT32))	type = cel_make_type(cel_type_int32);
	else if (ACCEPT(CEL_T_UINT32))	type = cel_make_type(cel_type_uint32);
	else if (ACCEPT(CEL_T_INT64))	type = cel_make_type(cel_type_int64);
	else if (ACCEPT(CEL_T_UINT64))	type = cel_make_type(cel_type_uint64);
	else if (ACCEPT(CEL_T_BOOL))	type = cel_make_type(cel_type_bool);
	else if (ACCEPT(CEL_T_CHAR))	type = cel_make_type(cel_type_schar);
	else if (ACCEPT(CEL_T_UCHAR))	type = cel_make_type(cel_type_schar);
	else if (ACCEPT(CEL_T_SCHAR))	type = cel_make_type(cel_type_uchar);
	else if (ACCEPT(CEL_T_SFLOAT))	type = cel_make_type(cel_type_sfloat);
	else if (ACCEPT(CEL_T_DFLOAT))	type = cel_make_type(cel_type_dfloat);
	else if (ACCEPT(CEL_T_QFLOAT))	type = cel_make_type(cel_type_qfloat);
	else if (ACCEPT(CEL_T_VOID))	type = cel_make_type(cel_type_void);
	else
		return NULL;

	while (ptr--)
		type = cel_make_ptr(type);
	
	return type;
}

cel_expr_t *
cel_parse_func(par, sc_)
	cel_scope_t	*sc_;
	cel_parser_t	*par;
{
/*
 * A function consists of a prototype, 'begin', a series of statements, and
 * an 'end'.
 *
 * The prototype consists of an identifier, a colon, a '(', a list of
 * comma-separated "identifier : type" pairs, a ')', a '->' and a type.
 */

cel_function_t	*func;
cel_expr_t	*e, *ef;
cel_type_t	*t;
int		 auto_type = 0;
int		 single_stmt = 0;
char		*extern_ = NULL;

	if (!ACCEPT(CEL_T_FUNC))
		return NULL;

	func = calloc(1, sizeof(*func));
	func->cf_type = calloc(1, sizeof(*func->cf_type));
	func->cf_type->ct_tag = cel_type_function;
	func->cf_type->ct_type.ct_function.ct_args = calloc(1, sizeof(cel_type_list_t));
	CEL_TAILQ_INIT(func->cf_type->ct_type.ct_function.ct_args);
	CEL_TAILQ_INIT(&func->cf_body);

/* Attributes */
	if (ACCEPT(CEL_T_LPAR)) {
		for (;;) {
			if (ACCEPT(CEL_T_RPAR))
				break;

			if (ACCEPT(CEL_T_EXTERN)) {
				if (EXPECT(CEL_T_LIT_STR)) {
					extern_ = strdup(par->cp_tok.ct_literal);
					CONSUME();
					continue;
				} else {
					ERROR("expected string");
					continue;
				}
			} 

			ERROR("expected 'extern' or ')'");
		}
	}

/* Identifier (function name) */
	if (EXPECT(CEL_T_ID)) {
		if (cel_scope_find_item(sc_, par->cp_tok.ct_literal)) {
		char		err[128];
		cel_token_t	err_tok = par->cp_tok;

			snprintf(err, sizeof(err), "symbol \"%s\" already declared",
				 par->cp_tok.ct_literal);
			CONSUME();
			ERROR_TOK(&err_tok, err);
		}

		func->cf_name = strdup(par->cp_tok.ct_literal);

		CONSUME();
	}

/* Colon */
	if (!ACCEPT(CEL_T_COLON)) {
		cel_function_free(func);
		FATAL("expected ':'");
	}

/* Opening bracket */
	if (!ACCEPT(CEL_T_LPAR)) {
		cel_function_free(func);
		FATAL("expected ')'");
	}

	func->cf_scope = cel_scope_new();

/* Argument list */
	if (ACCEPT(CEL_T_LPAR)) {
	cel_type_t	*a;
	char		*nm;

		for (;;) {
			if (EXPECT(CEL_T_ID)) {
				nm = strdup(par->cp_tok.ct_literal);
				CONSUME();

			/* Colon */
				if (!ACCEPT(CEL_T_COLON)) {
					cel_function_free(func);
					FATAL("expected ':'");
				}
			} else {
				nm = NULL;
			}

		/* Type */
			if ((a = cel_parse_type(par, sc_)) == NULL) {
				cel_function_free(func);
				FATAL("expected type name");
			}

			CEL_TAILQ_INSERT_TAIL(func->cf_type->ct_type.ct_function.ct_args,
					      a, ct_entry);
			if (nm) {
				cel_scope_add_vardecl(func->cf_scope, nm, cel_make_any(a));
				free(nm);
			}
			func->cf_nargs++;

		/* ')' or ',' */
			if (ACCEPT(CEL_T_COMMA))
				continue;

			break;
		}

		if (!ACCEPT(CEL_T_RPAR))
			FATAL("expected ')' or type");
	} else if (ACCEPT(CEL_T_VOID)) {
	} else
		FATAL("expected '(' or 'void'");

/* -> */
	if (ACCEPT(CEL_T_ARROW)) {
	/* Return type */
		if ((t = cel_parse_type(par, sc_)) == NULL) {
			cel_function_free(func);
			FATAL("expected type name");
		}
		func->cf_return_type = t;
		func->cf_type->ct_type.ct_function.ct_return_type = t;

		ef = cel_make_function(func);
		if (func->cf_name)
			cel_scope_add_function(sc_, func->cf_name, ef);
	} else
		auto_type = 1;

	if (!ACCEPT(CEL_T_RPAR)) {
		cel_function_free(func);
		FATAL("expected ')'");
	}

/* Single-statement function */
	if (ACCEPT(CEL_T_EQ)) {
	cel_expr_t	*f;
		single_stmt = 1;
		if ((e = cel_parse_expr(par, func->cf_scope, 0)) == NULL)
			FATAL("expected expression");

		func->cf_return_type = e->ce_type;
		func->cf_type->ct_type.ct_function.ct_return_type = e->ce_type;
		ef = cel_make_function(func);

		f = cel_make_return(e);
		cel_expr_free(e);
		e = f;

		CEL_TAILQ_INSERT_TAIL(&func->cf_body, e, ce_entry);
	} else {

		if (auto_type) {
			cel_function_free(func);
			ERROR("functions with automatic return type may not contain statement blocks");
		}

	/* Begin */
		if (ACCEPT(CEL_T_BEGIN)) {
		/*
		 * List of statements.
		 */

			while (e = cel_parse_expr(par, func->cf_scope, 0)) {
				CEL_TAILQ_INSERT_TAIL(&func->cf_body, e, ce_entry);

				if (ACCEPT(CEL_T_SEMI))
					continue;
			}


			if (!ACCEPT(CEL_T_END)) {
				return NULL;
			}
		} else if (extern_) {
			/* Prototype or external function */
			
#ifdef CEL_HAVE_FFI
		ffi_cif		*cif = calloc(1, sizeof(ffi_cif));
		ffi_type	*rtype;
		ffi_type	**argtypes;
		int		 i;
		cel_type_t	*ty;
		char		 err[128], t[64];

			func->cf_extern = 1;
			if ((func->cf_ptr = dlsym(RTLD_SELF, extern_)) == NULL) {
				snprintf(err, sizeof(err), "undefined external function \"%s\"",
					 extern_);
				ERROR(err);
			}

			switch (func->cf_return_type->ct_tag) {
			case cel_type_schar:
			case cel_type_int8:	rtype = &ffi_type_sint8; break;
			case cel_type_uchar:
			case cel_type_uint8:	rtype = &ffi_type_uint8; break;
			case cel_type_int16:	rtype = &ffi_type_sint16; break;
			case cel_type_uint16:	rtype = &ffi_type_uint16; break;
			case cel_type_int32:	rtype = &ffi_type_sint32; break;
			case cel_type_uint32:	rtype = &ffi_type_uint32; break;
			case cel_type_int64:	rtype = &ffi_type_sint64; break;
			case cel_type_uint64:	rtype = &ffi_type_uint64; break;
			case cel_type_sfloat:	rtype = &ffi_type_float; break;
			case cel_type_dfloat:	rtype = &ffi_type_double; break;
			case cel_type_qfloat:	rtype = &ffi_type_longdouble; break;
			case cel_type_ptr:	rtype = &ffi_type_pointer; break;
			case cel_type_void:	rtype = &ffi_type_void; break;
			default:
				cel_name_type(func->cf_return_type, t, sizeof(t));
				snprintf(err, sizeof(err), "unsupported FFI return type \"%s\"", t);
				ERROR(err);
				return ef;
			}

			argtypes = calloc(func->cf_nargs, sizeof(*argtypes));
			i = 0;
			CEL_TAILQ_FOREACH(ty, func->cf_type->ct_type.ct_function.ct_args, ct_entry) {
				switch (ty->ct_tag) {
				case cel_type_schar:
				case cel_type_int8:	argtypes[i] = &ffi_type_sint8; break;
				case cel_type_uchar:
				case cel_type_uint8:	argtypes[i] = &ffi_type_uint8; break;
				case cel_type_int16:	argtypes[i] = &ffi_type_sint16; break;
				case cel_type_uint16:	argtypes[i] = &ffi_type_uint16; break;
				case cel_type_int32:	argtypes[i] = &ffi_type_sint32; break;
				case cel_type_uint32:	argtypes[i] = &ffi_type_uint32; break;
				case cel_type_int64:	argtypes[i] = &ffi_type_sint64; break;
				case cel_type_uint64:	argtypes[i] = &ffi_type_uint64; break;
				case cel_type_sfloat:	argtypes[i] = &ffi_type_float; break;
				case cel_type_dfloat:	argtypes[i] = &ffi_type_double; break;
				case cel_type_qfloat:	argtypes[i] = &ffi_type_longdouble; break;
				case cel_type_ptr:	argtypes[i] = &ffi_type_pointer; break;
				default:
					cel_name_type(func->cf_return_type, t, sizeof(t));
					snprintf(err, sizeof(err), "unsupported FFI argument type \"%s\"", t);
					ERROR(err);
					return ef;
				}
				i++;
			}

			ffi_prep_cif(cif, FFI_DEFAULT_ABI, func->cf_nargs, rtype, argtypes);
			func->cf_ffi = cif;
#endif
		}
	}

	if (auto_type && func->cf_name)
		cel_scope_add_function(sc_, func->cf_name, ef);

	func->cf_bytecode = cel_vm_func_compile(sc_, func);

	return ef;
}

cel_expr_t *
cel_parse_if(par, sc)
	cel_parser_t	*par;
	cel_scope_t	*sc;
{
/*
 * An if expression begins 'if <expr> then', followed by a code block; the code
 * block can be followed by a number of 'else' or 'elif' statements also
 * followed by a code block, and finally an 'end'.
 */

cel_expr_t	*ret, *e;
cel_if_branch_t	*if_;
cel_type_t	*bool_;

	ret = calloc(1, sizeof(*ret));
	ret->ce_tag = cel_exp_if;
	ret->ce_type = cel_make_type(cel_type_void);
	ret->ce_op.ce_if = calloc(1, sizeof(*ret->ce_op.ce_if));
	CEL_TAILQ_INIT(ret->ce_op.ce_if);

	if_ = calloc(1, sizeof(*if_));

/* Expression */
	if ((e = cel_parse_expr(par, sc, 0)) == NULL) {
		cel_expr_free(ret);
		FATAL("expected expression");
	}

	bool_ = cel_make_type(cel_type_bool);
	if (!cel_type_convertable(bool_, e->ce_type)) {
		free(if_);
		cel_expr_free(e);
		cel_type_free(bool_);
		ERROR("type error: expected boolean");
	}

	if_->ib_condition = e;
	cel_type_free(bool_);

/* 'then' */
	if (!ACCEPT(CEL_T_THEN)) {
		cel_expr_free(ret);
		free(if_);
		FATAL("expected 'then'");
	}

	CEL_TAILQ_INIT(&if_->ib_exprs);

/* list of statements */
	for (;;) {
	cel_expr_t	*e;
		while ((e = cel_parse_expr(par, sc, 0)) != NULL) {
			CEL_TAILQ_INSERT_TAIL(&if_->ib_exprs, e, ce_entry);

			/*
			 * Each statement has to be followed by a semicolon,
			 * except for the last one in the block.
			 */
			if (ACCEPT(CEL_T_SEMI))
				continue;
		}
			
		if (ACCEPT(CEL_T_ELIF)) {
			CEL_TAILQ_INSERT_TAIL(e->ce_op.ce_if, if_, ib_entry);

			if ((e = cel_parse_expr(par, sc, 0)) == NULL) {
				cel_expr_free(ret);
				FATAL("expected expression");
			}

			if (!ACCEPT(CEL_T_THEN)) {
				cel_expr_free(ret);
				FATAL("expected 'then'");
			}

			if_ = calloc(1, sizeof(*if_));

			bool_ = cel_make_type(cel_type_bool);
			if (!cel_type_convertable(bool_, e->ce_type)) {
				free(if_);
				cel_expr_free(e);
				cel_type_free(bool_);
				FATAL("type error: expected boolean");
			}

			if_->ib_condition = e;
			cel_type_free(bool_);
			CEL_TAILQ_INIT(&if_->ib_exprs);
			continue;
		}

		if (ACCEPT(CEL_T_ELSE)) {
			CEL_TAILQ_INSERT_TAIL(ret->ce_op.ce_if, if_, ib_entry);
			if_ = calloc(1, sizeof(*if_));
			CEL_TAILQ_INIT(&if_->ib_exprs);
			continue;
		}

		if (ACCEPT(CEL_T_END)) {
			CEL_TAILQ_INSERT_TAIL(ret->ce_op.ce_if, if_, ib_entry);
			return ret;
		}

		cel_expr_free(ret);
		FATAL("expected statement, 'end', 'else' or 'elif'");
	}
}

cel_expr_t *
cel_parse_while(par, sc)
	cel_parser_t	*par;
	cel_scope_t	*sc;
{
cel_expr_t	*ret, *e;
cel_type_t	*bool_;

	ret = calloc(1, sizeof(*ret));
	ret->ce_tag = cel_exp_while;
	ret->ce_type = cel_make_type(cel_type_void);
	ret->ce_op.ce_while = calloc(1, sizeof(*ret->ce_op.ce_while));

/* Expression */
	if ((e = cel_parse_expr(par, sc, 0)) == NULL) {
		cel_expr_free(ret);
		FATAL("expected expression");
	}

	bool_ = cel_make_type(cel_type_bool);
	if (!cel_type_convertable(bool_, e->ce_type)) {
		cel_expr_free(e);
		cel_type_free(bool_);
		FATAL("type error: expected boolean");
	}

	ret->ce_op.ce_while->wh_condition = e;
	cel_type_free(bool_);

/* 'do' */
	if (!ACCEPT(CEL_T_DO)) {
		cel_expr_free(ret);
		FATAL("expected 'do'");
	}

	CEL_TAILQ_INIT(&ret->ce_op.ce_while->wh_exprs);

/* list of statements */
	for (;;) {
	cel_expr_t	*e;
		while ((e = cel_parse_expr(par, sc, 0)) != NULL) {
			CEL_TAILQ_INSERT_TAIL(&ret->ce_op.ce_while->wh_exprs, e, ce_entry);

			/*
			 * Each statement has to be followed by a semicolon,
			 * except for the last one in the block.
			 */
			if (ACCEPT(CEL_T_SEMI))
				continue;
		}
			
		if (ACCEPT(CEL_T_END))
			return ret;

		cel_expr_free(ret);
		FATAL("expected statement or 'end'");
	}
}

#if 0
/* Optimisation for i := i + 1  ->  i +:= 1 */
		if (f->ce_tag == cel_exp_binary) {
		cel_expr_t	*n = e, *v = NULL;
		cel_expr_t	*left = f->ce_op.ce_binary.left;

			if (left->ce_tag == cel_exp_variable)
				left = cel_scope_find_item(sc, left->ce_op.ce_variable)->si_ob.si_expr;

			if (n->ce_tag == cel_exp_variable)
				v = cel_scope_find_item(sc, e->ce_op.ce_variable)->si_ob.si_expr;
				
			if (left == v) {
			cel_bi_oper_t	oper_;
				switch (f->ce_op.ce_binary.oper) {
				case cel_op_plus:	oper_ = cel_op_incr; break;
				case cel_op_minus:	oper_ = cel_op_decr; break;
				case cel_op_mult:	oper_ = cel_op_multn; break;
				case cel_op_div:	oper_ = cel_op_divn; break;
				default:		abort();
				}

				n = cel_make_binary(oper_, n, f->ce_op.ce_binary.right);
				f->ce_op.ce_binary.left = NULL;
				f->ce_op.ce_binary.right = NULL;
				n->ce_type = e->ce_type;
				e = n;
				goto done;
			}
		}
#endif

#if 0
#endif
