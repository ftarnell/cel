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

#include	"celcore/type.h"

static cel_type_t *types[cel_last_type];

cel_type_t *
cel_make_type(t)
	cel_type_tag_t	t;
{
#if 0
	if (types[t])
		return types[t];
#endif
	types[t] = malloc(sizeof(cel_type_t));
	types[t]->ct_const = 1;
	types[t]->ct_tag = t;
	return types[t];
}

cel_type_t *
cel_make_array(t)
	cel_type_t	*t;
{
cel_type_t	*ret;
	if ((ret = malloc(sizeof(*ret))) == NULL)
		return NULL;
	ret->ct_const = 0;
	ret->ct_tag = cel_type_array;
	ret->ct_type.ct_array_type = t;
	return ret;
}

cel_type_t *
cel_make_ptr(t)
	cel_type_t	*t;
{
cel_type_t	*ret;
	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;
	ret->ct_tag = cel_type_ptr;
	ret->ct_type.ct_ptr_type = t;
	return ret;
}

cel_typedef_t *
cel_make_typedef(name, type)
	char const	*name;
	cel_type_t	*type;
{
cel_typedef_t	*ret;
	if ((ret = malloc(sizeof(*ret))) == NULL)
		return NULL;
	ret->ct_type = type;
	ret->ct_name = strdup(name);
	return ret;
}

void
cel_type_free(t)
	cel_type_t	*t;
{
	if (t->ct_const)
		return;

	if (t->ct_tag == cel_type_array)
		cel_type_free(t->ct_type.ct_array_type);
	free(t);
}

void
cel_name_function(type, buf, bsz)
	cel_type_t	*type;
	char		*buf;
	size_t		 bsz;
{
cel_type_t	*u;
char		 buf_[64] = {};

	strlcpy(buf, "((", bsz);

	CEL_TAILQ_FOREACH(u, type->ct_type.ct_function.ct_args, ct_entry) {
		buf_[0] = 0;
		cel_name_type(u, buf_, sizeof(buf_));
		strlcat(buf, buf_, bsz);
		strlcat(buf, ", ", bsz);
	}

	if (buf[2])
		buf[strlen(buf) - 2] = '\0';
	strlcat(buf, ") -> ", bsz);
	buf_[0] = 0;
	cel_name_type(type->ct_type.ct_function.ct_return_type, buf_, sizeof(buf_));
	strlcat(buf, buf_, bsz);
	strlcat(buf, ")", bsz);
}

void
cel_name_type(type, buf, bsz)
	cel_type_t	*type;
	char		*buf;
	size_t		 bsz;
{
	while (type->ct_tag == cel_type_array) {
		strlcat(buf, "[]", bsz);
		type = type->ct_type.ct_array_type;
	}

	switch (type->ct_tag) {
	case cel_type_int8:	strlcat(buf, "int8", bsz); break;
	case cel_type_int16:	strlcat(buf, "int16", bsz); break;
	case cel_type_int32:	strlcat(buf, "int32", bsz); break;
	case cel_type_int64:	strlcat(buf, "int64", bsz); break;
	case cel_type_uint8:	strlcat(buf, "uint8", bsz); break;
	case cel_type_uint16:	strlcat(buf, "uint16", bsz); break;
	case cel_type_uint32:	strlcat(buf, "uint32", bsz); break;
	case cel_type_uint64:	strlcat(buf, "uint64", bsz); break;
	case cel_type_sfloat:	strlcat(buf, "sfloat", bsz); break;
	case cel_type_dfloat:	strlcat(buf, "dfloat", bsz); break;
	case cel_type_bool:	strlcat(buf, "bool", bsz); break;
	case cel_type_schar:	strlcat(buf, "schar", bsz); break;
	case cel_type_uchar:	strlcat(buf, "uchar", bsz); break;
	case cel_type_function:	return cel_name_function(type, buf, bsz);
	case cel_type_void:	strlcat(buf, "void", bsz); break;
	case cel_type_ptr:	strlcat(buf, "^", bsz);
				cel_name_type(type->ct_type.ct_ptr_type, buf, bsz);
				break;
	default:		strlcat(buf, "<unknown>", bsz); break;
	}
}

