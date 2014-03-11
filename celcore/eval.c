/*
 * CEL: The Compact Embeddable Language.
 * Copyright (c) 2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#include	"celcore/eval.h"
#include	"celcore/type.h"

static cel_expr_t *
cel_eval_unary(e)
	cel_expr_t	*e;
{
cel_expr_t	*v, *pv, *ret = NULL;
cel_type_t	*ptype;

	if ((v = cel_eval(e->ce_op.ce_unary.operand)) == NULL)
		return NULL;

	if ((ptype = cel_derive_unary_promotion(e->ce_op.ce_unary.oper,
						v->ce_type)) == NULL) {
		cel_expr_free(v);
		return NULL;
	}

	if ((pv = cel_promote_expr(ptype, v)) == NULL) {
		cel_expr_free(v);
		cel_type_free(ptype);
		return NULL;
	}

	cel_expr_free(v);
	v = NULL;

	switch (e->ce_op.ce_unary.oper) {
	case cel_op_negate:
		ret = cel_make_bool(!pv->ce_op.ce_bool);
		break;

	case cel_op_uni_minus:
		switch (ptype->ct_tag) {
		case cel_type_int8:	ret = cel_make_int8(-pv->ce_op.ce_int8); break;
		case cel_type_uint8:	ret = cel_make_uint8(-pv->ce_op.ce_uint8); break;
		case cel_type_int16:	ret = cel_make_int16(-pv->ce_op.ce_int16); break;
		case cel_type_uint16:	ret = cel_make_uint16(-pv->ce_op.ce_uint16); break;
		case cel_type_int32:	ret = cel_make_int32(-pv->ce_op.ce_int32); break;
		case cel_type_uint32:	ret = cel_make_uint32(-pv->ce_op.ce_uint32); break;
		case cel_type_int64:	ret = cel_make_int64(-pv->ce_op.ce_int64); break;
		case cel_type_uint64:	ret = cel_make_uint64(-pv->ce_op.ce_uint64); break;
		default:
			break;
		}
	}

	return ret;
}

static cel_expr_t *
cel_eval_binary(e)
	cel_expr_t	*e;
{
cel_expr_t	*l, *r, *pl, *pr, *ret = NULL;
cel_type_t	*rtype;
wchar_t		*s;

	if ((l = cel_eval(e->ce_op.ce_binary.left)) == NULL)
		return NULL;

	if ((r = cel_eval(e->ce_op.ce_binary.right)) == NULL) {
		cel_expr_free(l);
		return NULL;
	}

	if ((rtype = cel_derive_binary_promotion(e->ce_op.ce_binary.oper,
					 l->ce_type, r->ce_type)) == NULL) {
		cel_expr_free(l);
		cel_expr_free(r);
		return NULL;
	}

	if ((pl = cel_promote_expr(rtype, l)) == NULL) {
		cel_expr_free(l);
		cel_expr_free(r);
		cel_type_free(rtype);
		return NULL;
	}

	if ((pr = cel_promote_expr(rtype, r)) == NULL) {
		cel_expr_free(l);
		cel_expr_free(r);
		cel_expr_free(pl);
		cel_type_free(rtype);
		return NULL;
	}

	cel_expr_free(l);
	cel_expr_free(r);
	l = r = NULL;

	switch (e->ce_op.ce_binary.oper) {
	case cel_op_plus:
		switch (rtype->ct_tag) {
		case cel_type_string:
			s = malloc(sizeof(wchar_t) * (wcslen(pl->ce_op.ce_string) +
						      wcslen(pr->ce_op.ce_string) + 1));
			wcscpy(s, pl->ce_op.ce_string);
			wcscat(s, pr->ce_op.ce_string);
			ret = cel_make_string(s);
			free(s);
			break;

		case cel_type_int8:	ret = cel_make_int8(pl->ce_op.ce_int8 + pr->ce_op.ce_int8); break;
		case cel_type_uint8:	ret = cel_make_uint8(pl->ce_op.ce_uint8 + pr->ce_op.ce_uint8); break;
		case cel_type_int16:	ret = cel_make_int16(pl->ce_op.ce_int16 + pr->ce_op.ce_int16); break;
		case cel_type_uint16:	ret = cel_make_uint16(pl->ce_op.ce_uint16 + pr->ce_op.ce_uint16); break;
		case cel_type_int32:	ret = cel_make_int32(pl->ce_op.ce_int32 + pr->ce_op.ce_int32); break;
		case cel_type_uint32:	ret = cel_make_uint32(pl->ce_op.ce_uint32 + pr->ce_op.ce_uint32); break;
		case cel_type_int64:	ret = cel_make_int64(pl->ce_op.ce_int64 + pr->ce_op.ce_int64); break;
		case cel_type_uint64:	ret = cel_make_uint64(pl->ce_op.ce_uint64 + pr->ce_op.ce_uint64); break;
		default:		break;
		}
		break;

	case cel_op_minus:
		switch (rtype->ct_tag) {
		case cel_type_int8:	ret = cel_make_int8(pl->ce_op.ce_int8 - pr->ce_op.ce_int8); break;
		case cel_type_uint8:	ret = cel_make_uint8(pl->ce_op.ce_uint8 - pr->ce_op.ce_uint8); break;
		case cel_type_int16:	ret = cel_make_int16(pl->ce_op.ce_int16 - pr->ce_op.ce_int16); break;
		case cel_type_uint16:	ret = cel_make_uint16(pl->ce_op.ce_uint16 - pr->ce_op.ce_uint16); break;
		case cel_type_int32:	ret = cel_make_int32(pl->ce_op.ce_int32 - pr->ce_op.ce_int32); break;
		case cel_type_uint32:	ret = cel_make_uint32(pl->ce_op.ce_uint32 - pr->ce_op.ce_uint32); break;
		case cel_type_int64:	ret = cel_make_int64(pl->ce_op.ce_int64 - pr->ce_op.ce_int64); break;
		case cel_type_uint64:	ret = cel_make_uint64(pl->ce_op.ce_uint64 - pr->ce_op.ce_uint64); break;
		default:		break;
		}
		break;

	case cel_op_mult:
		switch (rtype->ct_tag) {
		case cel_type_int8:	ret = cel_make_int8(pl->ce_op.ce_int8 * pr->ce_op.ce_int8); break;
		case cel_type_uint8:	ret = cel_make_uint8(pl->ce_op.ce_uint8 * pr->ce_op.ce_uint8); break;
		case cel_type_int16:	ret = cel_make_int16(pl->ce_op.ce_int16 * pr->ce_op.ce_int16); break;
		case cel_type_uint16:	ret = cel_make_uint16(pl->ce_op.ce_uint16 * pr->ce_op.ce_uint16); break;
		case cel_type_int32:	ret = cel_make_int32(pl->ce_op.ce_int32 * pr->ce_op.ce_int32); break;
		case cel_type_uint32:	ret = cel_make_uint32(pl->ce_op.ce_uint32 * pr->ce_op.ce_uint32); break;
		case cel_type_int64:	ret = cel_make_int64(pl->ce_op.ce_int64 * pr->ce_op.ce_int64); break;
		case cel_type_uint64:	ret = cel_make_uint64(pl->ce_op.ce_uint64 * pr->ce_op.ce_uint64); break;
		default:		break;
		}
		break;

	case cel_op_div:
		switch (rtype->ct_tag) {
		case cel_type_int8:	ret = cel_make_int8(pl->ce_op.ce_int8 / pr->ce_op.ce_int8); break;
		case cel_type_uint8:	ret = cel_make_uint8(pl->ce_op.ce_uint8 / pr->ce_op.ce_uint8); break;
		case cel_type_int16:	ret = cel_make_int16(pl->ce_op.ce_int16 / pr->ce_op.ce_int16); break;
		case cel_type_uint16:	ret = cel_make_uint16(pl->ce_op.ce_uint16 / pr->ce_op.ce_uint16); break;
		case cel_type_int32:	ret = cel_make_int32(pl->ce_op.ce_int32 / pr->ce_op.ce_int32); break;
		case cel_type_uint32:	ret = cel_make_uint32(pl->ce_op.ce_uint32 / pr->ce_op.ce_uint32); break;
		case cel_type_int64:	ret = cel_make_int64(pl->ce_op.ce_int64 / pr->ce_op.ce_int64); break;
		case cel_type_uint64:	ret = cel_make_uint64(pl->ce_op.ce_uint64 / pr->ce_op.ce_uint64); break;
		default:		break;
		}
		break;

	case cel_op_modulus:
		switch (rtype->ct_tag) {
		case cel_type_int8:	ret = cel_make_int8(pl->ce_op.ce_int8 % pr->ce_op.ce_int8); break;
		case cel_type_uint8:	ret = cel_make_uint8(pl->ce_op.ce_uint8 % pr->ce_op.ce_uint8); break;
		case cel_type_int16:	ret = cel_make_int16(pl->ce_op.ce_int16 % pr->ce_op.ce_int16); break;
		case cel_type_uint16:	ret = cel_make_uint16(pl->ce_op.ce_uint16 % pr->ce_op.ce_uint16); break;
		case cel_type_int32:	ret = cel_make_int32(pl->ce_op.ce_int32 % pr->ce_op.ce_int32); break;
		case cel_type_uint32:	ret = cel_make_uint32(pl->ce_op.ce_uint32 % pr->ce_op.ce_uint32); break;
		case cel_type_int64:	ret = cel_make_int64(pl->ce_op.ce_int64 % pr->ce_op.ce_int64); break;
		case cel_type_uint64:	ret = cel_make_uint64(pl->ce_op.ce_uint64 % pr->ce_op.ce_uint64); break;
		default:		break;
		}
		break;

	case cel_op_xor:
		switch (rtype->ct_tag) {
		case cel_type_int8:	ret = cel_make_int8(pl->ce_op.ce_int8 ^ pr->ce_op.ce_int8); break;
		case cel_type_uint8:	ret = cel_make_uint8(pl->ce_op.ce_uint8 ^ pr->ce_op.ce_uint8); break;
		case cel_type_int16:	ret = cel_make_int16(pl->ce_op.ce_int16 ^ pr->ce_op.ce_int16); break;
		case cel_type_uint16:	ret = cel_make_uint16(pl->ce_op.ce_uint16 ^ pr->ce_op.ce_uint16); break;
		case cel_type_int32:	ret = cel_make_int32(pl->ce_op.ce_int32 ^ pr->ce_op.ce_int32); break;
		case cel_type_uint32:	ret = cel_make_uint32(pl->ce_op.ce_uint32 ^ pr->ce_op.ce_uint32); break;
		case cel_type_int64:	ret = cel_make_int64(pl->ce_op.ce_int64 ^ pr->ce_op.ce_int64); break;
		case cel_type_uint64:	ret = cel_make_uint64(pl->ce_op.ce_uint64 ^ pr->ce_op.ce_uint64); break;
		default:		break;
		}
		break;

	case cel_op_lt:
		switch (rtype->ct_tag) {
		case cel_type_int8:	ret = cel_make_bool(pl->ce_op.ce_int8   < pr->ce_op.ce_int8);   break;
		case cel_type_uint8:	ret = cel_make_bool(pl->ce_op.ce_uint8  < pr->ce_op.ce_uint8);  break;
		case cel_type_int16:	ret = cel_make_bool(pl->ce_op.ce_int16  < pr->ce_op.ce_int16);  break;
		case cel_type_uint16:	ret = cel_make_bool(pl->ce_op.ce_uint16 < pr->ce_op.ce_uint16); break;
		case cel_type_int32:	ret = cel_make_bool(pl->ce_op.ce_int32  < pr->ce_op.ce_int32);  break;
		case cel_type_uint32:	ret = cel_make_bool(pl->ce_op.ce_uint32 < pr->ce_op.ce_uint32); break;
		case cel_type_int64:	ret = cel_make_bool(pl->ce_op.ce_int64  < pr->ce_op.ce_int64);  break;
		case cel_type_uint64:	ret = cel_make_bool(pl->ce_op.ce_uint64 < pr->ce_op.ce_uint64); break;
		default:		break;
		}
		break;

	case cel_op_le:
		switch (rtype->ct_tag) {
		case cel_type_int8:	ret = cel_make_bool(pl->ce_op.ce_int8   <= pr->ce_op.ce_int8);   break;
		case cel_type_uint8:	ret = cel_make_bool(pl->ce_op.ce_uint8  <= pr->ce_op.ce_uint8);  break;
		case cel_type_int16:	ret = cel_make_bool(pl->ce_op.ce_int16  <= pr->ce_op.ce_int16);  break;
		case cel_type_uint16:	ret = cel_make_bool(pl->ce_op.ce_uint16 <= pr->ce_op.ce_uint16); break;
		case cel_type_int32:	ret = cel_make_bool(pl->ce_op.ce_int32  <= pr->ce_op.ce_int32);  break;
		case cel_type_uint32:	ret = cel_make_bool(pl->ce_op.ce_uint32 <= pr->ce_op.ce_uint32); break;
		case cel_type_int64:	ret = cel_make_bool(pl->ce_op.ce_int64  <= pr->ce_op.ce_int64);  break;
		case cel_type_uint64:	ret = cel_make_bool(pl->ce_op.ce_uint64 <= pr->ce_op.ce_uint64); break;
		default:		break;
		}
		break;

	case cel_op_gt:
		switch (rtype->ct_tag) {
		case cel_type_int8:	ret = cel_make_bool(pl->ce_op.ce_int8   > pr->ce_op.ce_int8);   break;
		case cel_type_uint8:	ret = cel_make_bool(pl->ce_op.ce_uint8  > pr->ce_op.ce_uint8);  break;
		case cel_type_int16:	ret = cel_make_bool(pl->ce_op.ce_int16  > pr->ce_op.ce_int16);  break;
		case cel_type_uint16:	ret = cel_make_bool(pl->ce_op.ce_uint16 > pr->ce_op.ce_uint16); break;
		case cel_type_int32:	ret = cel_make_bool(pl->ce_op.ce_int32  > pr->ce_op.ce_int32);  break;
		case cel_type_uint32:	ret = cel_make_bool(pl->ce_op.ce_uint32 > pr->ce_op.ce_uint32); break;
		case cel_type_int64:	ret = cel_make_bool(pl->ce_op.ce_int64  > pr->ce_op.ce_int64);  break;
		case cel_type_uint64:	ret = cel_make_bool(pl->ce_op.ce_uint64 > pr->ce_op.ce_uint64); break;
		default:		break;
		}
		break;

	case cel_op_ge:
		switch (rtype->ct_tag) {
		case cel_type_int8:	ret = cel_make_bool(pl->ce_op.ce_int8   >= pr->ce_op.ce_int8);   break;
		case cel_type_uint8:	ret = cel_make_bool(pl->ce_op.ce_uint8  >= pr->ce_op.ce_uint8);  break;
		case cel_type_int16:	ret = cel_make_bool(pl->ce_op.ce_int16  >= pr->ce_op.ce_int16);  break;
		case cel_type_uint16:	ret = cel_make_bool(pl->ce_op.ce_uint16 >= pr->ce_op.ce_uint16); break;
		case cel_type_int32:	ret = cel_make_bool(pl->ce_op.ce_int32  >= pr->ce_op.ce_int32);  break;
		case cel_type_uint32:	ret = cel_make_bool(pl->ce_op.ce_uint32 >= pr->ce_op.ce_uint32); break;
		case cel_type_int64:	ret = cel_make_bool(pl->ce_op.ce_int64  >= pr->ce_op.ce_int64);  break;
		case cel_type_uint64:	ret = cel_make_bool(pl->ce_op.ce_uint64 >= pr->ce_op.ce_uint64); break;
		default:		break;
		}
		break;

	case cel_op_eq:
		switch (rtype->ct_tag) {
		case cel_type_int8:	ret = cel_make_bool(pl->ce_op.ce_int8   == pr->ce_op.ce_int8);   break;
		case cel_type_uint8:	ret = cel_make_bool(pl->ce_op.ce_uint8  == pr->ce_op.ce_uint8);  break;
		case cel_type_int16:	ret = cel_make_bool(pl->ce_op.ce_int16  == pr->ce_op.ce_int16);  break;
		case cel_type_uint16:	ret = cel_make_bool(pl->ce_op.ce_uint16 == pr->ce_op.ce_uint16); break;
		case cel_type_int32:	ret = cel_make_bool(pl->ce_op.ce_int32  == pr->ce_op.ce_int32);  break;
		case cel_type_uint32:	ret = cel_make_bool(pl->ce_op.ce_uint32 == pr->ce_op.ce_uint32); break;
		case cel_type_int64:	ret = cel_make_bool(pl->ce_op.ce_int64  == pr->ce_op.ce_int64);  break;
		case cel_type_uint64:	ret = cel_make_bool(pl->ce_op.ce_uint64 == pr->ce_op.ce_uint64); break;
		case cel_type_bool:	ret = cel_make_bool(pl->ce_op.ce_bool   == pr->ce_op.ce_bool);   break;
		default:		break;
		}
		break;

	case cel_op_neq:
		switch (rtype->ct_tag) {
		case cel_type_int8:	ret = cel_make_bool(pl->ce_op.ce_int8   != pr->ce_op.ce_int8);   break;
		case cel_type_uint8:	ret = cel_make_bool(pl->ce_op.ce_uint8  != pr->ce_op.ce_uint8);  break;
		case cel_type_int16:	ret = cel_make_bool(pl->ce_op.ce_int16  != pr->ce_op.ce_int16);  break;
		case cel_type_uint16:	ret = cel_make_bool(pl->ce_op.ce_uint16 != pr->ce_op.ce_uint16); break;
		case cel_type_int32:	ret = cel_make_bool(pl->ce_op.ce_int32  != pr->ce_op.ce_int32);  break;
		case cel_type_uint32:	ret = cel_make_bool(pl->ce_op.ce_uint32 != pr->ce_op.ce_uint32); break;
		case cel_type_int64:	ret = cel_make_bool(pl->ce_op.ce_int64  != pr->ce_op.ce_int64);  break;
		case cel_type_uint64:	ret = cel_make_bool(pl->ce_op.ce_uint64 != pr->ce_op.ce_uint64); break;
		case cel_type_bool:	ret = cel_make_bool(pl->ce_op.ce_bool   != pr->ce_op.ce_bool);   break;
		default:		break;
		}
		break;

	case cel_op_and:
		ret = cel_make_bool(pl->ce_op.ce_bool && pr->ce_op.ce_bool);
		break;

	case cel_op_or:
		ret = cel_make_bool(pl->ce_op.ce_bool || pr->ce_op.ce_bool);
		break;

	case cel_op_assign:
		break;
	}

	cel_expr_free(pl);
	cel_expr_free(pr);
	cel_type_free(rtype);
	return ret;
}

cel_expr_t *
cel_eval(e)
	cel_expr_t	*e;
{
	switch (e->ce_tag) {
	case cel_exp_int8:
	case cel_exp_uint8:
	case cel_exp_int16:
	case cel_exp_uint16:
	case cel_exp_int32:
	case cel_exp_uint32:
	case cel_exp_int64:
	case cel_exp_uint64:
	case cel_exp_string:
	case cel_exp_bool:
		return cel_expr_copy(e);

	case cel_exp_unary:
		return cel_eval_unary(e);
	
	case cel_exp_binary:
		return cel_eval_binary(e);

	case cel_exp_identifier:
	case cel_exp_function:
	case cel_exp_vardecl:
	case cel_exp_if:
		return NULL;
	}

	return NULL;
}

cel_expr_t *
cel_eval_list(l)
	cel_expr_list_t	*l;
{
cel_expr_t	*e = NULL;
cel_expr_t	*ret = NULL;

	CEL_TAILQ_FOREACH(e, l, ce_entry) {
	cel_expr_t	*r;

		if ((r = cel_eval(e)) == NULL) {
			cel_expr_free(ret);
			return NULL;
		}

		cel_expr_free(ret);
		ret = r;
	}

	return ret;
}

cel_expr_t *
cel_promote_expr(t, e)
	cel_type_t	*t;
	cel_expr_t	*e;
{
cel_expr_t	*ret;
	if ((ret = calloc(1, sizeof(*ret))) == NULL)
		return NULL;

	if (t->ct_tag == e->ce_type->ct_tag)
		return cel_expr_copy(e);

	switch (t->ct_tag) {
	case cel_type_int8:
		return NULL;

	case cel_type_uint8:
		switch (e->ce_type->ct_tag) {
		case cel_type_int8:	return cel_make_uint8((uint8_t) e->ce_op.ce_int8);
		default:		return NULL;
		}

	case cel_type_int16:
		switch (e->ce_type->ct_tag) {
		case cel_type_int8:	return cel_make_int16((int16_t) e->ce_op.ce_int8);
		case cel_type_uint8:	return cel_make_int16((int16_t) e->ce_op.ce_uint8);
		default:		return NULL;
		}

	case cel_type_uint16:
		switch (e->ce_type->ct_tag) {
		case cel_type_int8:	return cel_make_uint16((uint16_t) e->ce_op.ce_int8);
		case cel_type_uint8:	return cel_make_uint16((uint16_t) e->ce_op.ce_uint8);
		case cel_type_int16:	return cel_make_uint16((uint16_t) e->ce_op.ce_int16);
		default:		return NULL;
		}

	case cel_type_int32:
		switch (e->ce_type->ct_tag) {
		case cel_type_int8:	return cel_make_int32((int32_t) e->ce_op.ce_int8);
		case cel_type_uint8:	return cel_make_int32((int32_t) e->ce_op.ce_uint8);
		case cel_type_int16:	return cel_make_int32((int32_t) e->ce_op.ce_int16);
		case cel_type_uint16:	return cel_make_int32((int32_t) e->ce_op.ce_uint16);
		default:		return NULL;
		}

	case cel_type_uint32:
		switch (e->ce_type->ct_tag) {
		case cel_type_int8:	return cel_make_uint32((uint32_t) e->ce_op.ce_int8);
		case cel_type_uint8:	return cel_make_uint32((uint32_t) e->ce_op.ce_uint8);
		case cel_type_int16:	return cel_make_uint32((uint32_t) e->ce_op.ce_int16);
		case cel_type_uint16:	return cel_make_uint32((uint32_t) e->ce_op.ce_uint16);
		case cel_type_int32:	return cel_make_uint32((uint32_t) e->ce_op.ce_int32);
		default:		return NULL;
		}

	case cel_type_int64:
		switch (e->ce_type->ct_tag) {
		case cel_type_int8:	return cel_make_int64((int64_t) e->ce_op.ce_int8);
		case cel_type_uint8:	return cel_make_int64((int64_t) e->ce_op.ce_uint8);
		case cel_type_int16:	return cel_make_int64((int64_t) e->ce_op.ce_int16);
		case cel_type_uint16:	return cel_make_int64((int64_t) e->ce_op.ce_uint16);
		case cel_type_int32:	return cel_make_int64((int64_t) e->ce_op.ce_int32);
		case cel_type_uint32:	return cel_make_int64((int64_t) e->ce_op.ce_uint32);
		default:		return NULL;
		}

	case cel_type_uint64:
		switch (e->ce_type->ct_tag) {
		case cel_type_int8:	return cel_make_uint64((uint64_t) e->ce_op.ce_int8);
		case cel_type_uint8:	return cel_make_uint64((uint64_t) e->ce_op.ce_uint8);
		case cel_type_int16:	return cel_make_uint64((uint64_t) e->ce_op.ce_int16);
		case cel_type_uint16:	return cel_make_uint64((uint64_t) e->ce_op.ce_uint16);
		case cel_type_int32:	return cel_make_uint64((uint64_t) e->ce_op.ce_int32);
		case cel_type_int64:	return cel_make_uint64((uint64_t) e->ce_op.ce_int64);
		case cel_type_uint64:	return cel_make_uint64((uint64_t) e->ce_op.ce_uint64);
		default:		return NULL;
		}

	case cel_type_bool:
	case cel_type_array:
	case cel_type_function:
	case cel_type_string:
		return NULL;
	}

	return NULL;
}
