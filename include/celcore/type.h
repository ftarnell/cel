/*
 * CEL: The Compact Embeddable Language.
 * Copyright (c) 2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#ifndef	CEL_TYPE_H
#define	CEL_TYPE_H

#include	"celcore/expr.h"
#include	"celcore/tailq.h"

typedef enum cel_type_tag {
	cel_type_bool,
	cel_type_schar,
	cel_type_uchar,
	cel_type_int8,
	cel_type_uint8,
	cel_type_int16,
	cel_type_uint16,
	cel_type_int32,
	cel_type_uint32,
	cel_type_int64,
	cel_type_uint64,
	cel_last_int_type,
	cel_type_sfloat,
	cel_type_dfloat,
	cel_type_qfloat,
	cel_type_void,
	cel_type_array,
	cel_type_ptr,
	cel_type_function,
	cel_last_type
} cel_type_tag_t;

#define	cel_type_rank(tag)						\
	(((tag) == cel_type_bool)			       ? 0 :	\
	 ((tag) == cel_type_schar || (tag) == cel_type_uchar)  ? 1 :	\
	 ((tag) == cel_type_int8  || (tag) == cel_type_uint8)  ? 2 :	\
	 ((tag) == cel_type_int16 || (tag) == cel_type_uint16) ? 3 :	\
	 ((tag) == cel_type_int32 || (tag) == cel_type_uint32) ? 4 :	\
	 ((tag) == cel_type_int64 || (tag) == cel_type_uint64) ? 5 :	\
	 -1)

#define	cel_type_is_signed(tag)						\
	((tag) == cel_type_int8  || (tag) == cel_type_int16  ||		\
	 (tag) == cel_type_int32 || (tag) == cel_type_int64  ||		\
	 (tag) == cel_type_schar)

#define	cel_type_is_unsigned(tag) (!cel_type_is_signed(tag))

#define	cel_type_is_integer(tag)					\
	((tag) == cel_type_schar  || (tag) == cel_type_uchar   ||	\
	 (tag) == cel_type_int8   || (tag) == cel_type_int16   ||	\
	 (tag) == cel_type_uint8  || (tag) == cel_type_uint16  ||	\
	 (tag) == cel_type_int32  || (tag) == cel_type_int64   ||	\
	 (tag) == cel_type_uint32 || (tag) == cel_type_uint64)

#define	cel_type_promote(tag)						\
	((tag) < cel_type_int32 ? cel_type_int32 : tag)

struct cel_type_list;

typedef struct cel_type {
	cel_type_tag_t	ct_tag;
	int		ct_const;

	union {
		struct cel_type	*ct_array_type;
		struct cel_type	*ct_ptr_type;

		struct {
			struct cel_type		*ct_return_type;
			struct cel_type_list	*ct_args;
			int			 ct_nargs;
		} ct_function;
	} ct_type;

	CEL_TAILQ_ENTRY(cel_type) ct_entry;
} cel_type_t;

typedef CEL_TAILQ_HEAD(cel_type_list, cel_type) cel_type_list_t;

void		 cel_type_free(cel_type_t *);

cel_type_t	*cel_make_type(cel_type_tag_t);
cel_type_t	*cel_make_array(cel_type_t *);
cel_type_t	*cel_make_ptr(cel_type_t *);

typedef struct cel_typedef {
	cel_type_t	*ct_type;
	char const	*ct_name;
} cel_typedef_t;

cel_typedef_t	*cel_make_typedef(char const *name, cel_type_t *type);

/*
 * Return the type that results from the given binary operation being
 * applied to the supplied values.
 */
cel_type_t	*cel_derive_unary_type(cel_uni_oper_t op, cel_type_t *v);
cel_type_t	*cel_derive_binary_type(cel_bi_oper_t op, cel_type_t *a, cel_type_t *b);

int		 cel_type_convertable(cel_type_t *lhs, cel_type_t *rhs);

void		 cel_name_type(cel_type_t *type, char *buf, size_t bsz);

#endif	/* !CEL_TYPE_H */
