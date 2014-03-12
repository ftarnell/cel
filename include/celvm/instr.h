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
 *	Load zero
 *	Arguments: none
 */
#define	CEL_I_LOADZ4	1	/* load int32 zero		*/
#define	CEL_I_LOADZ8	2	/* load int64 zero		*/

/*	
 *	Load immediate
 *	Arguments: immediate value of appropriate size
 */
#define	CEL_I_LOADI4	4	/* load immed int32		*/
#define	CEL_I_LOADI8	5	/* load immed int64		*/

/*
 *	Load variable
 *	Arguments: immediate 2-byte variable index
 */
#define	CEL_I_LOADV4	8	/* load int32 variable		*/
#define	CEL_I_LOADV8	9	/* load int64 variable		*/

/*
 * 	Pop stack and discard
 * 	Arguments: none
 */
#define	CEL_I_POPD	10	/* pop stack			*/

/*
 *	Store variable
 *	Arguments: immediate 2-byte variable index
 */
#define	CEL_I_STOV4	13	/* store int32 variable		*/
#define	CEL_I_STOV8	14	/* store int64 variable		*/

/*
 *	Arithmetic
 *	Arguments: none
 */
#define	CEL_I_ADD4	15	/* add two ints and push	*/
#define	CEL_I_ADD8	16	/* add two int64s and push	*/
#define	CEL_I_SUB4	17	/* add two ints and push	*/
#define	CEL_I_SUB8	18	/* add two int64s and push	*/
#define	CEL_I_MUL4	19	/* add two ints and push	*/
#define	CEL_I_MUL8	20	/* add two int64s and push	*/
#define	CEL_I_DIV4	21	/* add two ints and push	*/
#define	CEL_I_DIV8	22	/* add two int64s and push	*/

/*
 *	Return
 *	Arguments: none
 */
#define	CEL_I_RET4	28	/* return int32			*/
#define	CEL_I_RET8	29	/* return int64			*/

#endif	/* !CEL_VM_INSTR_H */
