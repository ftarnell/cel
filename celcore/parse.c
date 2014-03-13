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
static cel_typedef_t	*cel_parse_typedef	(cel_parser_t *, cel_scope_t *);
static cel_type_t	*cel_parse_type		(cel_parser_t *, cel_scope_t *);
static cel_expr_t	*cel_parse_stmt		(cel_parser_t *, cel_scope_t *);
static cel_expr_t	*cel_parse_expr		(cel_parser_t *, cel_scope_t *);
static cel_expr_t	*cel_parse_expr_return	(cel_parser_t *, cel_scope_t *);
static cel_expr_t	*cel_parse_expr_assign	(cel_parser_t *, cel_scope_t *);
static cel_expr_t	*cel_parse_expr_or	(cel_parser_t *, cel_scope_t *);
static cel_expr_t	*cel_parse_expr_and	(cel_parser_t *, cel_scope_t *);
static cel_expr_t	*cel_parse_expr_xor	(cel_parser_t *, cel_scope_t *);
static cel_expr_t	*cel_parse_expr_eq1	(cel_parser_t *, cel_scope_t *);
static cel_expr_t	*cel_parse_expr_eq2	(cel_parser_t *, cel_scope_t *);
static cel_expr_t	*cel_parse_expr_plus	(cel_parser_t *, cel_scope_t *);
static cel_expr_t	*cel_parse_expr_mult	(cel_parser_t *, cel_scope_t *);
static cel_expr_t	*cel_parse_expr_unary	(cel_parser_t *, cel_scope_t *);
static cel_expr_t	*cel_parse_expr_post	(cel_parser_t *, cel_scope_t *);
static cel_expr_t	*cel_parse_expr_value	(cel_parser_t *, cel_scope_t *);
static cel_expr_t	*cel_parse_value	(cel_parser_t *, cel_scope_t *);
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

