/*
 * CEL: The Compact Embeddable Language.
 * Copyright (c) 2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#include	<stdio.h>

#include	"celcore/cel-config.h"

#ifdef	CEL_HAVE_FFI
# ifdef CEL_HAVE_FFI_FFI_H
#  include	<ffi/ffi.h>
# else
#  include	<ffi.h>
# endif
#endif

/* ffi pollutes the namespace with these */
#undef		PACKAGE_NAME
#undef		PACKAGE_STRING
#undef		PACKAGE_TARNAME
#undef		PACKAGE_VERSION
#undef		PACKAGE_BUGREPORT

#include	"celcore/function.h"
#include	"celcore/expr.h"

#include	"celvm/vm.h"
#include	"celvm/instr.h"

#include	"build.h"

#define	STACKSZ 16

#define	GET_UINT16(p)	((uint16_t) (p)[0] <<  8 |	\
			 (uint16_t) (p)[1])

#define	GET_UINT32(p)	((uint32_t) (p)[0] << 24 |	\
			 (uint32_t) (p)[1] << 16 |	\
			 (uint32_t) (p)[2] <<  8 |	\
			 (uint32_t) (p)[3])

#define	GET_UINT64(p)	((uint64_t) (p)[0] << 56 |	\
			 (uint64_t) (p)[1] << 48 |	\
			 (uint64_t) (p)[2] << 40 |	\
			 (uint64_t) (p)[3] << 32 |	\
			 (uint64_t) (p)[4] << 24 |	\
			 (uint64_t) (p)[5] << 16 |	\
			 (uint64_t) (p)[6] <<  8 |	\
			 (uint64_t) (p)[7])
#define	GET_SFLOAT(p)	(*(float *)p)
#define	GET_DFLOAT(p)	(*(double *)p)
#define	GET_QFLOAT(p)	(*(long double *)p)
#if SIZEOF_VOIDP == 8
# define GET_PTR(p)	GET_UINT64(p)
#else
# define GET_PTR(p)	GET_UINT32(p)
#endif

#define	GET_IS8(v)	do { (v) = *ip; ip++; } while (0)
#define	GET_IU8(v)	do { (v) = *ip; ip++; } while (0)
#define	GET_II16(v)	do { (v) = GET_UINT16(ip); ip += 2; } while (0)
#define	GET_IU16(v)	do { (v) = GET_UINT16(ip); ip += 2; } while (0)
#define	GET_IU32(v)	do { (v) = GET_UINT32(ip); ip += 4; } while (0)
#define	GET_IU64(v)	do { (v) = GET_UINT64(ip); ip += 8; } while (0)
#define	GET_SU8(v)	((v) = stack[--sp].u8)
#define	GET_SI8(v)	((v) = stack[--sp].i8)
#define	GET_SU16(v)	((v) = stack[--sp].u16)
#define	GET_SI16(v)	GET_SU16(v)
#define	GET_SU32(v)	((v) = stack[--sp].u32)
#define	GET_SI32(v)	GET_SU32(v)
#define	GET_SU64(v)	do { sp -= 2; (v) = (((uint64_t) stack[sp].u32) << 32) | stack[sp + 1].u32; } while (0)
#define	PUT_SU8(v)	(stack[sp++].u8 = (v))
#define PUT_SU16(v)	(stack[sp++].u16 = (v))
#define PUT_SU32(v)	(stack[sp++].u32 = (v))
#define PUT_SU64(v)	do { PUT_SU32((v) >> 32); PUT_SU32((v) & 0xFFFFFFFF); } while (0)
#define	PUT_SI8(v)	PUT_SU8(v)
#define	PUT_SI16(v)	PUT_SU16(v)
#define	PUT_SI32(v)	PUT_SU32(v)
#define	PUT_SI64(v)	PUT_SU64(v)
#define	GET_SI64(v)	GET_SU64(v)
#define	PUT_SP(v)	(stack[sp++].ptr = (v))
#define	GET_IP(v)	do { (v) = GET_PTR(ip); ip += SIZEOF_VOIDP; } while (0)
#define	GET_SP(v)	((v) = stack[--sp].ptr)
#define	GET_SSF(v)	((v) = stack[--sp].sflt)
#define	GET_SDF(v)	((v) = stack[--sp].dflt)
#define	GET_SQF(v)	((v) = stack[--sp].qflt)
#define	PUT_SSF(v)	(stack[sp++].sflt = (v))
#define	PUT_SDF(v)	(stack[sp++].dflt = (v))
#define	PUT_SQF(v)	(stack[sp++].qflt = (v))
#define	GET_ISF(v)	do { (v) = GET_SFLOAT(ip); ip += sizeof(float); } while (0)
#define	GET_IDF(v)	do { (v) = GET_DFLOAT(ip); ip += sizeof(double); } while (0)
#define	GET_IQF(v)	do { (v) = GET_QFLOAT(ip); ip += sizeof(long double); } while (0)

