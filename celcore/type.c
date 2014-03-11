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

#include	"celcore/type.h"

cel_type_t *
cel_make_type(t)
	cel_type_tag_t	t;
{
cel_type_t	*ret;
	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;
	ret->ct_tag = t;
	return ret;
}

cel_type_t *
cel_make_array(t)
	cel_type_t	*t;
{
cel_type_t	*ret;
	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;
	ret->ct_tag = cel_type_array;
	ret->ct_type.ct_array_type = t;
	return ret;
}

cel_typedef_t *
cel_make_typedef(name, type)
	wchar_t const	*name;
	cel_type_t	*type;
{
cel_typedef_t	*ret;
	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;
	ret->ct_type = type;
	ret->ct_name = wcsdup(name);
	return ret;
}

void
cel_type_free(t)
	cel_type_t	*t;
{
	if (t->ct_tag == cel_type_array)
		cel_type_free(t->ct_type.ct_array_type);
	free(t);
}

void
cel_name_type(type, buf, bsz)
	cel_type_t	*type;
	wchar_t		*buf;
	size_t		 bsz;
{
	*buf = 0;

	while (type->ct_tag == cel_type_array) {
		wcslcat(buf, L"[]", bsz);
		type = type->ct_type.ct_array_type;
	}

	switch (type->ct_tag) {
	case cel_type_int8:	wcslcat(buf, L"int8", bsz); break;
	case cel_type_int16:	wcslcat(buf, L"int16", bsz); break;
	case cel_type_int32:	wcslcat(buf, L"int32", bsz); break;
	case cel_type_int64:	wcslcat(buf, L"int64", bsz); break;
	case cel_type_uint8:	wcslcat(buf, L"uint8", bsz); break;
	case cel_type_uint16:	wcslcat(buf, L"uint16", bsz); break;
	case cel_type_uint32:	wcslcat(buf, L"uint32", bsz); break;
	case cel_type_uint64:	wcslcat(buf, L"uint64", bsz); break;
	case cel_type_bool:	wcslcat(buf, L"bool", bsz); break;
	case cel_type_string:	wcslcat(buf, L"string", bsz); break;
	case cel_type_function:	wcslcat(buf, L"function", bsz); break;
	default:		wcslcat(buf, L"<unknown>", bsz); break;
	}
}

