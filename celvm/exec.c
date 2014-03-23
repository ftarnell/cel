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
#include	<string.h>

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

#define	STACKSZ 128

#define	R_R0	0	/* IP */
#define	R_R1	1	/* SP */
#define	R_R2	2	/* VP */
#define	R_R3	3	/* return value */
#define	R_R4	4	/* var base */
#define	NREGS	64

#define	R_IP	R_R0
#define	R_SP	R_R1
#define	R_VP	R_R2

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
# define GET_PTR(p)	((uint8_t *) GET_UINT64(p))
#else
# define GET_PTR(p)	((uint8_t *) GET_UINT32(p))
#endif

#define	GET_IS8(v)	do { (v) = *(uint8_t *)regs[R_IP]; regs[R_IP]++; } while (0)
#define	GET_IU8(v)	do { (v) = *(uint8_t *)regs[R_IP]; regs[R_IP]++; } while (0)
#define	GET_II16(v)	do { (v) = GET_UINT16((uint8_t *)regs[R_IP]); regs[R_IP] += 2; } while (0)
#define	GET_IU16(v)	do { (v) = GET_UINT16((uint8_t *)regs[R_IP]); regs[R_IP] += 2; } while (0)
#define	GET_IU32(v)	do { (v) = GET_UINT32((uint8_t *)regs[R_IP]); regs[R_IP] += 4; } while (0)
#define	GET_IU64(v)	do { (v) = GET_UINT64((uint8_t *)regs[R_IP]); regs[R_IP] += 8; } while (0)
#define	GET_IP(v)	GET_IU64(v)

#define	GET_SU8(v)	do { regs[R_SP] -= sizeof(cel_vm_any_t); ((v) = (uint8_t) (*(cel_vm_reg_t *) regs[R_SP])); } while (0)
#define	GET_SI8(v)	do { regs[R_SP] -= sizeof(cel_vm_any_t); ((v) = (int8_t) (*(cel_vm_reg_t *) regs[R_SP])); } while (0)
#define	GET_SU16(v)	do { regs[R_SP] -= sizeof(cel_vm_any_t); ((v) = (uint16_t) (*(cel_vm_reg_t *) regs[R_SP])); } while (0)
#define	GET_SI16(v)	do { regs[R_SP] -= sizeof(cel_vm_any_t); ((v) = (int16_t) (*(cel_vm_reg_t *) regs[R_SP])); } while (0)
#define	GET_SU32(v)	do { regs[R_SP] -= sizeof(cel_vm_any_t); ((v) = (uint32_t) (*(cel_vm_reg_t *) regs[R_SP])); } while (0)
#define	GET_SI32(v)	do { regs[R_SP] -= sizeof(cel_vm_any_t); ((v) = (int32_t) (*(cel_vm_reg_t *) regs[R_SP])); } while (0)
#define	GET_SU64(v)	do { regs[R_SP] -= sizeof(cel_vm_any_t); ((v) = (uint64_t) (*(cel_vm_reg_t *) regs[R_SP])); } while (0)
#define	GET_SI64(v)	do { regs[R_SP] -= sizeof(cel_vm_any_t); ((v) = (int64_t) (*(cel_vm_reg_t *) regs[R_SP])); } while (0)
#define	GET_SP(v)	do { regs[R_SP] -= sizeof(cel_vm_any_t); ((v) = (uint64_t) (*(cel_vm_reg_t *) regs[R_SP])); } while (0)
#if 0
#define	GET_SSF(v)	do { regs[R_SP].ptr -= sizeof(cel_vm_any_t); (v) = ((cel_vm_any_t *) regs[R_SP].ptr)->sflt; } while (0)
#define	GET_SDF(v)	do { regs[R_SP].ptr -= sizeof(cel_vm_any_t); (v) = ((cel_vm_any_t *) regs[R_SP].ptr)->dflt; } while (0)
#define	GET_SQF(v)	do { regs[R_SP].ptr -= sizeof(cel_vm_any_t); (v) = ((cel_vm_any_t *) regs[R_SP].ptr)->qflt; } while (0)
#define	PUT_SSF(v)	do { (((cel_vm_any_t *) regs[R_SP].ptr)->sflt = (v)); regs[R_SP].ptr += sizeof(cel_vm_any_t); } while (0)
#define	PUT_SDF(v)	do { (((cel_vm_any_t *) regs[R_SP].ptr)->dflt = (v)); regs[R_SP].ptr += sizeof(cel_vm_any_t); } while (0)
#define	PUT_SQF(v)	do { (((cel_vm_any_t *) regs[R_SP].ptr)->qflt = (v)); regs[R_SP].ptr += sizeof(cel_vm_any_t); } while (0)
#endif