int
cel_vm_func_execute(s, f, ret)
	cel_scope_t	*s;
	cel_vm_func_t	*f;
	cel_vm_any_t	*ret;
{
cel_vm_any_t	 stack[STACKSZ];
cel_vm_any_t	*vars;
int		 sp;
uint8_t	const	*ip, *oip;
cel_function_t	*func;

	vars = calloc(f->vf_nvars, sizeof(cel_vm_any_t));

	ip = f->vf_bytecode;
	sp = 0;

	for (;;) {
	uint8_t		inst;
	cel_vm_any_t	a, b;

		if (ip > (f->vf_bytecode + f->vf_bytecodesz)) {
			printf("ip overflow\n");
			return -1;
		}

		oip = ip;
		inst = *ip++;

		switch (inst) {
		case CEL_I_RET:
			if (*ip != CEL_VA_VOID && (sp == 0))
				return -1;

			switch (*ip) {
			case CEL_VA_VOID:	break;
			case CEL_VA_INT8:
			case CEL_VA_UINT8:	GET_SU8(ret->u8); break;
			case CEL_VA_INT16:
			case CEL_VA_UINT16:	GET_SU16(ret->u16); break;
			case CEL_VA_INT32:
			case CEL_VA_UINT32:	GET_SU32(ret->u32); break;
			case CEL_VA_INT64:
			case CEL_VA_UINT64:	GET_SU64(ret->u64); break;
			case CEL_VA_PTR:	GET_SP(ret->ptr); break;
			case CEL_VA_SFLOAT:	GET_SSF(ret->sflt); break;
			case CEL_VA_DFLOAT:	GET_SDF(ret->dflt); break;
			case CEL_VA_QFLOAT:	GET_SQF(ret->qflt); break;
			default:		return -1;
			}

			return 0;

		case CEL_I_LOADI:
			switch (*ip++) {
			case CEL_VA_INT8:
			case CEL_VA_UINT8:	GET_IU8(a.u8); PUT_SU8(a.u8); break;
			case CEL_VA_INT16:
			case CEL_VA_UINT16:	GET_IU16(a.u16); PUT_SU16(a.u16); break;
			case CEL_VA_INT32:
			case CEL_VA_UINT32:	GET_IU32(a.u32); PUT_SU32(a.u32); break;
			case CEL_VA_INT64:
			case CEL_VA_UINT64:	GET_IU64(a.u64); PUT_SU64(a.u64); break;
			case CEL_VA_PTR:	GET_IP(a.ptr); PUT_SP(a.u64); break;
			case CEL_VA_SFLOAT:	GET_ISF(a.sflt); PUT_SSF(a.sflt); break;
			case CEL_VA_DFLOAT:	GET_IDF(a.dflt); PUT_SDF(a.dflt); break;
			case CEL_VA_QFLOAT:	GET_IQF(a.qflt); PUT_SQF(a.qflt); break;
			}
			break;

#define	MOP(op)							\
		switch (*ip) {					\
		case CEL_VA_UINT8:				\
		case CEL_VA_INT8:				\
			GET_SU8(a.u8);				\
			stack[sp - 1].u8 op a.u8;		\
			break;					\
		case CEL_VA_UINT16:				\
		case CEL_VA_INT16:				\
			GET_SU16(a.u16);			\
			stack[sp - 1].u16 op a.u16;		\
			break;					\
		case CEL_VA_UINT32:				\
		case CEL_VA_INT32:				\
			GET_SU32(a.u32);			\
			stack[sp - 1].u32 op a.u32;		\
			break;					\
		case CEL_VA_UINT64:				\
		case CEL_VA_INT64:				\
			GET_SU64(a.u64);			\
			stack[sp - 1].u64 op a.u64;		\
			break;					\
		case CEL_VA_SFLOAT:				\
			GET_SSF(a.sflt);			\
			stack[sp - 1].sflt op a.sflt;		\
			break;					\
		case CEL_VA_DFLOAT:				\
			GET_SDF(a.dflt);			\
			stack[sp - 1].dflt op a.dflt;		\
			break;					\
		case CEL_VA_QFLOAT:				\
			GET_SQF(a.qflt);			\
			stack[sp - 1].qflt op a.qflt;		\
			break;					\
		}						\
		ip++;

		case CEL_I_ADD:
			MOP(+=);
			break;

		case CEL_I_SUB:
			MOP(-=);
			break;

		case CEL_I_MUL:
			MOP(*=);
			break;

		case CEL_I_DIV:
			MOP(/=);
			break;

		case CEL_I_NEG:
			switch (*ip) {
			case CEL_VA_INT8:
			case CEL_VA_UINT8:
				stack[sp - 1].i8 = -stack[sp -1].i8;
				break;
			case CEL_VA_INT16:
			case CEL_VA_UINT16:
				stack[sp - 1].i16 = -stack[sp -1].i16;
				break;
			case CEL_VA_INT32:
			case CEL_VA_UINT32:
				stack[sp - 1].i32 = -stack[sp -1].i32;
				break;
			case CEL_VA_INT64:
			case CEL_VA_UINT64:
				stack[sp - 1].i64 = -stack[sp -1].i64;
				break;
			case CEL_VA_SFLOAT:
				stack[sp - 1].sflt = -stack[sp - 1].sflt;
				break;
			case CEL_VA_DFLOAT:
				stack[sp - 1].dflt = -stack[sp - 1].dflt;
				break;
			case CEL_VA_QFLOAT:
				stack[sp - 1].qflt = -stack[sp - 1].qflt;
				break;
			}
			ip++;
			break;

		case CEL_I_NOT:
			stack[sp - 1].u32 = !stack[sp - 1].u32;
			break;

#define	CMP_(ty, sz, var, op)					\
		case ty:					\
			GET_S##sz(b.var);			\
			GET_S##sz(a.var);			\
			PUT_SU32(a.var op b.var);		\
			break;

#define	CMP(in, op)						\
		case in:					\
		switch (*ip) {					\
			CMP_(CEL_VA_UINT8, U8, u8, op)		\
			CMP_(CEL_VA_INT8, I8, i8, op)		\
			CMP_(CEL_VA_UINT16, U16, u16, op)	\
			CMP_(CEL_VA_INT16, I16, i16, op)	\
			CMP_(CEL_VA_UINT32, U32, u32, op)	\
			CMP_(CEL_VA_INT32, I32, i32, op)	\
			CMP_(CEL_VA_UINT64, U64, u64, op)	\
			CMP_(CEL_VA_INT64, I64, i64, op)	\
			CMP_(CEL_VA_SFLOAT, SF, sflt, op)	\
			CMP_(CEL_VA_DFLOAT, DF, dflt, op)	\
			CMP_(CEL_VA_QFLOAT, QF, qflt, op)	\
		}						\
		ip++;						\
		break;

		CMP(CEL_I_TEQ, ==)
		CMP(CEL_I_TNE, !=)
		CMP(CEL_I_TLE, <=)
		CMP(CEL_I_TLT, <)
		CMP(CEL_I_TGT, >)
		CMP(CEL_I_TGE, >=)

		case CEL_I_BR:
			GET_II16(a.i16);
			ip = oip + a.i16;
			break;

		case CEL_I_BRT:
			GET_II16(b.i16);
			GET_SU32(a.u32);
			if (a.u32)
				ip = oip + b.i16;
			break;

		case CEL_I_BRF:
			GET_II16(b.i16);
			GET_SU32(a.u32);
			if (!a.u32)
				ip = oip + b.i16;
			break;

#define	INC_(op, ty, var, VAR)				\
		case ty:				\
			GET_S##VAR(a.var);		\
			vars[b.i16].var op a.var;	\
			break;
#define	INC(in, op)						\
		case in:					\
		GET_II16(b.i16);				\
		switch (*ip) {					\
			INC_(op, CEL_VA_INT8, i8, I8)		\
			INC_(op, CEL_VA_UINT8, u8, U8)		\
			INC_(op, CEL_VA_INT16, i16, I16)	\
			INC_(op, CEL_VA_UINT16, u16, U16)	\
			INC_(op, CEL_VA_INT32, i32, I32)	\
			INC_(op, CEL_VA_UINT32, u32, U32)	\
			INC_(op, CEL_VA_INT64, i64, I64)	\
			INC_(op, CEL_VA_UINT64, u64, U64)	\
			INC_(op, CEL_VA_SFLOAT, sflt, SF)	\
			INC_(op, CEL_VA_DFLOAT, dflt, DF)	\
			INC_(op, CEL_VA_QFLOAT, qflt, QF)	\
		}						\
		ip++;						\
		break;

		INC(CEL_I_INCV, +=)
		INC(CEL_I_DECV, -=)
		INC(CEL_I_MULV, *=)
		INC(CEL_I_DIVV, /=)

		case CEL_I_LOADV:
			GET_II16(b.i16);
			switch (*ip) {
			case CEL_VA_INT8:	PUT_SI8(vars[b.i16].i8); break;
			case CEL_VA_UINT8:	PUT_SU8(vars[b.i16].u8); break;
			case CEL_VA_INT16:	PUT_SI16(vars[b.i16].i16); break;
			case CEL_VA_UINT16:	PUT_SU16(vars[b.i16].u16); break;
			case CEL_VA_INT32:	PUT_SI32(vars[b.i16].i32); break;
			case CEL_VA_UINT32:	PUT_SU32(vars[b.i16].u32); break;
			case CEL_VA_INT64:	PUT_SI64(vars[b.i16].i64); break;
			case CEL_VA_UINT64:	PUT_SU64(vars[b.i16].u64); break;
			case CEL_VA_SFLOAT:	PUT_SSF(vars[b.i16].sflt); break;
			case CEL_VA_DFLOAT:	PUT_SDF(vars[b.i16].dflt); break;
			case CEL_VA_QFLOAT:	PUT_SQF(vars[b.i16].qflt); break;
			}
			ip++;
			break;

		case CEL_I_STOV:
			GET_II16(b.i16);
			switch (*ip) {
			case CEL_VA_INT8:	GET_SI8(a.i8); break;
			case CEL_VA_UINT8:	GET_SU8(a.u8); break;
			case CEL_VA_INT16:	GET_SI16(a.i16); break;
			case CEL_VA_UINT16:	GET_SU16(a.u16); break;
			case CEL_VA_INT32:	GET_SI32(a.i32); break;
			case CEL_VA_UINT32:	GET_SU32(a.u32); break;
			case CEL_VA_INT64:	GET_SI64(a.i64); break;
			case CEL_VA_UINT64:	GET_SU64(a.u64); break;
			case CEL_VA_SFLOAT:	GET_SSF(a.sflt); break;
			case CEL_VA_DFLOAT:	GET_SDF(a.dflt); break;
			case CEL_VA_QFLOAT:	GET_SQF(a.qflt); break;
			}

			vars[b.i16].u64 = a.u64;
			ip++;
			break;

		case CEL_I_CALL:
			GET_SP(a.ptr);
			func = (cel_function_t *) a.ptr;

			if (func->cf_extern) {
			void		**args;
			cel_vm_any_t	*aargs;
			cel_type_t	*ty;
			int		 i = 0;
			ffi_arg		 ret;

				args = malloc(sizeof(void *) * func->cf_nargs);
				aargs = malloc(sizeof(cel_vm_any_t) * func->cf_nargs);

				CEL_TAILQ_FOREACH(ty, func->cf_type->ct_type.ct_function.ct_args, ct_entry) {
					switch (ty->ct_tag) {
					case cel_type_int8:	GET_SI8(aargs[i].i8); args[i] = &aargs[i]; break;
					case cel_type_uint8:	GET_SU8(aargs[i].u8); args[i] = &aargs[i]; break;
					case cel_type_int16:	GET_SI16(aargs[i].i16); args[i] = &aargs[i]; break;
					case cel_type_uint16:	GET_SU16(aargs[i].u16); args[i] = &aargs[i]; break;
					case cel_type_int32:	GET_SI32(aargs[i].i32); args[i] = &aargs[i]; break;
					case cel_type_uint32:	GET_SU32(aargs[i].u32); args[i] = &aargs[i]; break;
					case cel_type_int64:	GET_SI64(aargs[i].i64); args[i] = &aargs[i]; break;
					case cel_type_uint64: 	GET_SU64(aargs[i].u64); args[i] = &aargs[i]; break;
					case cel_type_ptr:	GET_SP(aargs[i].ptr); args[i] = &aargs[i].ptr; break;
					case cel_type_sfloat:	GET_SSF(aargs[i].sflt); args[i] = &aargs[i].sflt; break;
					case cel_type_dfloat:	GET_SDF(aargs[i].dflt); args[i] = &aargs[i].dflt; break;
					case cel_type_qfloat:	GET_SQF(aargs[i].qflt); args[i] = &aargs[i].qflt; break;
					default:
						printf("bad arg type in call");
						return -1;
					}
					i++;
				}

				ffi_call(func->cf_ffi, func->cf_ptr, &ret, args);

				switch (func->cf_type->ct_type.ct_function.ct_return_type->ct_tag) {
				case cel_type_int8:	PUT_SI8((int8_t)ret); break;
				case cel_type_uint8:	PUT_SU8((uint8_t)ret); break;
				case cel_type_int16:	PUT_SI16((int16_t)ret); break;
				case cel_type_uint16:	PUT_SU16((uint16_t)ret); break;
				case cel_type_int32:	PUT_SI32((int32_t)ret); break;
				case cel_type_uint32:	PUT_SU32((uint32_t)ret); break;
				case cel_type_int64:	PUT_SI64((int64_t)ret); break;
				case cel_type_uint64:	PUT_SU64((uint64_t)ret); break;
				case cel_type_ptr:	PUT_SP((uintptr_t)ret); break;
				case cel_type_sfloat:	PUT_SSF(*(float *)&ret); break;
				case cel_type_dfloat:	PUT_SDF(*(double *)&ret); break;
				case cel_type_qfloat:	PUT_SQF(*(long double *)&ret); break;
				}

				free(args);
				free(aargs);
			} else {
			}
			break;

		default:
			printf("can't decode %d\n", inst);
			return -1;
		}
	}
}