cel_expr_list_t *
cel_parse(par)
	cel_parser_t	*par;
{
/*
 * Top-level parser.  The only things which can occur at the top level are
 * functions, typedefs or variables.
 */
cel_expr_list_t	*list;
cel_expr_t	*e;
cel_token_t	 start_tok;

	list = calloc(1, sizeof(*list));
	CEL_TAILQ_INIT(list);

	CONSUME();

	for (;;) {
		start_tok = par->cp_tok;

		if ((e = cel_parse_expr(par, par->cp_scope)) == NULL)
			break;

		CEL_TAILQ_INSERT_TAIL(list, e, ce_entry);

		if (!ACCEPT(T_SEMI)) {
			/*
			 * The last statement in the file doesn't need a
			 * semicolon.
			 */
			if (EXPECT(T_EOT))
				return par->cp_nerrs ? NULL : list;

			FATAL("expected ';'");
		}
	}

	if (EXPECT(T_EOT))
		return par->cp_nerrs ? NULL : list;

	FATAL_TOK(&start_tok, "failed to parse statement");
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

	if (!ACCEPT(T_TYPE))
		return NULL;

/* Identifier list, colon */
	for (;;) {
		if (!ACCEPT(T_ID))
			FATAL("expected identifier");
		name = strdup(par->cp_tok.ct_literal);

		if (ACCEPT(T_COMMA))
			continue;

		if (ACCEPT(T_COLON))
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

	if (!EXPECT(T_VAR))
		return NULL;
	CONSUME();

/* Attribute list */
	if (ACCEPT(T_LPAR)) {
		for (;;) {
			if (ACCEPT(T_RPAR))
				break;

			if (ACCEPT(T_CONST)) {
				const_ = 1;
				continue;
			}

			if (ACCEPT(T_EXTERN)) {
				if (EXPECT(T_LIT_STR)) {
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

		if (!EXPECT(T_ID)) {
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

		if (ACCEPT(T_ASSIGN)) {
			err_tok = par->cp_tok;

			if ((e = cel_parse_expr(par, sc)) == NULL) {
				free(name);
				FATAL("expected expression");
			}
		}

		names = realloc(names, (nnames + 1) * sizeof(struct vard));
		names[nnames].name = name;
		names[nnames].init = e;
		names[nnames].err_tok = err_tok;
		nnames++;

		if (ACCEPT(T_COMMA))
			continue;
		else
			break;
	}

/* Type */
	if (ACCEPT(T_COLON)) {
		if ((type = cel_parse_type(par, sc)) == NULL) {
			free_varvec(names, nnames);
			FATAL("expected type name");
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
			char	a1[64], a2[64];
			char	err[128];

				cel_name_type(type, a1, sizeof(a1));
				cel_name_type(names[i].init->ce_type, a2, sizeof(a2));

				snprintf(err, sizeof(err) / sizeof(char),
					 "incompatible types in initialisation: \"%s\" := \"%s\"",
					 a1, a2);

				ERROR_TOK(&err_tok, err);
			}
			e = cel_eval(sc, names[i].init);
		} else {
			e = cel_make_any(type);
		}
		e->ce_mutable = !const_;

		if (extern_)
			/* hmm */
			e->ce_op.ce_uint8 = dlsym(RTLD_SELF, extern_);
		cel_scope_add_expr(sc, names[i].name, e);
	}

	free_varvec(names, nnames);
	
	if (type)
		cel_type_free(type);

	return cel_make_void();
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

int		 array = 0;
cel_type_t	*type;

/* Optional array specifier */
	while (ACCEPT(T_LSQ)) {
		if (!ACCEPT(T_RSQ))
			FATAL("expected ']'");
		array++;
	}

/* Identifier */
	if (ACCEPT(T_INT))		type = cel_make_type(cel_type_int32);
	else if (ACCEPT(T_INT8))	type = cel_make_type(cel_type_int8);
	else if (ACCEPT(T_UINT8))	type = cel_make_type(cel_type_uint8);
	else if (ACCEPT(T_INT16))	type = cel_make_type(cel_type_int16);
	else if (ACCEPT(T_UINT16))	type = cel_make_type(cel_type_uint16);
	else if (ACCEPT(T_INT32))	type = cel_make_type(cel_type_int32);
	else if (ACCEPT(T_UINT32))	type = cel_make_type(cel_type_uint32);
	else if (ACCEPT(T_INT64))	type = cel_make_type(cel_type_int64);
	else if (ACCEPT(T_UINT64))	type = cel_make_type(cel_type_uint64);
	else if (ACCEPT(T_STRING))	type = cel_make_type(cel_type_string);
	else if (ACCEPT(T_BOOL))	type = cel_make_type(cel_type_bool);
	else if (ACCEPT(T_VOID))	type = cel_make_type(cel_type_void);
	else
		return NULL;

	while (array--)
		type = cel_make_array(type);
	
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

	if (!ACCEPT(T_FUNC))
		return NULL;

	func = calloc(1, sizeof(*func));
	func->cf_type = calloc(1, sizeof(*func->cf_type));
	func->cf_type->ct_tag = cel_type_function;
	func->cf_type->ct_type.ct_function.ct_args = calloc(1, sizeof(cel_type_list_t));
	CEL_TAILQ_INIT(func->cf_type->ct_type.ct_function.ct_args);
	CEL_TAILQ_INIT(&func->cf_body);

/* Attributes */
	if (ACCEPT(T_LPAR)) {
		for (;;) {
			if (ACCEPT(T_RPAR))
				break;

			if (ACCEPT(T_EXTERN)) {
				if (EXPECT(T_LIT_STR)) {
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
	if (EXPECT(T_ID)) {
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
	if (!ACCEPT(T_COLON)) {
		cel_function_free(func);
		FATAL("expected ':'");
	}

/* Opening bracket */
	if (!ACCEPT(T_LPAR)) {
		cel_function_free(func);
		FATAL("expected ')'");
	}

	func->cf_argscope = cel_scope_new(sc_);
	func->cf_scope = cel_scope_new(func->cf_argscope);

/* Argument list */
	if (!ACCEPT(T_LPAR)) {
		cel_function_free(func);
		FATAL("expected '('");
	}

	if (EXPECT(T_VOID)) {
		CONSUME();
	} else {
		while (EXPECT(T_ID)) {
		cel_type_t	*a;
		char		*nm;

			nm = strdup(par->cp_tok.ct_literal);
			CONSUME();

		/* Colon */
			if (!ACCEPT(T_COLON)) {
				cel_function_free(func);
				FATAL("expected ':'");
			}

		/* Type */
			if ((a = cel_parse_type(par, sc_)) == NULL) {
				cel_function_free(func);
				FATAL("expected type name");
			}

			CEL_TAILQ_INSERT_TAIL(func->cf_type->ct_type.ct_function.ct_args,
					      a, ct_entry);
			cel_scope_add_expr(func->cf_argscope, nm, cel_make_any(a));
			free(nm);
			func->cf_nargs++;

		/* ')' or ',' */
			if (ACCEPT(T_COMMA))
				continue;

			break;
		}
	}

	if (!ACCEPT(T_RPAR))
		FATAL("expected ')'");

/* -> */
	if (ACCEPT(T_ARROW)) {
	/* Return type */
		if ((t = cel_parse_type(par, sc_)) == NULL) {
			cel_function_free(func);
			FATAL("expected type name");
		}
		func->cf_return_type = t;
		func->cf_type->ct_type.ct_function.ct_return_type = t;

		ef = cel_make_function(func);
		if (func->cf_name)
			cel_scope_add_expr(sc_, func->cf_name, ef);
	} else
		auto_type = 1;

	if (!ACCEPT(T_RPAR)) {
		cel_function_free(func);
		FATAL("expected ')'");
	}

/* Single-statement function */
	if (ACCEPT(T_EQ)) {
	cel_expr_t	*f;
		single_stmt = 1;
		if ((e = cel_parse_expr(par, func->cf_scope)) == NULL)
			FATAL("expected expression");

		func->cf_return_type = e->ce_type;
		func->cf_type->ct_type.ct_function.ct_return_type = e->ce_type;
		ef = cel_make_function(func);

		if (e->ce_tag != cel_exp_return) {
			f = cel_make_return(e);
			cel_expr_free(e);
			e = f;
		} else {
			cel_function_free(func);
			ERROR("single-statement functions should not use 'return'");
		}

		CEL_TAILQ_INSERT_TAIL(&func->cf_body, e, ce_entry);
	} else {

		if (auto_type) {
			cel_function_free(func);
			ERROR("functions with automatic return type may not contain statement blocks");
		}

	/* Begin */
		if (ACCEPT(T_BEGIN)) {
		/*
		 * List of statements.
		 */

			while (e = cel_parse_expr(par, func->cf_scope)) {
				CEL_TAILQ_INSERT_TAIL(&func->cf_body, e, ce_entry);

				if (ACCEPT(T_SEMI))
					continue;
			}


			if (!ACCEPT(T_END)) {
				cel_function_free(func);
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

			func->cf_extern = 1;
			func->cf_ptr = dlsym(RTLD_SELF, extern_);
printf("extern %s\n", extern_);
			switch (func->cf_return_type->ct_tag) {
			case cel_type_int8:	rtype = &ffi_type_sint8; break;
			case cel_type_uint8:	rtype = &ffi_type_uint8; break;
			case cel_type_int16:	rtype = &ffi_type_sint16; break;
			case cel_type_uint16:	rtype = &ffi_type_uint16; break;
			case cel_type_int32:	rtype = &ffi_type_sint32; break;
			case cel_type_uint32:	rtype = &ffi_type_uint32; break;
			case cel_type_int64:	rtype = &ffi_type_sint64; break;
			case cel_type_uint64:	rtype = &ffi_type_uint64; break;
			case cel_type_string:	rtype = &ffi_type_pointer; break;
			default:
				ERROR("unsupported FFI return type");
				return ef;
			}

			argtypes = calloc(func->cf_nargs, sizeof(*argtypes));
			i = 0;
			CEL_TAILQ_FOREACH(ty, func->cf_type->ct_type.ct_function.ct_args, ct_entry) {
				switch (ty->ct_tag) {
				case cel_type_int8:	argtypes[i] = &ffi_type_sint8; break;
				case cel_type_uint8:	argtypes[i] = &ffi_type_uint8; break;
				case cel_type_int16:	argtypes[i] = &ffi_type_sint16; break;
				case cel_type_uint16:	argtypes[i] = &ffi_type_uint16; break;
				case cel_type_int32:	argtypes[i] = &ffi_type_sint32; break;
				case cel_type_uint32:	argtypes[i] = &ffi_type_uint32; break;
				case cel_type_int64:	argtypes[i] = &ffi_type_sint64; break;
				case cel_type_uint64:	argtypes[i] = &ffi_type_uint64; break;
				case cel_type_string:	argtypes[i] = &ffi_type_pointer; break;
				default:
					ERROR("unsupported FFI return type");
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
		cel_scope_add_expr(sc_, func->cf_name, ef);

	return ef;
}

cel_expr_t *
cel_parse_stmt(par, sc)
	cel_scope_t	*sc;
	cel_parser_t	*par;
{
/*
 * Statement handling.  The definition of 'statement' is fairly wide and
 * includes anything that can occur within a function, including variable
 * definitions, control constructs (if) and function calls.  'end' is
 * handled as a special type of statement; we return '2' to distinguish
 * that, as it relates to parsing rather than semantics.
 */
cel_expr_t	*e;

	if (e = cel_parse_expr(par, sc))
		return e;

	return NULL;
}

cel_expr_t *
cel_parse_expr(par, sc)
	cel_scope_t	*sc;
	cel_parser_t	*par;
{
	return cel_parse_expr_return(par, sc);
}

cel_expr_t *
cel_parse_expr_return(par, sc)
	cel_scope_t	*sc;
	cel_parser_t	*par;
{
	if (ACCEPT(T_RETURN)) {
		cel_expr_t	*e;

		if ((e = cel_parse_expr_assign(par, sc)) == NULL)
			FATAL("expected expression");

		return cel_make_return(e);
	} else
		return cel_parse_expr_assign(par, sc);
}

cel_expr_t *
cel_parse_expr_assign(par, sc)
	cel_scope_t	*sc;
	cel_parser_t	*par;
{
cel_expr_t	*e, *f;
cel_token_t	 lv_tok, op_tok;

	lv_tok = par->cp_tok;

	if ((e = cel_parse_expr_or(par, sc)) == NULL)
		return NULL;

	for (;;) {
	cel_type_t	*type;
	cel_type_t	*t;
	cel_bi_oper_t	 oper = cel_bi_op_last;
	int		 op;

		op_tok = par->cp_tok;

		if (!(op = ACCEPT(T_ASSIGN)) && !(op = ACCEPT(T_INCRN)) &&
		    !(op = ACCEPT(T_DECRN)) && !(op = ACCEPT(T_MULTN)) &&
		    !(op = ACCEPT(T_DIVN)))
			break;

		if ((f = cel_parse_expr_assign(par, sc)) == NULL) {
			cel_expr_free(e);
			FATAL("expected expression");
		}

		if (!e->ce_mutable)
			ERROR_TOK(&op_tok, "assignment to read-only location");

		switch (op) {
		case T_INCRN:	oper = cel_op_plus; break;
		case T_DECRN:	oper = cel_op_minus; break;
		case T_MULTN:	oper = cel_op_mult; break;
		case T_DIVN:	oper = cel_op_div; break;
		}

		if (oper != cel_bi_op_last) {
			t = cel_derive_binary_type(oper, e->ce_type, f->ce_type);
			f = cel_make_binary(oper, e, f);
			f->ce_type = t;
		}

		if (!cel_type_convertable(e->ce_type, f->ce_type)) {
		char	a1[64], a2[64];
		char	err[128];

			cel_name_type(e->ce_type, a1, sizeof(a1) / sizeof(char));
			cel_name_type(f->ce_type, a2, sizeof(a2) / sizeof(char));

			snprintf(err, sizeof(err) / sizeof(char),
				 "incompatible types in assignment: \"%s\" := \"%s\"",
				 a1, a2);

			ERROR_TOK(&op_tok, err);
		}

		if (f->ce_tag == cel_exp_binary) {
		cel_expr_t	*n = e;
		cel_expr_t	*left = f->ce_op.ce_binary.left;

			if (left->ce_tag == cel_exp_variable) {
				left = cel_scope_find_item(sc, left->ce_op.ce_variable)->si_ob.si_expr;
			}

			if (n->ce_tag == cel_exp_variable) {
				n = cel_scope_find_item(sc, e->ce_op.ce_variable)->si_ob.si_expr;
			}
				
			if (left == n) {
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

		type = e->ce_type;
		e = cel_make_assign(e, f);
		e->ce_type = type;
done:
		;
	}

	return e;
}

cel_expr_t *
cel_parse_expr_or(par, sc)
	cel_scope_t	*sc;
	cel_parser_t	*par;
{
cel_expr_t	*e, *f;
cel_token_t	 op_tok;
int		 op;

	if ((e = cel_parse_expr_and(par, sc)) == NULL)
		return NULL;

	for (;;) {
	cel_type_t	*type;

		op_tok = par->cp_tok;

		if (!(op = ACCEPT(T_OR)))
			break;

		if ((f = cel_parse_expr_and(par, sc)) == NULL) {
			cel_expr_free(e);
			FATAL("expected expression");
		}

		if ((type = cel_derive_binary_type(cel_op_or, e->ce_type, f->ce_type)) == NULL) {
		char	a1[64], a2[64];
		char	err[128];

			cel_name_type(e->ce_type, a1, sizeof(a1) / sizeof(char));
			cel_name_type(f->ce_type, a2, sizeof(a2) / sizeof(char));

			snprintf(err, sizeof(err) / sizeof(char),
				 "incompatible types in expression: \"%s\", \"%s\"",
				 a1, a2);

			cel_expr_free(e);
			cel_expr_free(f);
			ERROR_TOK(&op_tok, err);
		}

		e = cel_make_or(e, f);
		e->ce_type = type;
	}

	return e;
}

cel_expr_t *
cel_parse_expr_and(par, sc)
	cel_scope_t	*sc;
	cel_parser_t	*par;
{
cel_expr_t	*e, *f;
cel_token_t	 op_tok;
int		 op;

	if ((e = cel_parse_expr_xor(par, sc)) == NULL)
		return NULL;

	for (;;) {
	cel_type_t	*type;

		op_tok = par->cp_tok;

		if (!(op = ACCEPT(T_AND)))
			break;

		if ((f = cel_parse_expr_xor(par, sc)) == NULL) {
			cel_expr_free(e);
			FATAL("expected expression");
		}

		if ((type = cel_derive_binary_type(cel_op_and, e->ce_type, f->ce_type)) == NULL) {
		char	a1[64], a2[64];
		char	err[128];

			cel_name_type(e->ce_type, a1, sizeof(a1) / sizeof(char));
			cel_name_type(f->ce_type, a2, sizeof(a2) / sizeof(char));

			snprintf(err, sizeof(err) / sizeof(char),
				 "incompatible types in expression: \"%s\", \"%s\"",
				 a1, a2);

			cel_expr_free(e);
			cel_expr_free(f);
			ERROR_TOK(&op_tok, err);
		}

		e = cel_make_and(e, f);
		e->ce_type = type;
	}

	return e;
}

cel_expr_t *
cel_parse_expr_xor(par, sc)
	cel_scope_t	*sc;
	cel_parser_t	*par;
{
cel_expr_t	*e, *f;
cel_token_t	 op_tok;
int		 op;

	if ((e = cel_parse_expr_eq1(par, sc)) == NULL)
		return NULL;

	for (;;) {
	cel_type_t	*type;

		op_tok = par->cp_tok;

		if (!(op = ACCEPT(T_CARET)))
			break;

		if ((f = cel_parse_expr_eq1(par, sc)) == NULL) {
			cel_expr_free(e);
			FATAL("expected expression");
		}

		if ((type = cel_derive_binary_type(cel_op_xor, e->ce_type, f->ce_type)) == NULL) {
		char	a1[64], a2[64];
		char	err[128];

			cel_name_type(e->ce_type, a1, sizeof(a1) / sizeof(char));
			cel_name_type(f->ce_type, a2, sizeof(a2) / sizeof(char));

			snprintf(err, sizeof(err) / sizeof(char),
				 "incompatible types in expression: \"%s\", \"%s\"",
				 a1, a2);

			cel_expr_free(e);
			cel_expr_free(f);
			ERROR_TOK(&op_tok, err);
		}

		e = cel_make_xor(e, f);
		e->ce_type = type;
	}

	return e;
}

cel_expr_t *
cel_parse_expr_eq1(par, sc)
	cel_scope_t	*sc;
	cel_parser_t	*par;
{
cel_expr_t	*e, *f;
cel_token_t	 op_tok;
int		 op;

	if ((e = cel_parse_expr_eq2(par, sc)) == NULL)
		return NULL;

	for (;;) {
	cel_type_t	*type;
	cel_bi_oper_t	 oper;

		op_tok = par->cp_tok;

		if (!(op = ACCEPT(T_EQ)) && !(op = ACCEPT(T_NEQ)))
			break;

		if ((f = cel_parse_expr_eq2(par, sc)) == NULL) {
			cel_expr_free(e);
			FATAL("expected expression");
		}

		switch (op) {
		case T_EQ:	oper = cel_op_eq; break;
		case T_NEQ:	oper = cel_op_neq; break;
		}

		if ((type = cel_derive_binary_type(oper, e->ce_type, f->ce_type)) == NULL) {
		char	a1[64], a2[64];
		char	err[128];

			cel_name_type(e->ce_type, a1, sizeof(a1) / sizeof(char));
			cel_name_type(f->ce_type, a2, sizeof(a2) / sizeof(char));

			snprintf(err, sizeof(err) / sizeof(char),
				 "incompatible types in expression: \"%s\", \"%s\"",
				 a1, a2);

			cel_expr_free(e);
			cel_expr_free(f);
			ERROR_TOK(&op_tok, err);
		}

		e = cel_make_binary(oper, e, f);
		e->ce_type = type;
	}

	return e;
}

cel_expr_t *
cel_parse_expr_eq2(par, sc)
	cel_scope_t	*sc;
	cel_parser_t	*par;
{
cel_expr_t	*e, *f;
cel_token_t	 op_tok;
int		 op;

	if ((e = cel_parse_expr_plus(par, sc)) == NULL)
		return NULL;

	for (;;) {
	cel_type_t	*type;
	cel_bi_oper_t	 oper;

		op_tok = par->cp_tok;

		if (!(op = ACCEPT(T_GT)) && !(op = ACCEPT(T_GE)) &&
		    !(op = ACCEPT(T_LT)) && !(op = ACCEPT(T_LE)))
			break;

		if ((f = cel_parse_expr_plus(par, sc)) == NULL) {
			cel_expr_free(e);
			FATAL("expected expression");
		}

		switch (op) {
		case T_GT:	oper = cel_op_gt; break;
		case T_GE:	oper = cel_op_ge; break;
		case T_LT:	oper = cel_op_lt; break;
		case T_LE:	oper = cel_op_le; break;
		}

		if ((type = cel_derive_binary_type(oper, e->ce_type, f->ce_type)) == NULL) {
		char	a1[64], a2[64];
		char	err[128];

			cel_name_type(e->ce_type, a1, sizeof(a1) / sizeof(char));
			cel_name_type(f->ce_type, a2, sizeof(a2) / sizeof(char));

			snprintf(err, sizeof(err) / sizeof(char),
				 "incompatible types in expression: \"%s\", \"%s\"",
				 a1, a2);

			cel_expr_free(e);
			cel_expr_free(f);
			ERROR_TOK(&op_tok, err);
		}

		e = cel_make_binary(oper, e, f);
		e->ce_type = type;
	}

	return e;
}

cel_expr_t *
cel_parse_expr_plus(par, sc)
	cel_parser_t	*par;
	cel_scope_t	*sc;
{
cel_expr_t	*e, *f;
cel_token_t	 op_tok;
int		 op;

	if ((e = cel_parse_expr_mult(par, sc)) == NULL)
		return NULL;

	for (;;) {
	cel_type_t	*type;
	cel_bi_oper_t	 oper;

		op_tok = par->cp_tok;

		if (!(op = ACCEPT(T_PLUS)) && !(op = ACCEPT(T_MINUS)))
			break;

		if ((f = cel_parse_expr_mult(par, sc)) == NULL) {
			cel_expr_free(e);
			FATAL("expected expression");
		}

		switch (op) {
		case T_PLUS:	oper = cel_op_plus; break;
		case T_MINUS:	oper = cel_op_minus; break;
		}

		if ((type = cel_derive_binary_type(oper, e->ce_type, f->ce_type)) == NULL) {
		char	a1[64], a2[64];
		char	err[128];

			cel_name_type(e->ce_type, a1, sizeof(a1) / sizeof(char));
			cel_name_type(f->ce_type, a2, sizeof(a2) / sizeof(char));

			snprintf(err, sizeof(err) / sizeof(char),
				 "incompatible types in expression: \"%s\", \"%s\"",
				 a1, a2);

			cel_expr_free(e);
			cel_expr_free(f);
			ERROR_TOK(&op_tok, err);
		}

		e = cel_make_binary(oper, e, f);
		e->ce_type = type;
	}

	return e;
}

cel_expr_t *
cel_parse_expr_mult(par, sc)
	cel_parser_t	*par;
	cel_scope_t	*sc;
{
cel_expr_t	*e, *f;
cel_token_t	 op_tok;
int		 op;

	if ((e = cel_parse_expr_unary(par, sc)) == NULL) {
		return NULL;
	}

	for (;;) {
	cel_type_t	*type;
	cel_bi_oper_t	 oper;

		op_tok = par->cp_tok;

		if (!(op = ACCEPT(T_STAR)) && !(op = ACCEPT(T_SLASH)) &&
		    !(op = ACCEPT(T_PERCENT)))
			break;

		if ((f = cel_parse_expr_unary(par, sc)) == NULL) {
			cel_expr_free(e);
			FATAL("expected expression");
		}

		switch (op) {
		case T_STAR:	oper = cel_op_mult; break;
		case T_SLASH:	oper = cel_op_div; break;
		case T_PERCENT:	oper = cel_op_modulus; break;
		}

		if ((type = cel_derive_binary_type(oper, e->ce_type, f->ce_type)) == NULL) {
		char	a1[64], a2[64];
		char	err[128];

			cel_name_type(e->ce_type, a1, sizeof(a1) / sizeof(char));
			cel_name_type(f->ce_type, a2, sizeof(a2) / sizeof(char));

			snprintf(err, sizeof(err) / sizeof(char),
				 "incompatible types in expression: \"%s\", \"%s\"",
				 a1, a2);

			cel_expr_free(e);
			cel_expr_free(f);
			ERROR_TOK(&op_tok, err);
		}

		e = cel_make_binary(oper, e, f);
		e->ce_type = type;
	}

	return e;
}

cel_expr_t *
cel_parse_expr_unary(par, sc)
	cel_parser_t	*par;
	cel_scope_t	*sc;
{
/* expr_unary  --> expr_post  | "-" expr_unary | "!" expr_unary */
cel_expr_t	*e = NULL;

	for (;;) {
	cel_type_t	*type;
	cel_uni_oper_t	 oper;
	int		 op;
	cel_token_t	 op_tok;

		op_tok = par->cp_tok;

		if (!(op = ACCEPT(T_MINUS)) && !(op = ACCEPT(T_NEGATE)))
			break;

		if ((e = cel_parse_expr_unary(par, sc)) == NULL) {
			cel_expr_free(e);
			FATAL("expected expression");
		}

		switch (op) {
		case T_NEGATE:	oper = cel_op_negate; break;
		case T_MINUS:	oper = cel_op_uni_minus; break;
		}

		if ((type = cel_derive_unary_type(oper, e->ce_type)) == NULL) {
		char	a1[64], err[128];

			cel_name_type(e->ce_type, a1, sizeof(a1) / sizeof(char));

			snprintf(err, sizeof(err) / sizeof(char),
				 "incompatible type in expression: \"%s\"", a1);

			cel_expr_free(e);
			ERROR_TOK(&op_tok, err);
		}

		e = cel_make_unary(oper, e);
		e->ce_type = type;
	}

	if (e)
		return e;

	return cel_parse_expr_post(par, sc);
}

cel_expr_t *
cel_parse_expr_post(par, sc)
	cel_parser_t	*par;
	cel_scope_t	*sc;
{
/* expr_post   --> expr_value [ ( "(" arglist ")" | "[" expr "]" ) ] */
cel_expr_t	*e;
int		 op;
cel_token_t	 err_tok;

	if ((e = cel_parse_expr_value(par, sc)) == NULL)
		return NULL;

/* Function call */
	while ((op = ACCEPT(T_LPAR)) || (op = ACCEPT(T_LSQ)) ||
	       (op = ACCEPT(T_AS))) {
	cel_type_t	*t;
	cel_arglist_t	*args;

		switch (op) {
		case T_LPAR:
			if ((args = cel_parse_arglist(par, sc)) == NULL) {
				cel_expr_free(e);
				return NULL;
			}

			if (!ACCEPT(T_RPAR)) {
				cel_expr_free(e);
				FATAL("expected ')'");
			}

			e = cel_make_call(e, args);
			break;

		case T_LSQ:
			if (cel_parse_expr(par, sc) == NULL) {
				cel_expr_free(e);
				FATAL("expected expression");
			}

			if (!ACCEPT(T_RSQ)) {
				cel_expr_free(e);
				FATAL("expected ']'");
			}
			break;

		case T_AS:
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

cel_expr_t *
cel_parse_expr_value(par, sc)
	cel_scope_t	*sc;
	cel_parser_t	*par;
{
/* expr_value  --> value | "(" expr ")" | ifexpr */
cel_expr_t	*e;

	if ((e = cel_parse_value(par, sc)) != NULL)
		return e;

	if (ACCEPT(T_LPAR)) {
		if ((e = cel_parse_expr(par, sc)) == NULL)
			FATAL("expected expression");

		if (!ACCEPT(T_RPAR)) {
			cel_expr_free(e);
			FATAL("expected ')'");
		}

		return e;
	}

/* if expression */
	if (ACCEPT(T_IF)) {
		if ((e = cel_parse_if(par, sc)) == NULL)
			return NULL;
		return e;
	}

/* while expression */
	if (ACCEPT(T_WHILE)) {
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

	return NULL;
}

cel_expr_t *
cel_parse_value(par, sc)
	cel_parser_t	*par;
	cel_scope_t	*sc;
{
cel_expr_t	*ret = NULL;

	if (EXPECT(T_ID)) {
	cel_scope_item_t	*v;
	cel_token_t		 err_tok = par->cp_tok;

		v = cel_scope_find_item(sc, par->cp_tok.ct_literal);

		if (v == NULL) {
		char	err[128];
			snprintf(err, sizeof(err), "undeclared identifier \"%s\"",
				 par->cp_tok.ct_literal);
			CONSUME();

			ERROR_TOK(&err_tok, err);
		}

		if (v->si_type != cel_item_expr) {
		char	err[128];
			snprintf(err, sizeof(err), "not a variable type: \"%s\"",
				 par->cp_tok.ct_literal);
			CONSUME();

			ERROR_TOK(&err_tok, err);
		}

		ret = cel_make_variable(v->si_name, v->si_ob.si_expr->ce_type);
		ret->ce_mutable = v->si_ob.si_expr->ce_mutable;
	} else if (EXPECT(T_LIT_INT8))
		ret = cel_make_int8(strtol(par->cp_tok.ct_literal, NULL, 0));
	else if (EXPECT(T_LIT_UINT8))
		ret = cel_make_uint8(strtol(par->cp_tok.ct_literal, NULL, 0));
	else if (EXPECT(T_LIT_INT16))
		ret = cel_make_int16(strtol(par->cp_tok.ct_literal, NULL, 0));
	else if (EXPECT(T_LIT_UINT16))
		ret = cel_make_uint16(strtol(par->cp_tok.ct_literal, NULL, 0));
	else if (EXPECT(T_LIT_INT32))
		ret = cel_make_int32(strtol(par->cp_tok.ct_literal, NULL, 0));
	else if (EXPECT(T_LIT_UINT32))
		ret = cel_make_uint32(strtol(par->cp_tok.ct_literal, NULL, 0));
	else if (EXPECT(T_LIT_INT64))
		ret = cel_make_int64(strtol(par->cp_tok.ct_literal, NULL, 0));
	else if (EXPECT(T_LIT_UINT64))
		ret = cel_make_uint64(strtol(par->cp_tok.ct_literal, NULL, 0));
	else if (EXPECT(T_LIT_STR)) {
		ret = cel_make_string(par->cp_tok.ct_literal);
	} else if (EXPECT(T_TRUE))
		ret = cel_make_bool(1);
	else if (EXPECT(T_FALSE))
		ret = cel_make_bool(0);

	if (ret)
		CONSUME();

	return ret;
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

	while (e = cel_parse_expr(par, sc)) {
		al->ca_args = realloc(al->ca_args,
				      sizeof(cel_type_t *) * (al->ca_nargs + 1));
		al->ca_args[al->ca_nargs] = e;
		al->ca_nargs++;

		if (ACCEPT(T_COMMA))
			continue;
	}

	return al;
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
	if ((e = cel_parse_expr(par, sc)) == NULL) {
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
	if (!ACCEPT(T_THEN)) {
		cel_expr_free(ret);
		free(if_);
		FATAL("expected 'then'");
	}

	CEL_TAILQ_INIT(&if_->ib_exprs);

/* list of statements */
	for (;;) {
	cel_expr_t	*e;
		while ((e = cel_parse_stmt(par, sc)) != NULL) {
			CEL_TAILQ_INSERT_TAIL(&if_->ib_exprs, e, ce_entry);

			/*
			 * Each statement has to be followed by a semicolon,
			 * except for the last one in the block.
			 */
			if (ACCEPT(T_SEMI))
				continue;
		}
			
		if (ACCEPT(T_ELIF)) {
			CEL_TAILQ_INSERT_TAIL(e->ce_op.ce_if, if_, ib_entry);

			if ((e = cel_parse_expr(par, sc)) == NULL) {
				cel_expr_free(ret);
				FATAL("expected expression");
			}

			if (!ACCEPT(T_THEN)) {
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

		if (ACCEPT(T_ELSE)) {
			CEL_TAILQ_INSERT_TAIL(ret->ce_op.ce_if, if_, ib_entry);
			if_ = calloc(1, sizeof(*if_));
			CEL_TAILQ_INIT(&if_->ib_exprs);
			continue;
		}

		if (ACCEPT(T_END)) {
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
	if ((e = cel_parse_expr(par, sc)) == NULL) {
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
	if (!ACCEPT(T_DO)) {
		cel_expr_free(ret);
		FATAL("expected 'do'");
	}

	CEL_TAILQ_INIT(&ret->ce_op.ce_while->wh_exprs);

/* list of statements */
	for (;;) {
	cel_expr_t	*e;
		while ((e = cel_parse_stmt(par, sc)) != NULL) {
			CEL_TAILQ_INSERT_TAIL(&ret->ce_op.ce_while->wh_exprs, e, ce_entry);

			/*
			 * Each statement has to be followed by a semicolon,
			 * except for the last one in the block.
			 */
			if (ACCEPT(T_SEMI))
				continue;
		}
			
		if (ACCEPT(T_END))
			return ret;

		cel_expr_free(ret);
		FATAL("expected statement or 'end'");
	}
}
