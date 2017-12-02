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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "jhjr_math.h"


static pthread_key_t		jhjr_stack;

static inline integer_t *	pop(void);
static inline void		push(void);


int
jhjr_init(void)
{
   	pthread_key_create(&jhjr_stack, NULL);

	return 0;
}

int
jhjr_shutdown(void)
{
	pthread_key_delete(jhjr_stack);

	return 0;
}

integer_t *
mkinteger(unsigned int nbits,int N)
{
	integer_t *	retval;
	unsigned int	nbytes;

	if (nbits<sizeof(int)*8) {
		return NULL;
	}
	retval=(integer_t *)malloc(sizeof(integer_t));
	assert(retval);
	retval->nbytes=nbytes=(nbits>>3)+((nbits & 0x7) ? 1 : 0);
#if 1
	retval->nbits=nbits;
#else
	retval->nbits=nbytes<<3;
#endif
	retval->data=(unsigned char *)malloc(nbytes);
	assert(retval->data);

	stinteger(retval,N);

	return retval;
}

void
rminteger(integer_t * s)
{
	if (s==NULL)
		return;

	if (s->data)
		free(s->data);
	free(s);
}

int
cpinteger(integer_t * d,integer_t * s)
{
	int	retval=JHJR_SUCCESS;

	assert(d);
	assert(s);
	if (d->nbytes>=s->nbytes) {
		unsigned int	offset=d->nbytes-s->nbytes;

		memcpy((void *)(d->data+offset),(void *)s->data,s->nbytes);
	} else {
		retval=JHJR_OVERFLOW;
	}

	return retval;
}

void
stinteger(integer_t * d,int N)
{
	assert(d);

	if (N & 0x80000000)
		memset((void *)d->data,0xff,d->nbytes);
	else
		memset((void *)d->data,0x00,d->nbytes);
#if	1
/* a "salute" to the x86 architecture's little-endianness... */
	*((unsigned char *)(d->data+d->nbytes-1)-3)=(N & 0xff000000)>>24;
	*((unsigned char *)(d->data+d->nbytes-1)-2)=(N & 0x00ff0000)>>16;
	*((unsigned char *)(d->data+d->nbytes-1)-1)=(N & 0x0000ff00)>>8;
	*((unsigned char *)(d->data+d->nbytes-1)-0)=(N & 0x000000ff);
#else
/* for big-endianness... */
	*((unsigned char *)((d->data+(d->nbytes-sizeof(int))))+3)=(N & 0xff000000)>>24;
	*((unsigned char *)((d->data+(d->nbytes-sizeof(int))))+2)=(N & 0x00ff0000)>>16;
	*((unsigned char *)((d->data+(d->nbytes-sizeof(int))))+1)=(N & 0x0000ff00)>>8;
	*((unsigned char *)((d->data+(d->nbytes-sizeof(int))))+0)=(N & 0x000000ff);
#endif
}

void
mkstack(unsigned int nbits,unsigned int nelem)
{
	unsigned int	i;
	istack_t *	S;	

	if (nelem<=0)
		return;

	S=(istack_t *)malloc(sizeof(istack_t));
	assert(S);
	S->nelem=nelem;
	S->nbits=nbits;
	S->nbytes=(nbits>>3)+((nbits & 0x7) ? 1 : 0);
	S->tosptr=0;
	S->block=(integer_t **)malloc(sizeof(integer_t)*nelem);
	assert(S->block);
	for(i=0;i<nelem;i++) {
		S->block[i]=mkinteger(nbits,0);
		assert(S->block[i]);
	}

	pthread_setspecific(jhjr_stack,(void *)S);
}

void
rmstack(void)
{
	unsigned int	i;
	istack_t *	S;

	S=(istack_t *)pthread_getspecific(jhjr_stack);
	if (S==NULL)
		return;
	for(i=0;i<S->nelem;i++) {
		if (S->block[i])
			rminteger(S->block[i]);
	}
	free(S);
}

static inline integer_t *
pop(void)
{
	integer_t *	retval;
	istack_t *	S;

	S=(istack_t *)pthread_getspecific(jhjr_stack);
	assert(S);
	if (S->tosptr>=(S->nelem-1))
		return NULL;
	retval=S->block[S->tosptr];
	S->tosptr+=1;

	return retval;
}