#define	PUT_SU8(v)	do { ((*(uint64_t *)regs[R_SP]) = (uint8_t) (v)); regs[R_SP] += sizeof(cel_vm_any_t); } while (0)
#define	PUT_SI8(v)	do { ((*(int64_t *)regs[R_SP]) = (int8_t) (v)); regs[R_SP] += sizeof(cel_vm_any_t); } while (0)
#define	PUT_SU16(v)	do { ((*(uint64_t *)regs[R_SP]) = (uint16_t) (v)); regs[R_SP] += sizeof(cel_vm_any_t); } while (0)
#define	PUT_SI16(v)	do { ((*(int64_t *)regs[R_SP]) = (int16_t) (v)); regs[R_SP] += sizeof(cel_vm_any_t); } while (0)
#define	PUT_SU32(v)	do { ((*(uint64_t *)regs[R_SP]) = (uint32_t) (v)); regs[R_SP] += sizeof(cel_vm_any_t); } while (0)
#define	PUT_SI32(v)	do { ((*(int64_t *)regs[R_SP]) = (int32_t) (v)); regs[R_SP] += sizeof(cel_vm_any_t); } while (0)
#define	PUT_SU64(v)	do { ((*(uint64_t *)regs[R_SP]) = (uint64_t) (v)); regs[R_SP] += sizeof(cel_vm_any_t); } while (0)
#define	PUT_SI64(v)	do { ((*(int64_t *)regs[R_SP]) = (int64_t) (v)); regs[R_SP] += sizeof(cel_vm_any_t); } while (0)
#define	PUT_SP(v)	do { ((*(uint64_t *)regs[R_SP]) = (uint64_t) (v)); regs[R_SP] += sizeof(cel_vm_any_t); } while (0)

#if 0
#define	GET_ISF(v)	do { (v) = GET_SFLOAT(regs[R_IP].ptr); regs[R_IP].ptr += sizeof(float); } while (0)
#define	GET_IDF(v)	do { (v) = GET_DFLOAT(regs[R_IP].ptr); regs[R_IP].ptr += sizeof(double); } while (0)
#define	GET_IQF(v)	do { (v) = GET_QFLOAT(regs[R_IP].ptr); regs[R_IP].ptr += sizeof(long double); } while (0)
#endif

typedef cel_vm_reg_t vm_regs_t[NREGS];

static int cel_vm_bytecode_exec(vm_regs_t regs);

int
cel_vm_func_execute(s, f, ret)
	cel_scope_t	*s;
	cel_vm_func_t	*f;
	cel_vm_reg_t	*ret;
{
vm_regs_t	regs;

	regs[R_SP] = (cel_vm_reg_t) calloc(STACKSZ, sizeof(cel_vm_any_t));
	/* XXX - fudge two words up the stack for main()'s arguments */
	regs[R_SP] += 16;
	regs[R_IP] = (cel_vm_reg_t) f->vf_bytecode;
	regs[5] = 0;

	if (cel_vm_bytecode_exec(regs) == -1)
		return -1;

	if (ret)
		*ret = regs[R_R3];

	return 0;
}

