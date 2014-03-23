/*
 * CEL: The Compact Embeddable Language.
 * Copyright (c) 2014 Felicity Tarnell.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely. This software is provided 'as-is', without any express or implied
 * warranty.
 */

#ifndef	CEL_VM_INSTR_H
#define	CEL_VM_INSTR_H

#include	<inttypes.h>

typedef	uint8_t	cel_vm_instr_t;

/*
 * Argument types.
 */
#define	CEL_VA_VOID	0
#define	CEL_VA_INT8	1
#define	CEL_VA_UINT8	2
#define	CEL_VA_INT16	3
#define	CEL_VA_UINT16	4
#define	CEL_VA_INT32	5
#define	CEL_VA_UINT32	6
#define	CEL_VA_INT64	7
#define	CEL_VA_UINT64	8
#define	CEL_VA_PTR	9
#define	CEL_VA_SFLOAT	10
#define	CEL_VA_DFLOAT	11
#define	CEL_VA_QFLOAT	12

/*	NOP */
#define	CEL_I_NOP	0

/*
 *	Load zero
 *	Arguments: none
 */
#define	CEL_I_LOADZ4	1	/* load int32 zero		*/
#define	CEL_I_LOADZ8	2	/* load int64 zero		*/

/*	
 *	Load immediate
 *	Arguments:	immediate 1-byte type id
 *			immediate value of appropriate size
 */
#define	CEL_I_LOADI	3

/*
 *	Store to register
 *	Arguments:	immediate 1-byte register number
 */
#define	CEL_I_STOR	4

/*
 *	Load from register
 *	Arguments:	immediate 1-byte register number
 */
#define	CEL_I_LOADR	5

/*
 * 	Pop stack and discard
 * 	Arguments: none
 */
#define	CEL_I_POPD	6	/* pop stack			*/

/*
 *	Arithmetic
 *	Arguments:	immediate 1-byte type
 */
#define	CEL_I_ADD	7
#define	CEL_I_SUB	8
#define	CEL_I_MUL	9
#define	CEL_I_DIV	10
#define	CEL_I_NEG	11
#define	CEL_I_NOT	12

/*
 * 	Conditional tests
 * 	Arguments: 	immediate 1-byte type
 */
#define	CEL_I_TEQ	13
#define	CEL_I_TNE	14
#define	CEL_I_TLE	15
#define	CEL_I_TLT	16
#define	CEL_I_TGE	17
#define	CEL_I_TGT	18

/*
 *	Return
 *	Arguments:	immediate 1-byte return type
 */
#define	CEL_I_RET	19

/*
 * 	Branches
 * 	Arguments:	immediate signed 16-bit offset
 */
#define	CEL_I_BRT	20	/* branch if true			*/
#define	CEL_I_BRF	21	/* branch if false			*/
#define	CEL_I_BR	22	/* unconditional branch			*/

/*
 *	Inc / dec memory location
 *	Arguments:	stack pointer to memory location
 *			stack 64-bit value
 */
#define	CEL_I_INCM	23
#define	CEL_I_DECM	24
#define	CEL_I_MULM	25
#define	CEL_I_DIVM	26

/*
 * 	Call function
 * 	Arguments:	immediate two-byte function index
 */
#define	CEL_I_CALL	27
#define	CEL_I_CALLE	28	/* call external			*/

/*
 *	Store to LR
 *	Arguments:	none
 */
#define	CEL_I_STOLR	29

/*
 *	Allocate variable storage
 *	Arguments:	immediate 1-byte variable count
 */
#define	CEL_I_ALLV	30

/*
 * 	Store to memory
 * 	Arguments:	immediate 1-byte type
 * 			stack pointer to memory location
 * 			stack variable-size value
 */
#define	CEL_I_STOM	31	/* store to memory			*/

/*
 * 	Load from memory
 * 	Arguments:	immediate 1-byte type
 * 			stack pointer to memory location
 */
#define	CEL_I_LOADM	32	/* load from memory			*/

/*
 *	Take address of local variable
 *	Arguments:	immediate two-byte variable index
 */
#define	CEL_I_VADDR	33

/*
 *	Move register to register
 *	Arguments:	immediate 1-byte dest register number
 *			immediate 1-byte src register number
 */
#define	CEL_I_MOVR	34

/*
 *	Load from memory address by register with offset
 *	Arguments:	immediate 1-byte register number
 *			immediate 4-byte offset
 */
#define	CEL_I_LOADMR	35

/*
 *	Store to memory address by register with offset
 *	Arguments:	immediate 1-byte register number
 *			immediate 4-byte offset
 */
#define	CEL_I_STOMR	36

/*
 *	Increment register by immediate
 *	Arguments:	immediate 1-byte register number
 *			immediate 4-byte value
 */
#define	CEL_I_INCR	37

/*
 *	Decrement register by immediate
 *	Arguments:	immediate 1-byte register number
 *			immediate 4-byte value
 */
#define	CEL_I_DECR	38

#endif	/* !CEL_VM_INSTR_H */