static inline void
push(void)
{
	istack_t *	S;

	S=(istack_t *)pthread_getspecific(jhjr_stack);
	assert(S);
	if (S->tosptr==0)
		return;
	S->tosptr-=1;
}

int
jhjr_add(integer_t * d,integer_t * s0,integer_t * s1)
{
	int		i,retval=JHJR_SUCCESS;
	integer_t *	tmp;
	integer_t *	src0;
	integer_t *	src1;
	unsigned int	intermediate;

	assert(d);
	assert(s0);
	assert(s1);

	if ((d==s0) || (d==s1)) {
		tmp=pop();
		assert(tmp);
	} else {
		tmp=d;
	}
	stinteger(tmp,0);

	src1=s1;
	src0=s0;

	intermediate=0;
	for (i=src0->nbytes-1;i>=0;i--) {
		intermediate=src1->data[i]+src0->data[i]+(intermediate>>8);
		tmp->data[i]=intermediate&0xff;
	}
	retval=((intermediate) ? JHJR_OVERFLOW : JHJR_SUCCESS);

	if (d!=tmp) {
		cpinteger(d,tmp);
		push(); /* tmp */
	}

	return retval;
}

int
jhjr_shift(integer_t * d,integer_t * s,unsigned int N,unsigned char op)
{
	int		i,retval=JHJR_SUCCESS;
	integer_t *	tmp;
	unsigned int	intermediate;
	unsigned int	offset;
	unsigned int	mask7;

	assert(d);
	assert(s);

	if (d==s) {
		tmp=pop();
		assert(tmp);
	} else {
		tmp=d;
	}

	if (0==N) {
		cpinteger(tmp,s);

		goto done;
	}

	stinteger(tmp,0);

	offset=N>>3;
	mask7=N&0x7;
	intermediate=0;
	switch (op) {
	case JHJR_OP_SLL:
		if (N>=tmp->nbits)
			break;
		memcpy((void *)tmp->data,(void *)(s->data+offset),s->nbytes-offset);
		if (mask7==0)
			break;
		for(i=tmp->nbytes-1-offset;i>=0;i--) {
			intermediate=(tmp->data[i]<<mask7)|(intermediate>>8);
			tmp->data[i]=intermediate&0xff;
		}
		break;
	case JHJR_OP_SRA:
		if (tmp->data[0] & 0x80)
			stinteger(tmp,-1);
		intermediate=0xff<<(8-mask7);
	case JHJR_OP_SRL:
		if (N>=tmp->nbits)
			break;
		memcpy((void *)(tmp->data+offset),(void *)s->data,s->nbytes-offset);
		if (mask7==0)
			break;
		for(i=offset;i<tmp->nbytes;i++) {
			unsigned char	scratch;

			scratch=tmp->data[i];
			tmp->data[i]=(intermediate&0xff)|(scratch>>mask7);
			intermediate=scratch<<(8-mask7);
		}
		break;
	default:
		retval=JHJR_INVALIDOPERATOR;
		goto done;
	}

	if (d!=tmp) {
		cpinteger(d,tmp);
	}
done:
	if (d!=tmp) {
		push(); /* tmp */
	}

	return retval;
}

int
jhjr_bitop(integer_t * d,integer_t * s0,integer_t * s1,unsigned char op)
{
	int		i,retval=JHJR_SUCCESS;
	integer_t *	tmp;

	assert(d);
	assert(s0);
	if (op!=JHJR_OP_NOT)
		assert(s1);
	else
		assert(s1==NULL);

	if ((d==s0) || (d==s1)) {
		tmp=pop();
		assert(tmp);
	} else {
		tmp=d;
	}
	stinteger(tmp,0);

	if (op!=JHJR_OP_NOT && s0->nbytes!=s1->nbytes) {
		retval=JHJR_OPERANDMISMATCH;
		goto error;
	}

	switch (op) {
		case JHJR_OP_AND:
			for(i=s0->nbytes-1;i>=0;i--)
				tmp->data[i]=(s0->data[i] & s1->data[i]);
			break;
		case JHJR_OP_OR:
			for(i=s0->nbytes-1;i>=0;i--)
				tmp->data[i]=(s0->data[i] | s1->data[i]);
			break;
		case JHJR_OP_XOR:
			for(i=s0->nbytes-1;i>=0;i--)
				tmp->data[i]=(s0->data[i] ^ s1->data[i]);
			break;
		case JHJR_OP_NOT:
			for(i=s0->nbytes-1;i>=0;i--)
				tmp->data[i]=~(s0->data[i]);
			break;
		default:
			retval=JHJR_INVALIDOPERATOR;
			goto error;
	}

	if (d!=tmp) {
		cpinteger(d,tmp);
	}
error:
	if (d!=tmp) {
		push(); /* tmp */
	}
	
	return retval;
}