int
cel_vm_bytecode_exec(regs)
	vm_regs_t	regs;
{
cel_vm_reg_t	 oip;
cel_function_t	*func;

	regs[R_VP] = 0;

	for (;;) {
	uint8_t		inst;
	cel_vm_reg_t	a, b;

#if 0
		if (ip > (f->vf_bytecode + f->vf_bytecodesz)) {
			printf("ip overflow\n");
			return -1;
		}
#endif

		oip = regs[R_IP];
		inst = *(uint8_t *)(regs[R_IP]++);

		switch (inst) {
		case CEL_I_CALL:
			GET_SU64(a);

			/* Push return address */
			PUT_SU64(regs[R_IP]);
			regs[R_IP] = a;
			break;

		case CEL_I_RET:
			regs[R_SP] = regs[6];
			/* Retrieve return address from %r5 */
			regs[R_IP] = regs[5];

			if (regs[R_IP] == 0)
				return 0;

			break;

		case CEL_I_LOADI:
			switch (*((uint8_t *)(regs[R_IP]++))) {
			case CEL_VA_INT8:
			case CEL_VA_UINT8:	GET_IU8(a); PUT_SU64(a); break;
			case CEL_VA_INT16:
			case CEL_VA_UINT16:	GET_IU16(a); PUT_SU64(a); break;
			case CEL_VA_INT32:
			case CEL_VA_UINT32:	GET_IU32(a); PUT_SU64(a); break;
			case CEL_VA_INT64:
			case CEL_VA_UINT64:	GET_IU64(a); PUT_SU64(a); break;
			case CEL_VA_PTR:	GET_IP(a); PUT_SP(a); break;
#if 0
			case CEL_VA_SFLOAT:	GET_ISF(a.sflt); PUT_SSF(a.sflt); break;
			case CEL_VA_DFLOAT:	GET_IDF(a.dflt); PUT_SDF(a.dflt); break;
			case CEL_VA_QFLOAT:	GET_IQF(a.qflt); PUT_SQF(a.qflt); break;
#endif
			}
			break;

#define	MOP(op)													\
		GET_SU64(b);										\
		GET_SU64(a);										\
		switch (*(uint8_t *)regs[R_IP]) {									\
		case CEL_VA_UINT8:	PUT_SU64((uint8_t) ((uint8_t)a op (uint8_t)b)); break;		\
		case CEL_VA_INT8:	PUT_SU64((int8_t) ((int8_t)a op (uint8_t)b)); break;		\
		case CEL_VA_UINT16:	PUT_SU64((uint16_t) ((uint16_t)a op (uint16_t)b)); break;	\
		case CEL_VA_INT16:	PUT_SU64((int16_t) ((int16_t)a op (uint16_t)b)); break;		\
		case CEL_VA_UINT32:	PUT_SU64((uint32_t) ((uint32_t)a op (uint32_t)b)); break;	\
		case CEL_VA_INT32:	PUT_SU64((int32_t) ((int32_t)a op (uint32_t)b)); break;		\
		case CEL_VA_UINT64:	PUT_SU64((uint64_t) ((uint64_t)a op (uint64_t)b)); break;	\
		case CEL_VA_INT64:	PUT_SU64((int64_t) ((int64_t)a op (uint64_t)b)); break;		\
		}												\
		regs[R_IP]++;

#if 0
		case CEL_VA_SFLOAT:				\
			((cel_vm_any_t *)(regs[R_SP] - sizeof(cel_vm_any_t)))->sflt op a.sflt;	\
			break;					\
		case CEL_VA_DFLOAT:				\
			((cel_vm_any_t *)(regs[R_SP] - sizeof(cel_vm_any_t)))->dflt op a.dflt;	\
			break;					\
		case CEL_VA_QFLOAT:				\
			((cel_vm_any_t *)(regs[R_SP] - sizeof(cel_vm_any_t)))->qflt op a.qflt;	\
			break;					\
		}						\

#endif

		case CEL_I_ADD:
			MOP(+);
			break;

		case CEL_I_SUB:
			MOP(-);
			break;

		case CEL_I_MUL:
			MOP(*);
			break;

		case CEL_I_DIV:
			MOP(/);
			break;

		case CEL_I_NEG:
			GET_SU64(a);
			switch (*(uint8_t *)regs[R_IP]) {
			case CEL_VA_INT8:	PUT_SU64(-(int8_t)a); break;
			case CEL_VA_UINT8:	PUT_SU64(-(uint8_t)a); break;
			case CEL_VA_INT16:	PUT_SU64(-(int16_t)a); break;
			case CEL_VA_UINT16:	PUT_SU64(-(uint16_t)a); break;
			case CEL_VA_INT32:	PUT_SU64(-(int32_t)a); break;
			case CEL_VA_UINT32:	PUT_SU64(-(uint32_t)a); break;
			case CEL_VA_INT64:	PUT_SU64(-(int64_t)a); break;
			case CEL_VA_UINT64:	PUT_SU64(-(uint64_t)a); break;
#if 0
			case CEL_VA_SFLOAT:
				((cel_vm_any_t *) (regs[R_SP] - sizeof(cel_vm_any_t)))->sflt = -((cel_vm_any_t *) (regs[R_SP] - sizeof(cel_vm_any_t)))->sflt;
				break;
			case CEL_VA_DFLOAT:
				((cel_vm_any_t *) (regs[R_SP] - sizeof(cel_vm_any_t)))->dflt = -((cel_vm_any_t *) (regs[R_SP] - sizeof(cel_vm_any_t)))->dflt;
				break;
			case CEL_VA_QFLOAT:
				((cel_vm_any_t *) (regs[R_SP] - sizeof(cel_vm_any_t)))->qflt = -((cel_vm_any_t *) (regs[R_SP] - sizeof(cel_vm_any_t)))->qflt;
				break;
#endif
			}
			regs[R_IP]++;
			break;

		case CEL_I_NOT:
			((cel_vm_any_t *) (regs[R_SP] - sizeof(cel_vm_any_t)))->u32 = -((cel_vm_any_t *) (regs[R_SP] - sizeof(cel_vm_any_t)))->u32;
			break;

#define	CMP(ty, op)										\
		case ty:									\
			GET_SU64(b);								\
			GET_SU64(a);								\
			switch (*(uint8_t *)regs[R_IP]) {							\
			case CEL_VA_INT8:   PUT_SU64((int8_t) a op (int8_t)b);	break;		\
			case CEL_VA_UINT8:  PUT_SU64((uint8_t) a op (uint8_t)b); break;		\
			case CEL_VA_INT16:  PUT_SU64((int16_t) a op (int16_t)b); break;		\
			case CEL_VA_UINT16: PUT_SU64((uint16_t) a op (uint16_t)b); break;	\
			case CEL_VA_INT32:  PUT_SU64((int32_t) a op (int32_t)b); break;		\
			case CEL_VA_UINT32: PUT_SU64((uint32_t) a op (uint32_t)b); break;	\
			case CEL_VA_INT64:  PUT_SU64((int64_t) a op (int64_t)b); break;		\
			case CEL_VA_UINT64: PUT_SU64((uint64_t) a op (uint64_t)b); break;	\
			}									\
			regs[R_IP]++;								\
			break;									\

		CMP(CEL_I_TEQ, ==)
		CMP(CEL_I_TNE, !=)
		CMP(CEL_I_TLE, <=)
		CMP(CEL_I_TLT, <)
		CMP(CEL_I_TGT, >)
		CMP(CEL_I_TGE, >=)

		case CEL_I_BR:
			GET_II16(a);
			regs[R_IP] = oip + a;
			break;

		case CEL_I_BRT:
			GET_II16(b);
			GET_SU32(a);
			if (a)
				regs[R_IP] = oip + b;
			break;

		case CEL_I_BRF:
			GET_II16(b);
			GET_SU32(a);
			if (!a)
				regs[R_IP] = oip + b;
			break;

#define	INC(in, op)						\
		case in:					\
		GET_SP(a);					\
		GET_SU64(b);				\
		*(uint64_t *)a op b;			\
		break;

		INC(CEL_I_INCM, +=)
		INC(CEL_I_DECM, -=)
		INC(CEL_I_MULM, *=)
		INC(CEL_I_DIVM, /=)

		case CEL_I_STOM:
			GET_SP(a);
			switch (*(uint8_t *)regs[R_IP]) {
			case CEL_VA_INT8:
			case CEL_VA_UINT8:	GET_SU8(*((uint8_t *) a)); break;
			case CEL_VA_INT16:	
			case CEL_VA_UINT16:	GET_SU16(*((uint16_t *) a)); break;
			case CEL_VA_INT32:	
			case CEL_VA_UINT32:	GET_SU32(*((uint32_t *) a)); break;
			case CEL_VA_INT64:	
			case CEL_VA_UINT64:	GET_SU64(*((uint64_t *) a)); break;
			case CEL_VA_PTR:	GET_SU64(*((uint64_t *) a)); break;
#if 0
			case CEL_VA_SFLOAT:	GET_SSF(*((float *) a)); break;
			case CEL_VA_DFLOAT:	GET_SDF(*((double *) a)); break;
			case CEL_VA_QFLOAT:	GET_SQF(*((long double *) a)); break;
#endif
			}

			regs[R_IP]++;
			break;

		case CEL_I_LOADM:
			GET_SP(a);
			switch (*(uint8_t *)regs[R_IP]) {
			case CEL_VA_INT8:
			case CEL_VA_UINT8:	PUT_SU8(*((uint8_t *) a)); break;
			case CEL_VA_INT16:	
			case CEL_VA_UINT16:	PUT_SU16(*((uint16_t *) a)); break;
			case CEL_VA_INT32:	
			case CEL_VA_UINT32:	PUT_SU32(*((uint32_t *) a)); break;
			case CEL_VA_INT64:	
			case CEL_VA_UINT64:	PUT_SU64(*((uint64_t *) a)); break;
			case CEL_VA_PTR:	PUT_SP(*((void **) a)); break;
#if 0
			case CEL_VA_SFLOAT:	PUT_SSF(*((float *) a)); break;
			case CEL_VA_DFLOAT:	PUT_SDF(*((double *) a)); break;
			case CEL_VA_QFLOAT:	PUT_SQF(*((long double *) a)); break;
#endif
			}

			regs[R_IP]++;
			break;

		case CEL_I_CALLE: {
			void		**args;
			cel_vm_any_t	*aargs;
			cel_type_t	*ty;
			int		 i = 0;
			ffi_arg		 ret;

				GET_SU64(a);
				func = (cel_function_t *) a;
				args = malloc(sizeof(void *) * func->cf_nargs);
				aargs = malloc(sizeof(cel_vm_any_t) * func->cf_nargs);

				CEL_TAILQ_FOREACH(ty, func->cf_type->ct_type.ct_function.ct_args, ct_entry) {
					switch (ty->ct_tag) {
					case cel_type_int8:	GET_SI8(aargs[i].i8);   args[i] = &aargs[i].i8; break;
					case cel_type_uint8:	GET_SU8(aargs[i].u8);   args[i] = &aargs[i].u8; break;
					case cel_type_int16:	GET_SI16(aargs[i].i16); args[i] = &aargs[i].i16; break;
					case cel_type_uint16:	GET_SU16(aargs[i].u16); args[i] = &aargs[i].u16; break;
					case cel_type_int32:	GET_SI32(aargs[i].i32); args[i] = &aargs[i].i32; break;
					case cel_type_uint32:	GET_SU32(aargs[i].u32); args[i] = &aargs[i].u32; break;
					case cel_type_int64:	GET_SI64(aargs[i].i64); args[i] = &aargs[i].i64; break;
					case cel_type_uint64: 	GET_SU64(aargs[i].u64); args[i] = &aargs[i].u64; break;
					case cel_type_ptr:	GET_SU64(aargs[i].u64); args[i] = &aargs[i].u64; break;
#if 0
					case cel_type_sfloat:	GET_SSF(aargs[i].sflt); args[i] = &aargs[i].sflt; break;
					case cel_type_dfloat:	GET_SDF(aargs[i].dflt); args[i] = &aargs[i].dflt; break;
					case cel_type_qfloat:	GET_SQF(aargs[i].qflt); args[i] = &aargs[i].qflt; break;
#endif
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
				case cel_type_ptr:	PUT_SP((void *)ret); break;
#if 0
				case cel_type_sfloat:	PUT_SSF(*(float *)&ret); break;
				case cel_type_dfloat:	PUT_SDF(*(double *)&ret); break;
				case cel_type_qfloat:	PUT_SQF(*(long double *)&ret); break;
#endif
				}

				free(args);
				free(aargs);
			}
			break;

		case CEL_I_STOR:
			GET_IU8(a);
			GET_SU64(regs[a]);
			break;

		case CEL_I_LOADR:
			GET_IU8(a);
			PUT_SU64(regs[a]);
			break;

		case CEL_I_MOVR:
			GET_IU8(a);
			GET_IU8(b);
			regs[a] = regs[b];
			break;

		case CEL_I_POPD:
			regs[R_SP] -= sizeof(cel_vm_any_t);
			break;

		case CEL_I_LOADMR:
			GET_IU8(a);
			GET_IU32(b);
			PUT_SU64(* (uint64_t *) (regs[a] + b));
			break;

		case CEL_I_STOMR:
			GET_IU8(a);
			GET_IU32(b);
			GET_SU64(* (uint64_t *) (regs[a] + b));
			break;

		case CEL_I_INCR:
			GET_IU8(a);
			GET_IU32(b);
			regs[a] += b;
			break;

		case CEL_I_DECR:
			GET_IU8(a);
			GET_IU32(b);
			regs[a] -= b;
			break;

#if 0
		case CEL_I_ADDZN:
			GET_IU8(c);
			GET_SU64(a);
			GET_SU64(b);
			switch (c) {
			case 0: PUT_SU64(a + b); break;
			case 7: PUT_SU64((int8_t) (a + b)); break;
			case 6: PUT_SU64((int16_t) (a + b)); break;
			case 4: PUT_SU64((int32_t) (a + b)); break;
			}

		case CEL_I_ADDSX: {
			GET_IU8(c);
			GET_SU64(a);
			GET_SU64(b);
			switch (c) {
			case 0: PUT_SU64(a + b); break;
			case 7: PUT_SU64((int8_t) (a + b)); break;
			case 6: PUT_SU64((int16_t) (a + b)); break;
			case 4: PUT_SU64((int32_t) (a + b)); break;
			}
			break;
		}
#endif

		default:
			printf("can't decode %d\n", inst);
			return -1;
		}
	}
}
