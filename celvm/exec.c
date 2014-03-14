/*
 * CEL: The Compact Embeddable Language.
 * Copyright (c) 2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#include	"celcore/expr.h"

#include	"celvm/vm.h"
#include	"celvm/instr.h"

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
			}
			ip++;
			break;

		case CEL_I_NOT:
			stack[sp - 1].u32 = !stack[sp - 1].u32;
			break;

#define	CMP_(ty, sz, var, op)					\
		case ty:					\
			GET_SI##sz(b.var);			\
			GET_SI##sz(a.var);			\
			PUT_SU32(a.var op b.var);		\
			break;

#define	CMP(in, op)						\
		case in:					\
		switch (*ip) {					\
			CMP_(CEL_VA_UINT8, 8, u8, op)		\
			CMP_(CEL_VA_INT8, 8, i8, op)		\
			CMP_(CEL_VA_UINT16, 16, u16, op)	\
			CMP_(CEL_VA_INT16, 16, i16, op)		\
			CMP_(CEL_VA_UINT32, 32, u32, op)	\
			CMP_(CEL_VA_INT32, 32, i32, op)		\
			CMP_(CEL_VA_UINT64, 64, u64, op)	\
			CMP_(CEL_VA_INT64, 64, i64, op)		\
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
			}

			vars[b.i16].u64 = a.u64;
			ip++;
			break;

		default:
			printf("can't decode %d\n", inst);
			return -1;
		}
	}
}