int
jhjr_compar(integer_t * s0,integer_t * s1)
{
	int		i,retval;
	integer_t *	src0;
	integer_t *	src1;
	unsigned char	s0_negative=0;
	unsigned char	s1_negative=0;

	assert(s0);
	assert(s1);

	src1=s1;
	src0=s0;

	if (src0->data[0] & 0x80) {
		s0_negative=1;
	}
	if (src1->data[0] & 0x80) {
		s1_negative=1;
	}

/* retval:
 *	-1 if s0 < s1
 *	 0 if s0 = s1
 *	 1 if s0 > s1
 */
	if (s0_negative && !s1_negative) {
		retval=-1;

		goto done;
	}
	if (!s0_negative && s1_negative) {
		retval=1;

		goto done;
	}

	retval=0;
	for(i=0;retval==0 && i<src0->nbytes;i++) {
		if (src0->data[i]<src1->data[i]) {
			retval=-1;
		} else
		if (src0->data[i]>src1->data[i]) {
			retval=1;
		}
	}

	if (s0_negative && s1_negative) {
		retval*=-1;
	}

done:
	return retval;
}

int
jhjr_divmod(integer_t * q,integer_t *r ,integer_t * s0,integer_t *s1)
{
	int		i,retval=JHJR_SUCCESS;
	integer_t *	zero;
	integer_t *	one;
	integer_t *	tmp0;
	integer_t *	tmp1;
	integer_t *	tmp2;
	integer_t *	tmp3;
	integer_t *	tmpq;
	integer_t *	tmpr;
	integer_t *	src0;
	integer_t *	src1;
	unsigned char	s0_negative=0;
	unsigned char	s1_negative=0;

	assert(s0);
	assert(s1);

	zero=pop();
	one=pop();
	src0=pop();
	src1=pop();

	assert(zero);
	assert(one);
	assert(src0);
	assert(src1);

	stinteger(zero,0);
	stinteger(one,1);

	if (jhjr_eq(s1,zero)) {
		retval=JHJR_DIV0ERROR;

		goto done;
	}

	if ((NULL==q) && (NULL==r)) {
		goto done;
	}

	if (jhjr_lt(s0,zero)) {
		jhjr_not(src0,s0);
		jhjr_add(src0,src0,one);
		s0_negative=1;
	} else {
		cpinteger(src0,s0);
	}
	if (jhjr_lt(s1,zero)) {
		jhjr_not(src1,s1);
		jhjr_add(src1,src1,one);
		s1_negative=1;
	} else {
		cpinteger(src1,s1);
	}

	if (jhjr_lt(src0,src1)) {
		if (NULL!=q) {
			stinteger(q,0);
		}
		if (NULL!=r) {
			if (s0_negative) {
				jhjr_not(src0,src0);
				jhjr_add(src0,src0,one);
			}
			cpinteger(r,src0);
		}

		goto done;
	}

	tmp0=pop();
	tmp1=pop();
	tmp2=pop();
	tmp3=pop();

	assert(tmp0);
	assert(tmp1);
	assert(tmp2);
	assert(tmp3);

	if ((q==src0) || (q==src1)) {
		tmpq=pop();
		assert(tmpq);
	} else {
		tmpq=q;
	}
	if ((r==src0) || (r==src1)) {
		tmpr=pop();
		assert(tmpr);
	} else {
		tmpr=r;
	}

	cpinteger(tmpq,zero);
	cpinteger(tmpr,src0);
	while (jhjr_ge(tmpr,src1)) {
		cpinteger(tmp1,src1);
		i=0;
		while (jhjr_lt(tmp1,tmpr)) {
			jhjr_sll(tmp1,tmp1,1);
			i+=1;
		}
		if (jhjr_gt(tmp1,tmpr)) {
			jhjr_srl(tmp1,tmp1,1);
			i-=1;
		}
		jhjr_not(tmp1,tmp1);
		jhjr_add(tmp1,tmp1,one);
		jhjr_add(tmpr,tmpr,tmp1);
		jhjr_sll(tmp0,one,i);
		jhjr_add(tmpq,tmpq,tmp0);
	}

	if (s0_negative) {
		jhjr_not(tmpr,tmpr);
		jhjr_add(tmpr,tmpr,one);
	}
	if (s0_negative!=s1_negative) {
		jhjr_not(tmpq,tmpq);
		jhjr_add(tmpq,tmpq,one);
	}

	if (r!=tmpr) {
		cpinteger(r,tmpr);
		push(); /* tmpr */
	}
	if (q!=tmpq) {
		cpinteger(q,tmpq);
		push(); /* tmpq */
	}
	push(); /* tmp3 */
	push(); /* tmp2 */
	push(); /* tmp1 */
	push(); /* tmp0 */

done:
	push(); /* src1 */
	push(); /* src0 */
	push(); /* one */
	push(); /* zero */

	return retval;
}