cel_type_t *
cel_derive_unary_type(op, a)
	cel_uni_oper_t	 op;
	cel_type_t	*a;
{
	switch (op) {
	case cel_op_addr:
		return cel_make_ptr(a);

	case cel_op_deref:
		if (a->ct_tag != cel_type_ptr)
			return NULL;
		return a->ct_type.ct_ptr_type;

	case cel_op_uni_minus:
		switch (a->ct_tag) {
		case cel_type_int8:
		case cel_type_int16:
		case cel_type_int32:
		case cel_type_uint8:
		case cel_type_uint16:
		case cel_type_uint32:
		case cel_type_int64:
		case cel_type_uint64:
		case cel_type_sfloat:
		case cel_type_dfloat:
		case cel_type_schar:
		case cel_type_uchar:
			return a;
		default:
			return NULL;
		}
		break;

	case cel_op_negate:
		if (a->ct_tag != cel_type_bool)
			return NULL;
		return a;

	default:
		return NULL;
	}
}

cel_type_t *
cel_derive_unary_promotion(op, a)
	cel_uni_oper_t	 op;
	cel_type_t	*a;
{
	switch (op) {
	case cel_op_addr:
		return cel_make_ptr(a);

	case cel_op_deref:
		return a->ct_type.ct_ptr_type;

	case cel_op_uni_minus:
		switch (a->ct_tag) {
		case cel_type_schar:
		case cel_type_int8:
		case cel_type_int16:
		case cel_type_int32:
			return cel_make_type(cel_type_int32);

		case cel_type_uchar:
		case cel_type_uint8:
		case cel_type_uint16:
		case cel_type_uint32:
			return cel_make_type(cel_type_uint32);

		case cel_type_int64:
			return cel_make_type(cel_type_int64);

		case cel_type_uint64:
			return cel_make_type(cel_type_uint64);

		default:
			return NULL;
		}
		break;

	case cel_op_negate:
		if (a->ct_tag != cel_type_bool)
			return NULL;
		return cel_make_type(cel_type_bool);

	default:
		return NULL;
	}
}