cel_type_t *
cel_derive_unary_type(op, a)
	cel_uni_oper_t	 op;
	cel_type_t	*a;
{
	switch (op) {
	case cel_op_uni_minus:
		switch (a->ct_tag) {
		case cel_type_int8:
		case cel_type_int16:
		case cel_type_int32:
			return cel_make_type(cel_type_int32);

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
cel_derive_unary_promotion(op, a)
	cel_uni_oper_t	 op;
	cel_type_t	*a;
{
	switch (op) {
	case cel_op_uni_minus:
		switch (a->ct_tag) {
		case cel_type_int8:
		case cel_type_int16:
		case cel_type_int32:
			return cel_make_type(cel_type_int32);

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
#define IS_ORD(t)						\
	((t) == cel_type_int8  || (t) == cel_type_uint8  ||	\
	 (t) == cel_type_int16 || (t) == cel_type_uint16 ||	\
	 (t) == cel_type_int32 || (t) == cel_type_uint32 ||	\
	 (t) == cel_type_int64 || (t) == cel_type_uint64)

	/*
	 * These operators always return boolean types; we still do type
	 * checking on their arguments.
	 */
	switch (op) {
	case cel_op_eq:
	case cel_op_neq:
		if (a->ct_tag == cel_type_bool && b->ct_tag == cel_type_bool)
			return cel_make_type(cel_type_bool);

	case cel_op_lt:
	case cel_op_le:
	case cel_op_gt:
	case cel_op_ge:
		if (!IS_ORD(a->ct_tag) || !IS_ORD(b->ct_tag))
			return NULL;
		return cel_make_type(cel_type_bool);

	case cel_op_and:
	case cel_op_or:
		if (a->ct_tag == cel_type_bool && b->ct_tag == cel_type_bool)
			return cel_make_type(cel_type_bool);
		return NULL;

	default:
		break;
	}
#undef IS_ORD

	switch (a->ct_tag) {
	/*
	 * Anything <= 32 bits is promoted to the 32-bit type.
	 */
	case cel_type_int8:
	case cel_type_uint8:
	case cel_type_int16:
	case cel_type_uint16:
	case cel_type_int32:
		switch (b->ct_tag) {
		case cel_type_int8:	return cel_make_type(cel_type_int32);
		case cel_type_uint8:	return cel_make_type(cel_type_int32);
		case cel_type_int16:	return cel_make_type(cel_type_int32);
		case cel_type_uint16:	return cel_make_type(cel_type_int32);
		case cel_type_int32:	return cel_make_type(cel_type_int32);
		case cel_type_uint32:	return cel_make_type(cel_type_uint32);
		case cel_type_int64:	return cel_make_type(cel_type_int64);
		case cel_type_uint64:	return cel_make_type(cel_type_uint64);
		default:		return NULL;
		}

	case cel_type_uint32:
		switch (b->ct_tag) {
		case cel_type_int8:	return cel_make_type(cel_type_uint32);
		case cel_type_uint8:	return cel_make_type(cel_type_uint32);
		case cel_type_int16:	return cel_make_type(cel_type_uint32);
		case cel_type_uint16:	return cel_make_type(cel_type_uint32);
		case cel_type_int32:	return cel_make_type(cel_type_uint32);
		case cel_type_uint32:	return cel_make_type(cel_type_uint32);
		case cel_type_int64:	return cel_make_type(cel_type_int64);
		case cel_type_uint64:	return cel_make_type(cel_type_uint64);
		default:		return NULL;
		}

	case cel_type_int64:
		switch (b->ct_tag) {
		case cel_type_int8:	return cel_make_type(cel_type_int64);
		case cel_type_uint8:	return cel_make_type(cel_type_int64);
		case cel_type_int16:	return cel_make_type(cel_type_int64);
		case cel_type_uint16:	return cel_make_type(cel_type_int64);
		case cel_type_int32:	return cel_make_type(cel_type_int64);
		case cel_type_uint32:	return cel_make_type(cel_type_int64);
		case cel_type_int64:	return cel_make_type(cel_type_int64);
		case cel_type_uint64:	return cel_make_type(cel_type_uint64);
		default:		return NULL;
		}

	case cel_type_uint64:
		switch (b->ct_tag) {
		case cel_type_int8:	return cel_make_type(cel_type_uint64);
		case cel_type_uint8:	return cel_make_type(cel_type_uint64);
		case cel_type_int16:	return cel_make_type(cel_type_uint64);
		case cel_type_uint16:	return cel_make_type(cel_type_uint64);
		case cel_type_int32:	return cel_make_type(cel_type_uint64);
		case cel_type_uint32:	return cel_make_type(cel_type_uint64);
		case cel_type_int64:	return cel_make_type(cel_type_uint64);
		case cel_type_uint64:	return cel_make_type(cel_type_uint64);
		default:		return NULL;
		}

	/*
	 * The only valid operations on strings is + (concat).
	 */
	case cel_type_string:
		if (op == cel_op_plus && b->ct_tag == cel_type_string)
			return cel_make_type(cel_type_string);
		return NULL;

	/*
	 * Bools can only be compared for equality with each other; not with
	 * integer types, as that almost certainly doesn't make sense.
	 */
	case cel_type_bool:
		if ((op == cel_op_eq || op == cel_op_neq) &&
		    b->ct_tag == cel_type_bool)
			return cel_make_type(cel_type_bool);
		return NULL;

	default:
		return NULL;
	}
}

cel_type_t *
cel_derive_binary_promotion(op, a, b)
	cel_bi_oper_t	 op;
	cel_type_t	*a, *b;
{
	switch (op) {
	case cel_op_and:
	case cel_op_or:
		if (a->ct_tag == cel_type_bool && b->ct_tag == cel_type_bool)
			return cel_make_type(cel_type_bool);
		return NULL;

	default:
		break;
	}

	switch (a->ct_tag) {
	/*
	 * Anything <= 32 bits is promoted to the 32-bit type.
	 */
	case cel_type_int8:
	case cel_type_uint8:
	case cel_type_int16:
	case cel_type_uint16:
	case cel_type_int32:
		switch (b->ct_tag) {
		case cel_type_int8:	return cel_make_type(cel_type_int32);
		case cel_type_uint8:	return cel_make_type(cel_type_int32);
		case cel_type_int16:	return cel_make_type(cel_type_int32);
		case cel_type_uint16:	return cel_make_type(cel_type_int32);
		case cel_type_int32:	return cel_make_type(cel_type_int32);
		case cel_type_uint32:	return cel_make_type(cel_type_uint32);
		case cel_type_int64:	return cel_make_type(cel_type_int64);
		case cel_type_uint64:	return cel_make_type(cel_type_uint64);
		default:		return NULL;
		}

	case cel_type_uint32:
		switch (b->ct_tag) {
		case cel_type_int8:	return cel_make_type(cel_type_uint32);
		case cel_type_uint8:	return cel_make_type(cel_type_uint32);
		case cel_type_int16:	return cel_make_type(cel_type_uint32);
		case cel_type_uint16:	return cel_make_type(cel_type_uint32);
		case cel_type_int32:	return cel_make_type(cel_type_uint32);
		case cel_type_uint32:	return cel_make_type(cel_type_uint32);
		case cel_type_int64:	return cel_make_type(cel_type_int64);
		case cel_type_uint64:	return cel_make_type(cel_type_uint64);
		default:		return NULL;
		}

	case cel_type_int64:
		switch (b->ct_tag) {
		case cel_type_int8:	return cel_make_type(cel_type_int64);
		case cel_type_uint8:	return cel_make_type(cel_type_int64);
		case cel_type_int16:	return cel_make_type(cel_type_int64);
		case cel_type_uint16:	return cel_make_type(cel_type_int64);
		case cel_type_int32:	return cel_make_type(cel_type_int64);
		case cel_type_uint32:	return cel_make_type(cel_type_int64);
		case cel_type_int64:	return cel_make_type(cel_type_int64);
		case cel_type_uint64:	return cel_make_type(cel_type_uint64);
		default:		return NULL;
		}

	case cel_type_uint64:
		switch (b->ct_tag) {
		case cel_type_int8:	return cel_make_type(cel_type_uint64);
		case cel_type_uint8:	return cel_make_type(cel_type_uint64);
		case cel_type_int16:	return cel_make_type(cel_type_uint64);
		case cel_type_uint16:	return cel_make_type(cel_type_uint64);
		case cel_type_int32:	return cel_make_type(cel_type_uint64);
		case cel_type_uint32:	return cel_make_type(cel_type_uint64);
		case cel_type_int64:	return cel_make_type(cel_type_uint64);
		case cel_type_uint64:	return cel_make_type(cel_type_uint64);
		default:		return NULL;
		}

	/*
	 * The only valid operations on strings is + (concat).
	 */
	case cel_type_string:
		if (op == cel_op_plus && b->ct_tag == cel_type_string)
			return cel_make_type(cel_type_string);
		return NULL;

	/*
	 * Bools can only be compared for equality with each other; not with
	 * integer types, as that almost certainly doesn't make sense.
	 */
	case cel_type_bool:
		if ((op == cel_op_eq || op == cel_op_neq) &&
		    b->ct_tag == cel_type_bool)
			return cel_make_type(cel_type_bool);
		return NULL;

	default:
		return NULL;
	}
}