int
jhjr_mul(integer_t * d,integer_t * s0,integer_t *s1)
{
	int	i,retval=JHJR_SUCCESS;
	integer_t *	zero;
	integer_t *	one;
	integer_t *	src0;
	integer_t *	src1;
	integer_t *	tmp0;
	integer_t *	tmp1;
	unsigned char	s0_negative=0;
	unsigned char	s1_negative=0;

	assert(d);
	assert(s0);
	assert(s1);

	zero=pop();
	one=pop();
	src0=pop();
	src1=pop();

	assert(zero);
	assert(one);
	assert(src0);
	assert(src1);

	stinteger(zero,0);
	stinteger(one,1);

	if (jhjr_eq(s0,zero) || jhjr_eq(s1,zero)) {
		stinteger(d,0);

		goto done;
	} else
	if (jhjr_eq(s0,one)) {
		cpinteger(d,s1);

		goto done;
	} else
	if (jhjr_eq(s1,one)) {
		cpinteger(d,s0);

		goto done;
	}

	if (jhjr_lt(s0,zero)) {
		jhjr_not(src0,s0);
		jhjr_add(src0,src0,one);
		s0_negative=1;
	} else {
		cpinteger(src0,s0);
	}
	if (jhjr_lt(s1,zero)) {
		jhjr_not(src1,s1);
		jhjr_add(src1,src1,one);
		s1_negative=1;
	} else {
		cpinteger(src1,s1);
	}

	if ((d==s0) || (d==s1)) {
		tmp0=pop();
		assert(tmp0);
	} else {
		tmp0=d;
	}

	tmp1=pop();
	assert(tmp1);

	if (jhjr_gt(src0,src1)) {
		cpinteger(tmp1,src0);
		cpinteger(src0,src1);
		cpinteger(src1,tmp1);
	}
	stinteger(tmp0,0);
	stinteger(tmp1,0);
	i=0;
	while(jhjr_gt(src0,zero)) {
		jhjr_and(tmp1,src0,one);
		if (jhjr_eq(tmp1,one)) {
			jhjr_sll(tmp1,src1,i);
			retval|=jhjr_add(tmp0,tmp0,tmp1);
		}
		jhjr_srl(src0,src0,1);
		i+=1;
	}
	if (1==(s0_negative+s1_negative)) {
		// either s0 or s1 was a negative
		// number, but NOT both
		jhjr_not(tmp0,tmp0);
		jhjr_add(tmp0,tmp0,one);
	}

	push(); /* tmp1 */

	if (d!=tmp0) {
		cpinteger(d,tmp0);
		push(); /* tmp0 */
	}

done:
	push(); /* src1 */
	push(); /* src0 */
	push(); /* one */
	push(); /* zero */

	return retval;
}

void
jhjr_dump(integer_t * s)
{
	int	i;

	printf("jhjr_dump(): s->nbits = %d\n",s->nbits);
	printf("jhjr_dump(): s->nbytes = %d\n",s->nbytes);
	for(i=0;i<s->nbytes;i++)
		printf("jhjr_dump(): s->data[%2d] = %02x\n",i,s->data[i]);
}

void
jhjr_fputs(integer_t * s,FILE * fp)
{
	int	i;

	for(i=0;i<s->nbytes;i++)
		fprintf(fp,"%02x",s->data[i]);
}
