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

#include	"celcore/eval.h"
#include	"celcore/type.h"
#include	"celcore/function.h"
#include	"celcore/scope.h"

static cel_expr_t *
cel_eval_if(s, e)
	cel_scope_t	*s;
	cel_expr_t	*e;
{
cel_expr_t	*stmt;
cel_expr_t	*ret;
cel_if_branch_t	*if_;

	CEL_TAILQ_FOREACH(if_, e->ce_op.ce_if, ib_entry) {
	cel_expr_t	*e;
		e = cel_eval(s, if_->ib_condition);
		if (!e->ce_op.ce_bool) {
			cel_expr_free(e);
			continue;
		}

		cel_expr_free(e);

		CEL_TAILQ_FOREACH(stmt, &if_->ib_exprs, ce_entry) {
		cel_expr_t	*v, *w;
			if ((v = cel_eval(s, stmt)) == NULL)
				return NULL;

			cel_expr_free(v);
#if 0
			if (v->ce_tag != cel_exp_return)
				continue;

			w = cel_eval(v->ce_op.ce_unary.operand);
			cel_expr_free(v);
			v = cel_expr_convert(w, e->ce_op.ce_function->cf_return_type);
			cel_expr_free(w);
			return v;
#endif
		}
		break;
	}

	return cel_make_void();
}

static cel_expr_t *
cel_call_function(s, e)
	cel_scope_t	*s;
	cel_expr_t	*e;
{
cel_expr_t	*stmt;
cel_expr_t	*ret;
cel_expr_t	*fu;
cel_scope_t	*sc;

	if ((fu = cel_eval(s, e)) == NULL)
		return NULL;

	sc = cel_scope_copy(fu->ce_op.ce_function->cf_scope);

	CEL_TAILQ_FOREACH(stmt, &fu->ce_op.ce_function->cf_body, ce_entry) {
	cel_expr_t	*v, *w;
		if ((v = cel_eval(sc, stmt)) == NULL) {
			cel_expr_free(fu);
			cel_scope_free(sc);
			return NULL;
		}

		if (v->ce_tag != cel_exp_return)
			continue;

		w = cel_eval(sc, v->ce_op.ce_unary.operand);
		cel_expr_free(v);
		v = cel_expr_convert(w, fu->ce_op.ce_function->cf_return_type);
		cel_expr_free(w);
		cel_expr_free(fu);
		cel_scope_free(sc);
		return v;
	}

	cel_expr_free(fu);
	cel_scope_free(sc);
	return cel_make_void();
}

static cel_expr_t *
cel_eval_unary(s, e)
	cel_scope_t	*s;
	cel_expr_t	*e;
{
cel_expr_t	*v, *pv, *ret = NULL;
cel_type_t	*ptype;

	if ((v = cel_eval(s, e->ce_op.ce_unary.operand)) == NULL)
		return NULL;

	if (e->ce_tag == cel_exp_cast) {
	cel_expr_t	*ret;
		ret = cel_expr_convert(v, e->ce_type);
		cel_expr_free(v);
		return ret;
	}

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
cel_eval_assign(s, l, r)
	cel_scope_t	*s;
	cel_expr_t	*l, *r;
{
cel_expr_t	*er, *cr, *el;

	if ((el = cel_eval(s, l)) == NULL)
		return NULL;

	if ((er = cel_eval(s, r)) == NULL) {
		cel_expr_free(el);
		return NULL;
	}

	cr = cel_expr_convert(er, el->ce_type);
	cel_expr_assign(el, cr);
	cel_expr_free(cr);
	return cel_expr_copy(el);
}

static cel_expr_t *
cel_eval_binary(s, e)
	cel_scope_t	*s;
	cel_expr_t	*e;
{
cel_expr_t	*l, *r, *pl, *pr, *ret = NULL;
cel_type_t	*rtype;
char		*str;

	/* This one is not like the others */
	if (e->ce_op.ce_binary.oper == cel_op_assign) {
		ret = cel_eval_assign(s, e->ce_op.ce_binary.left,
				         e->ce_op.ce_binary.right);
		return ret;
	}

	if ((l = cel_eval(s, e->ce_op.ce_binary.left)) == NULL)
		return NULL;

	if ((r = cel_eval(s, e->ce_op.ce_binary.right)) == NULL) {
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
			str = malloc(sizeof(char) * (strlen(pl->ce_op.ce_string) +
						     strlen(pr->ce_op.ce_string) + 1));
			strcpy(str, pl->ce_op.ce_string);
			strcat(str, pr->ce_op.ce_string);
			ret = cel_make_string(str);
			free(str);
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
		/*NOTREACHED*/
		break;
	}

	cel_expr_free(pl);
	cel_expr_free(pr);
	cel_type_free(rtype);
	return ret;
}

cel_expr_t *
cel_eval(s, e)
	cel_scope_t	*s;
	cel_expr_t	*e;
{
cel_scope_item_t	*sc;

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
	case cel_exp_void:
	case cel_exp_return:
	case cel_exp_function:
		return cel_expr_copy(e);

	case cel_exp_unary:
	case cel_exp_cast:
		return cel_eval_unary(s, e);
	
	case cel_exp_binary:
		return cel_eval_binary(s, e);

	case cel_exp_call:
		return cel_call_function(s, e->ce_op.ce_unary.operand);

	case cel_exp_if:
		return cel_eval_if(s, e);

	case cel_exp_variable:
		if ((sc = cel_scope_find_item(s, e->ce_op.ce_variable)) == NULL)
			return NULL;
		return sc->si_ob.si_expr;

	case cel_exp_vardecl:
		return NULL;
	}

	return NULL;
}

cel_expr_t *
cel_eval_list(s, l)
	cel_scope_t	*s;
	cel_expr_list_t	*l;
{
cel_expr_t	*e = NULL;
cel_expr_t	*ret = NULL;

	CEL_TAILQ_FOREACH(e, l, ce_entry) {
	cel_expr_t	*r;

		if ((r = cel_eval(s, e)) == NULL) {
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
	case cel_type_void:
		return NULL;
	}

	return NULL;
}
