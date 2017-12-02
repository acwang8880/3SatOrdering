/* Copyright (c) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011;
 * John Haskins, Jr.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by John Haskins, Jr.
 * 4. The name of John Haskins, Jr. be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN HASKINS, JR. ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL JOHN HASKINS, JR. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

typedef struct integer_st {
	unsigned int	nbits;
	unsigned int	nbytes;
	unsigned char *	data;
} integer_t;

typedef struct istack_st {
	unsigned int	nelem;
	unsigned int	nbits;
	unsigned int	nbytes;
	unsigned int	tosptr;
	integer_t **	block;
} istack_t;

#define	JHJR_SUCCESS		0
#define	JHJR_OVERFLOW		1
#define	JHJR_OPERANDMISMATCH	2
#define	JHJR_INVALIDOPERATOR	4
#define JHJR_DIV0ERROR		8

#define	JHJR_OP_AND		0
#define	JHJR_OP_OR		1
#define	JHJR_OP_XOR		2
#define	JHJR_OP_NOT		3
#define	JHJR_OP_SLL		4
#define	JHJR_OP_SRL		5
#define	JHJR_OP_SRA		6

int		jhjr_init(void);
int		jhjr_shutdown(void);
integer_t *	mkinteger(unsigned int,int);
void		rminteger(integer_t *);
int		cpinteger(integer_t *,integer_t *);
void		stinteger(integer_t *,int);
void		mkstack(unsigned int,unsigned int);
void		rmstack(void);
int		jhjr_add(integer_t *,integer_t *,integer_t *);
int		jhjr_shift(integer_t *,integer_t *,unsigned int,unsigned char);
#define		jhjr_sll(d,s,N)		jhjr_shift(d,s,N,JHJR_OP_SLL)
#define		jhjr_srl(d,s,N)		jhjr_shift(d,s,N,JHJR_OP_SRL)
#define		jhjr_sra(d,s,N)		jhjr_shift(d,s,N,JHJR_OP_SRA)
int		jhjr_bitop(integer_t *,integer_t *,integer_t *,unsigned char);
#define		jhjr_and(d,s0,s1)	jhjr_bitop(d,s0,s1,JHJR_OP_AND)
#define		jhjr_or(d,s0,s1)	jhjr_bitop(d,s0,s1,JHJR_OP_OR)
#define		jhjr_xor(d,s0,s1)	jhjr_bitop(d,s0,s1,JHJR_OP_XOR)
#define		jhjr_not(d,s0)		jhjr_bitop(d,s0,NULL,JHJR_OP_NOT)
int		jhjr_compar(integer_t *,integer_t *);
#define		jhjr_eq(s0,s1)		(jhjr_compar(s0,s1)==0)
#define		jhjr_lt(s0,s1)		(jhjr_compar(s0,s1)==-1)
#define		jhjr_gt(s0,s1)		(jhjr_compar(s0,s1)==1)
#define		jhjr_le(s0,s1)		(jhjr_eq(s0,s1) || jhjr_lt(s0,s1))
#define		jhjr_ge(s0,s1)		(jhjr_eq(s0,s1) || jhjr_gt(s0,s1))
int		jhjr_divmod(integer_t *,integer_t *,integer_t *,integer_t *);
#define		jhjr_div(q,s0,s1)	jhjr_divmod(q,NULL,s0,s1)
#define		jhjr_mod(r,s0,s1)	jhjr_divmod(NULL,r,s0,s1)
int		jhjr_mul(integer_t *,integer_t *,integer_t *);
void		jhjr_dump(integer_t *);
void		jhjr_fputs(integer_t *,FILE *);
#define		jhjr_puts(s)		jhjr_fputs(s,stdout);