cel_type_t *
cel_derive_binary_type(op, a, b)
	cel_bi_oper_t	 op;
	cel_type_t	*a, *b;
{
	switch (a->ct_tag) {
	/*
	 * Bools can only be compared for equality with each other; not with
	 * integer types, as that almost certainly doesn't make sense.
	 */
	case cel_type_bool:
		if ((op == cel_op_eq || op == cel_op_neq) &&
		    b->ct_tag == cel_type_bool)
			return cel_make_type(cel_type_bool);
		return NULL;

	case cel_type_function:
		if (cel_type_convertable(a, b))
			return a;
		return NULL;

	default:
		break;
	}

	if (a->ct_tag >= cel_last_int_type ||
	    b->ct_tag >= cel_last_int_type)
		return NULL;

	switch (op) {
		/* These always return a bool */
	case cel_op_and:
	case cel_op_or:
		if (a->ct_tag == cel_type_bool && b->ct_tag == cel_type_bool)
			return cel_make_type(cel_type_bool);
		return NULL;

		/* These return their argument type */
	case cel_op_plus:
	case cel_op_minus:
	case cel_op_mult:
	case cel_op_div:
	case cel_op_incr:
	case cel_op_decr:
	case cel_op_lshift:
	case cel_op_rshift:
	case cel_op_bit_and:
	case cel_op_bit_or:
	case cel_op_bit_not:
		if (cel_type_is_integer(a->ct_tag)) {
		cel_type_tag_t	pa, pb;

			if (!cel_type_is_integer(b->ct_tag))
				return NULL;

			pa = cel_type_promote(a->ct_tag);
			pb = cel_type_promote(b->ct_tag);

			/*
			 * C integer conversion rules:
			 */

			/* 1. If both operands have the same type, no further 
			 * conversion is needed */
			if (pa == b->ct_tag)
				return cel_make_type(pa);

			/* 2. If both operands are of the same integer type (signed or
			 * unsigned), the operand with the type of lesser integer
			 * conversion rank is converted to the type of the operand with
			 * greater rank.
			 */
			if (cel_type_is_signed(pa) == cel_type_is_signed(pb))
				return cel_make_type(cel_type_rank(pa) < 
						     cel_type_rank(pb)
						     ? pb : pa);

			/* 3. If the operand that has unsigned integer type has rank
			 * greater than or equal to the rank of the type of the other
			 * operand, the operand with signed integer type is converted
			 * to the type of the operand with unsigned integer type.
			 */
			if (cel_type_is_unsigned(pa) &&
			    (cel_type_rank(pa) >= cel_type_rank(pb)))
				return cel_make_type(pa);

			if (cel_type_is_unsigned(pb) &&
			    (cel_type_rank(pb) >= cel_type_rank(pa)))
				return cel_make_type(pb);

			/* 4. If the type of the operand with signed integer type can
			 * represent all of the values of the type of the operand with
			 * unsigned integer type, the operand with unsigned integer
			 * type is converted to the type of the operand with signed
			 * integer type.
			 */
			if (cel_type_is_signed(pa) &&
			    (cel_type_rank(pa) > cel_type_rank(pb)))
				return cel_make_type(pa);

			if (cel_type_is_signed(pb) &&
			    (cel_type_rank(pb) > cel_type_rank(pa)))
				return cel_make_type(pb);

			/* 5. Otherwise, both operands are converted to the unsigned
			 * integer type corresponding to the type of the operand with
			 * signed integer type.
			 */
			return cel_make_type(cel_type_is_signed(pa) ? pb : pa);
		}
		break;

	default:
		return NULL;
	}

	return NULL;
}

int
cel_func_convertable(lhs, rhs)
	cel_type_t	*lhs, *rhs;
{
cel_type_t	*u, *v;

	for (u = CEL_TAILQ_FIRST(lhs->ct_type.ct_function.ct_args),
	     v = CEL_TAILQ_FIRST(rhs->ct_type.ct_function.ct_args);
	     u && v;
	     u = CEL_TAILQ_NEXT(u, ct_entry),
	     v = CEL_TAILQ_NEXT(v, ct_entry)) {
		if (!cel_type_convertable(u, v))
			return 0;
	}

	if (!u != !v)
		return 0;

	if (!cel_type_convertable(lhs->ct_type.ct_function.ct_return_type,
				  rhs->ct_type.ct_function.ct_return_type))
		return 0;

	return 1;
}

int
cel_type_convertable(lhs, rhs)
	cel_type_t	*lhs, *rhs;
{
	switch (lhs->ct_tag) {
	case cel_type_schar:
	case cel_type_uchar:
	case cel_type_int8:
	case cel_type_uint8:
	case cel_type_int16:
	case cel_type_uint16:
	case cel_type_int32:
	case cel_type_uint32:
	case cel_type_int64:
	case cel_type_uint64:
		switch (rhs->ct_tag) {
		case cel_type_int8:
		case cel_type_uint8:
		case cel_type_int16:
		case cel_type_uint16:
		case cel_type_int32:
		case cel_type_uint32:
		case cel_type_int64:
		case cel_type_uint64:
		case cel_type_schar:
		case cel_type_uchar:
			return 1;
		default:
			return 0;
		}
		break;

	case cel_type_bool:
		return (rhs->ct_tag == cel_type_bool);

	case cel_type_function:
		return cel_func_convertable(lhs, rhs);

	case cel_type_ptr:
		if (rhs->ct_tag != cel_type_ptr)
			return 0;
		return cel_type_convertable(lhs->ct_type.ct_ptr_type,
					    rhs->ct_type.ct_ptr_type);

	case cel_type_void:
	case cel_type_array:
		return 0;
	}

	return 0;
}
