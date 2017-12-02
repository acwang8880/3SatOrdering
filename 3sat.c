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
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>

#include "jhjr_math.h"


#define	BIN_FILENAME	argv[0]
#define	IFILENAME	1
#define	OFILENAME	2
#define	MAXSTRLEN	256
#define max(a,b)	({ __typeof__ (a) _a = (a); \
				__typeof__ (b) _b = (b); \
				_a > _b ? _a : _b; })


struct clause_te {
	unsigned int	v1;
	unsigned int	v2;
	unsigned int	v3;
	unsigned int	inv;
} clause_te;

struct solve_pckt {
	int *		status_report;
	integer_t *	s_start;
	integer_t *	s_end;
};

struct solve_pckt *	arg=NULL;
int			NVAR_MAX;
int			n_var;
int			n_clause;
int			n_thread=1;
int			n_solution=0;
int *			status_report;
struct clause_te *	psi=NULL;
pthread_mutex_t		printf_lock;
pthread_mutex_t		restart_lock;
pthread_mutex_t		solution_lock;
integer_t **		eval0_bits=NULL;
integer_t **		eval0_mask=NULL;


int			main(int,char **);
int			parseoptions(int,char **,char **,int *);
int			clause_compar(const void *,const void *);
int			clause_count(struct clause_te *);
static inline int	EVAL(integer_t *,int,integer_t *);
void *			solve(void *);
int			restart(integer_t *,integer_t *);
void			status(int);
void			dump(struct clause_te *,int);
void			freeze(struct clause_te *,int,int,FILE *);
long long		choose(int,int);
long long		ipow(int,int);
size_t			strcnt(const char *,char);


int
main(int argc,char ** argv)
{
	struct clause_te *	scratch=NULL;
	unsigned int		scr_v1;
	unsigned int		scr_v2;
	unsigned int		scr_v3;
	int			i;
	int			j=0;
	struct rusage		usage[2];
	char *			filename=NULL;
	FILE *			ifp=NULL;
	FILE *			ofp=NULL;
	int			optval;

	if (0>(optval=parseoptions(argc,argv,&filename,&n_thread))) {
		exit(1);
	}
	if (IFILENAME==optval) {
		if (0==strcmp(filename,"-")) {
			ifp=stdin;
		} else {
			char *	tmp[3];
			char *	buf;

			assert(NULL!=(tmp[0]=(char *)malloc(MAXSTRLEN)));
			assert(NULL!=(tmp[1]=(char *)malloc(MAXSTRLEN)));
			assert(NULL!=(tmp[2]=(char *)malloc(MAXSTRLEN)));
			ifp=fopen(filename,"r");
			if (NULL==ifp) {
				fprintf(stderr,"%s: Cannot open "
					       "'%s' for obtaining "
						"Boolean expression.\n",
					BIN_FILENAME,argv[4]);
				exit(1);
			}
			buf=tmp[2];
			while (NULL!=(fgets(buf,MAXSTRLEN,ifp))) {
				(void)sscanf(buf,"%s",tmp[0]);
				if (0==strncmp("c",tmp[0],MAXSTRLEN)) {
					/*
					 * a comment... nothing to do.
					 */
					continue;
				} else
				if (0==strncmp("p",tmp[0],MAXSTRLEN)) {
					/*
					 * TODO: Some of these asserts are really
					 * lazy.  I should retool this to generate
					 * useful error diagnostics...
					 */					
					assert(4==sscanf(buf,"%s %s %d %d", tmp[0],tmp[1],&n_var,&n_clause));
					assert(0==strncmp("p",tmp[0],MAXSTRLEN));
					assert(0==strncmp("cnf",tmp[1],MAXSTRLEN));
					assert(n_var>=3);
					printf("8*choose(%d,3) = %lld\n",n_var,8*choose(n_var,3));
					assert(n_clause<=8*choose(n_var,3));
					assert(NULL!=(psi=(struct clause_te *)malloc(sizeof(struct clause_te)*n_clause)));
				} else {
					struct clause_te *	p;
					int			v1;
					int			v2;
					int			v3;
					int			terminator;

					assert(4==sscanf(buf,"%d %d %d %d",&v1,&v2,&v3,&terminator));
					assert(0==terminator);
					p=psi+j;
					/*
					 * Certain elements of the solve() algorithm
					 * depend on abs(v1) < abs(v2) < abs(v3), so
					 * let's be sure to sort the variables accordingly here.
					 */
					if (abs(v2)>abs(v3)) {
						v3^=v2;
						v2^=v3;
						v3^=v2;
					}
					if (abs(v1)>abs(v3)) {
						v3^=v1;
						v1^=v3;
						v3^=v1;
					}
					if (abs(v1)>abs(v2)) {
						v1^=v2;
						v2^=v1;
						v1^=v2;
					}
					p->v1=abs(v1);
					p->v2=abs(v2);
					p->v3=abs(v3);
					p->inv=((0>v1) ? 4 : 0)+((0>v2) ? 2 : 0)+((0>v3) ? 1 : 0);
					j+=1;
#if 0
					printf("%08x %08x %08x %d\n",p->v1,p->v2,p->v3,p->inv);
#endif
				}
			}
			free(tmp[0]);
			free(tmp[1]);
			free(tmp[2]);
			fclose(ifp);
			ifp=NULL;
			getrusage(RUSAGE_SELF,&usage[0]);

			goto launch;
		}
	} else {
		if (n_var<3) {
			fprintf(stderr,"%s: n_var must be >= 3!\n",
				BIN_FILENAME);
			exit(1);
		}
		if ((n_clause<1) || (n_clause>(8*choose(n_var,3)))) {
			fprintf(stderr,"%s: n_clause must be "
				       "[1,8*binom(n_var,3)]!\n",
				BIN_FILENAME);
			exit(1);
		} else {
			printf("main(): MAX(n_clause) = %lld\n",8*choose(n_var,3));
		}
	} 
	if (OFILENAME==optval) {
		if (0==strcmp(filename,"-")) {
			ofp=stdout;
		} else {
			ofp=fopen(argv[4],"w");
			if (NULL==ofp) {
				fprintf(stderr,"%s: Cannot open "
					       "'%s' for storing "
						"Boolean expression.\n",
					BIN_FILENAME,argv[4]);
				exit(1);
			}
		}
	}

	psi=(struct clause_te *)malloc(sizeof(struct clause_te)*n_clause);
	if (psi==NULL) {
		fprintf(stderr,"Cannot malloc() psi!\n");
		exit(2);
	}

	scratch=(struct clause_te *)malloc(sizeof(struct clause_te)*8*choose(n_var,3));
	scr_v1=1;
	scr_v2=2;
	scr_v3=3;
	for(i=0;i<8*choose(n_var,3);i+=8) {
		int	j;

		for(j=0;j<8;j++) {
			scratch[i+j].inv=j;
			scratch[i+j].v1=scr_v1;
			scratch[i+j].v2=scr_v2;
			scratch[i+j].v3=scr_v3;
		}

		scr_v3+=1;
		if (scr_v3>n_var) {
			scr_v2+=1;
			if (scr_v2>n_var-1) {
				scr_v1+=1;
				scr_v2=scr_v1+1;
			}
			scr_v3=scr_v2+1;
		}
	}

	j=8*choose(n_var,3);
	i=0;
	while (n_clause!=j) {
		if (!(scratch[i].inv&0x80000000) && (rand()&0x1)) {
			scratch[i].inv=0x80000000;
			j-=1;
		}
		i+=1;
		i%=8*choose(n_var,3);
	}
	j=0;
	for(i=0;i<8*choose(n_var,3);i++) {
		if (!(scratch[i].inv&0x80000000)) {
			psi[j].v1=scratch[i].v1;
			psi[j].v2=scratch[i].v2;
			psi[j].v3=scratch[i].v3;
			psi[j].inv=scratch[i].inv;
			j+=1;
		}
	}
	getrusage(RUSAGE_SELF,&usage[0]);
	printf("main(): getrusage() -> %15ld %15ld\n",
		(1000000*usage[0].ru_utime.tv_sec+usage[0].ru_utime.tv_usec),
		(1000000*usage[0].ru_stime.tv_sec+usage[0].ru_stime.tv_usec));
	qsort(psi,n_clause,sizeof(struct clause_te),clause_compar);

	if (ofp) {
		freeze(psi,n_var,n_clause,ofp);
		fclose(ofp);
		ofp=NULL;
	}

launch:
	/*
	 * Certain elements of the solve() algorithm
	 * depend on f(Ci) < f(Cj), where Ci = (a + b + c), Cj = (d + e + f),
	 * and f(x + y + z) = x + y * n_var + z * n_var^2; so
	 * let's be sure to sort the clauses accordingly here.
	 */
	qsort(psi,n_clause,sizeof(struct clause_te),clause_compar);
#if defined(DEBUG)
	printf("psi =\n");
	dump(psi,n_clause);
#endif
	status_report=(int *)malloc(sizeof(int)*n_thread);
	assert(status_report);

	if (clause_count(psi)) {
		goto done;
	}
	assert(signal(SIGUSR1,status)!=SIG_ERR);
	printf("main(): starting solve()...\n");

	pthread_t *		thread=NULL;
	integer_t *		one;
	integer_t *		negativeone;
	integer_t *		n;
	integer_t *		d;
	integer_t *		q;
	integer_t *		r;

	jhjr_init();
	NVAR_MAX=max(n_var+1,32); /* ... +1 prevents overflow from addition */
	mkstack(NVAR_MAX,64);

#if 0
	n=mkinteger(NVAR_MAX,239);
	d=mkinteger(NVAR_MAX,-2);
	q=mkinteger(NVAR_MAX,0);
	r=mkinteger(NVAR_MAX,0);
	jhjr_divmod(q,r,n,d);
	printf("main(): n = ");
	jhjr_puts(n);
	printf("\n");
	printf("main(): d = ");
	jhjr_puts(d);
	printf("\n");
	printf("main(): q = ");
	jhjr_puts(q);
	printf("\n");
	printf("main(): r = ");
	jhjr_puts(r);
	printf("\n");

	jhjr_mul(n,q,d);
	printf("main(): n = ");
	jhjr_puts(n);
	printf("\n");
	jhjr_add(n,n,r);
	printf("main(): n = ");
	jhjr_puts(n);
	printf("\n");
	exit(0);
#endif

	one=mkinteger(NVAR_MAX,1);
	assert(one);
	negativeone=mkinteger(NVAR_MAX,-1);
	assert(negativeone);

	thread=(pthread_t *)malloc(sizeof(pthread_t *)*n_thread);
	assert(thread);

	arg=(struct solve_pckt *)malloc(sizeof(struct solve_pckt)*n_thread);
	assert(arg);


	n=mkinteger(NVAR_MAX,1);
	assert(n);
	jhjr_sll(n,n,n_var);
	d=mkinteger(NVAR_MAX,n_thread);
	assert(d);
	q=mkinteger(NVAR_MAX,0);
	assert(q);
	r=mkinteger(NVAR_MAX,0);
	assert(r);
#if defined(DEBUG)
	printf("q: 0x");
	jhjr_puts(q);
	puts("");
	printf("r: 0x");
	jhjr_puts(r);
	puts("");
	printf("n: 0x");
	jhjr_puts(n);
	puts("");
	printf("d: 0x");
	jhjr_puts(d);
	puts("");
#endif
	jhjr_divmod(q,r,n,d);
#if defined(DEBUG)
	printf("q: 0x");
	jhjr_puts(q);
	puts("");
	printf("r: 0x");
	jhjr_puts(r);
	puts("");
	printf("n: 0x");
	jhjr_puts(n);
	puts("");
	printf("d: 0x");
	jhjr_puts(d);
	puts("");
#endif


	integer_t *		a;
	integer_t *		b;
	integer_t *		c;

	a=mkinteger(NVAR_MAX,0);
	assert(a);
	b=mkinteger(NVAR_MAX,0);
	assert(b);
	c=mkinteger(NVAR_MAX,0);
	assert(c);
	eval0_bits=(integer_t **)malloc(n_clause*sizeof(integer_t *));
	assert(eval0_bits);
	eval0_mask=(integer_t **)malloc(n_clause*sizeof(integer_t *));
	assert(eval0_mask);
	for(i=0;i<n_clause;i++) {
		struct clause_te *	p;

		p=psi+i;

		eval0_bits[i]=mkinteger(NVAR_MAX,0);
		assert(eval0_bits[i]);
		stinteger(a,((p->inv & 1)>>0));
		stinteger(b,((p->inv & 2)>>1));
		stinteger(c,((p->inv & 4)>>2));
		jhjr_sll(a,a,(p->v1)-1);
		jhjr_sll(b,b,(p->v2)-1);
		jhjr_sll(c,c,(p->v3)-1);
		jhjr_or(eval0_bits[i],eval0_bits[i],a);
		jhjr_or(eval0_bits[i],eval0_bits[i],b);
		jhjr_or(eval0_bits[i],eval0_bits[i],c);

		eval0_mask[i]=mkinteger(NVAR_MAX,0);
		assert(eval0_mask[i]);
		stinteger(a,1);
		stinteger(b,1);
		stinteger(c,1);
		jhjr_sll(a,a,(p->v1)-1);
		jhjr_sll(b,b,(p->v2)-1);
		jhjr_sll(c,c,(p->v3)-1);
		jhjr_or(eval0_mask[i],eval0_mask[i],a);
		jhjr_or(eval0_mask[i],eval0_mask[i],b);
		jhjr_or(eval0_mask[i],eval0_mask[i],c);
	}
	rminteger(a);
	rminteger(b);
	rminteger(c);

#if defined(DEBUG)
	for(i=0;i<n_clause;i++) {
		printf("eval0_bits[%3d] -> ",i);
		jhjr_puts(eval0_bits[i]);
		printf("\teval0_mask[%3d] -> ",i);
		jhjr_puts(eval0_mask[i]);
		puts("");
	}
#endif

	pthread_mutex_init(&printf_lock,NULL);
	pthread_mutex_init(&restart_lock,NULL);
	pthread_mutex_init(&solution_lock,NULL);

	for(i=0;i<n_thread;i++) {
		arg[i].status_report=status_report+i;
		if (0==i) {
			arg[i].s_start=mkinteger(NVAR_MAX,0);
			arg[i].s_end=mkinteger(NVAR_MAX,0);
			jhjr_add(arg[i].s_end,arg[i].s_end,q);
			jhjr_add(arg[i].s_end,arg[i].s_end,r);
			jhjr_add(arg[i].s_end,arg[i].s_end,negativeone);
		} else {
			arg[i].s_start=mkinteger(NVAR_MAX,0);
			jhjr_add(arg[i].s_start,arg[i-1].s_end,one);
			arg[i].s_end=mkinteger(NVAR_MAX,0);
			jhjr_add(arg[i].s_end,arg[i-1].s_end,q);
		}
		pthread_create(&(thread[i]),NULL,solve,&(arg[i]));
	}

	for(i=0;i<n_thread;i++)
		pthread_join(thread[i],NULL);

	getrusage(RUSAGE_SELF,&usage[1]);
	printf("main(): getrusage() -> %15ld %15ld\n",
		((1000000*usage[1].ru_utime.tv_sec+usage[1].ru_utime.tv_usec)-
		 (1000000*usage[0].ru_utime.tv_sec+usage[0].ru_utime.tv_usec)),
		((1000000*usage[1].ru_stime.tv_sec+usage[1].ru_stime.tv_usec)-
		 (1000000*usage[0].ru_stime.tv_sec+usage[0].ru_stime.tv_usec)));

	for(i=0;i<n_clause;i++) {
		rminteger(eval0_bits[i]);
		rminteger(eval0_mask[i]);
	}
	free(eval0_bits);
	free(eval0_mask);
	rminteger(one);
	rminteger(negativeone);
	rmstack();
	jhjr_shutdown();

done:
	if (filename) {
		free(filename);
	}

	fflush(NULL);

	return	0;
}

int
parseoptions(int argc,char ** argv,char ** filename,int * n_thread)
{
	int	retval=0;
	int	argn=1;

	assert(NULL==*filename);

	if (argc<3) {
		goto fail;
	}

	*n_thread=1;

	while(argn<argc) {
		int	advance=1;

		switch(argv[argn][0]) {
			case '0' :
			case '1' :
			case '2' :
			case '3' :
			case '4' :
			case '5' :
			case '6' :
			case '7' :
			case '8' :
			case '9' :
				{
					if (argn+1>=argc)
						goto fail;

					n_var=atoi(argv[argn]);
					n_clause=atoi(argv[argn+1]);

					advance=2;
				}
				break;
			case '-' :
				{
					if (argn+1>=argc)
						goto fail;

					if (0==strcmp(argv[argn],"-i")) {
						*filename=strdup(argv[argn+1]);
						retval=IFILENAME;
					} else
					if (0==strcmp(argv[argn],"-o")) {
						*filename=strdup(argv[argn+1]);
						retval=OFILENAME;
					} else
					if (0==strcmp(argv[argn],"-t")) {
						*n_thread=atoi(argv[argn+1]);
					} else {
						goto fail;
					}
					advance=2;
				}
				break;
			default :
				{
					goto fail;
				}
		}
		argn+=advance;
	}

	return retval;
fail:
	fprintf(stderr,
		"usage: %s n_var n_clause [-o filename] [-t n_thread]\n"
		"       %s -i filename [-t n_thread]\n",
		BIN_FILENAME,
		BIN_FILENAME);

	return -1;
}


/* will yield decending-order sort when
 * used in conjuction with qsort()...
 */
int clause_compar(const void * a,const void * b)
{
	struct clause_te * p=(struct clause_te *)a;
	struct clause_te * q=(struct clause_te *)b;

	if (p->v1<q->v1)
		return 1;
	else
	if (p->v1>q->v1)
		return -1;
	else
	if (p->v2<q->v2)
		return 1;
	else
	if (p->v2>q->v2)
		return -1;
	else
	if (p->v3<q->v3)
		return 1;
	else
	if (p->v3>q->v3)
		return -1;
	else
		return 0;
}

int
clause_count(struct clause_te * psi)
{
	int 			i;
	struct clause_te *	p=psi;
	int			count=1;
	unsigned int		v1prev;
	unsigned int		v2prev;
	unsigned int		v3prev;
	struct clause_te *	tmp;


	assert(NULL!=(tmp=(struct clause_te *)malloc(sizeof(struct clause_te)*8)));
	memset((void *)tmp,0,sizeof(struct clause_te)*8);
	v1prev=p->v1;
	v2prev=p->v2;
	v3prev=p->v3;
	tmp[0].v1=p->v1;
	tmp[0].v2=p->v2;
	tmp[0].v3=p->v3;
	for(i=1;i<n_clause;i++) {
		p=psi+i;
		if ((p->v1==v1prev) && (p->v2==v2prev) && (p->v3==v3prev)) {
			tmp[count].v1=p->v1;
			tmp[count].v2=p->v2;
			tmp[count].v3=p->v3;
			tmp[count].inv=p->inv;
			count+=1;
			if (8==count) {
				printf("clause_count(): Perfect logical contradiction...\n");
				dump(tmp,8);
				printf("clause_count(): Boolean expression is trivially unsatisfiable!\n");
				free(tmp);

				return 1;
			}
		} else {
			v1prev=p->v1;
			v2prev=p->v2;
			v3prev=p->v3;
			tmp[0].v1=p->v1;
			tmp[0].v2=p->v2;
			tmp[0].v3=p->v3;
			count=1;
		}
	}
	free(tmp);

	return 0;
}


static inline int
EVAL(integer_t * s,int i,integer_t * eval_t0)
{
	jhjr_and(eval_t0,s,eval0_mask[i]);

	return 1-jhjr_eq(eval_t0,eval0_bits[i]);
}

void *
solve(void * arg)
{
	integer_t *		one;
	integer_t *		negativeone;
	integer_t *		eval_t0;
	struct clause_te *	p;
	integer_t *	 	s;
	integer_t *		s_max;
	int			eval=0;
	int			partition=0;
	integer_t **		mask;
	integer_t **		incr;
	int			k;

#if defined(DEBUG)
	pthread_mutex_lock(&printf_lock);
	printf("tid=%08x s_start: ",(unsigned int)pthread_self());
	jhjr_puts(((struct solve_pckt *)arg)->s_start);
	puts("");
	printf("             s_end:   ");
	jhjr_puts(((struct solve_pckt *)arg)->s_end);
	puts("");
	pthread_mutex_unlock(&printf_lock);
#endif

	*(((struct solve_pckt *)arg)->status_report)=0;

/* initialize useful interger_t variables... */
	one=mkinteger(NVAR_MAX,1);
	negativeone=mkinteger(NVAR_MAX,-1);
	eval_t0=mkinteger(NVAR_MAX,0);
	mkstack(NVAR_MAX,64);

	s=((struct solve_pckt *)arg)->s_start;
	s_max=((struct solve_pckt *)arg)->s_end;

	mask=(integer_t **)malloc(sizeof(integer_t *)*n_var);
	incr=(integer_t **)malloc(sizeof(integer_t *)*n_var);
	for(k=0;k<n_var;k++) {
		mask[k]=mkinteger(NVAR_MAX,0);
		cpinteger(mask[k],negativeone);
		jhjr_sll(mask[k],mask[k],k);
		incr[k]=mkinteger(NVAR_MAX,1);
		jhjr_sll(incr[k],incr[k],k);
	}
restart:
	while (jhjr_lt(s,s_max)) {
		int	i;

		eval=1;
		for(i=0;i<n_clause;i++) {
			p=psi+i;
			eval&=EVAL(s,i,eval_t0);
			if (eval==0)
				break;
		}
		if (eval) {
			pthread_mutex_lock(&solution_lock);
			n_solution+=1;
			pthread_mutex_unlock(&solution_lock);
			pthread_mutex_lock(&printf_lock);
			printf("solve(): [SOLUTION] s = ");
			jhjr_puts(s);
#if defined(DEBUG)
			printf(" @%08x",(unsigned int)pthread_self());
#endif
			printf("\n");
			pthread_mutex_unlock(&printf_lock);
			jhjr_add(s,s,one);
		} else {
			partition+=1;
			if (*(((struct solve_pckt *)arg)->status_report)) {
				*(((struct solve_pckt *)arg)->status_report)=0;
				pthread_mutex_lock(&printf_lock);
				fprintf(stderr,"solve(): [STATUS REPORT] s = ");
				pthread_mutex_unlock(&printf_lock);
				jhjr_fputs(s,stderr);
				fputs("\n",stderr);
			}
			jhjr_and(s,s,mask[(p->v1-1)]);
			jhjr_add(s,s,incr[(p->v1-1)]);
#if defined(DEBUG)
			dump(p,1);
			printf("\tp->v1  = 0x%08x\n",p->v1);
			printf("\tp->v2  = 0x%08x\n",p->v2);
			printf("\tp->v3  = 0x%08x\n",p->v3);
			printf("\tp->inv = 0x%08x\n",p->inv);
			pthread_mutex_lock(&printf_lock);
			printf("solve(): partition = %9d:0x",partition);
			jhjr_puts(s);
			puts("");
			pthread_mutex_unlock(&printf_lock);
#endif
		}
		pthread_mutex_lock(&restart_lock);
		pthread_mutex_unlock(&restart_lock);
	}
	pthread_mutex_lock(&restart_lock);
	if (restart(s,s_max)) {
		pthread_mutex_unlock(&restart_lock);
		goto restart;
	}
	pthread_mutex_unlock(&restart_lock);
	for(k=0;k<n_var;k++) {
		rminteger(mask[k]);
		rminteger(incr[k]);
	}
	free(mask);
	free(incr);

	// Do *NOT* rminteger() 's' or 's_max'!
	// While variables of type integer_t *, they are
	// used as aliases to integer_t * entities passed
	// in via the 'arg' function parameter.  As such,
	// neither was created in this function; hence,
	// neither should be deleted in this function.
	//
	// - JHJr.; 20110508, 00h49

	rminteger(one);
	rminteger(negativeone);
	rminteger(eval_t0);
	rmstack();

	return NULL;
}

int
restart(integer_t * s_start,integer_t * s_end){
	int		i;
	int		t=-1;
	integer_t *	t0;
	integer_t *	t_init;
	integer_t *	t_fini;
	integer_t *	max;
	integer_t *	one;
	int		retval=0;

	t0=mkinteger(NVAR_MAX,0);
	max=mkinteger(NVAR_MAX,0);
	one=mkinteger(NVAR_MAX,1);
	t_init=mkinteger(NVAR_MAX<<1,0);
	t_fini=mkinteger(NVAR_MAX<<1,0);
	for(i=0;i<n_thread;i++) {
		cpinteger(t_init,arg[i].s_start);
		cpinteger(t_fini,arg[i].s_end);
#if defined(DEBUG)
		fprintf(stderr,"arg[%2d] t_init: ",i);
		jhjr_fputs(t_init,stderr);
		fputs("\n",stderr);
		fprintf(stderr,"arg[%2d] t_fini: ",i);
		jhjr_fputs(t_fini,stderr);
		fputs("\n",stderr);
#endif
		if (jhjr_lt(t_init,t_fini)) {
			jhjr_not(t0,arg[i].s_start);
			jhjr_add(t0,t0,one);
			jhjr_add(t0,arg[i].s_end,t0);
			if (jhjr_gt(t0,max)) {
				cpinteger(max,t0);
				t=i;
			}
		}
	}
	stinteger(t0,(1<<16));
	if (jhjr_gt(max,t0)) {
		cpinteger(s_end,arg[t].s_end);
		jhjr_srl(max,max,1);		
		jhjr_add(arg[t].s_end,arg[t].s_start,max);
		jhjr_add(s_start,arg[t].s_end,one);
#if defined(DEBUG)
		fprintf(stderr,"arg[%2d].s_start: ",t);
		jhjr_fputs(arg[t].s_start,stderr);
		fputs("\n",stderr);
		fprintf(stderr,"arg[%2d].s_end: ",t);
		jhjr_fputs(arg[t].s_end,stderr);
		fputs("\n",stderr);
		fprintf(stderr,"s_start: ",t);
		jhjr_fputs(s_start,stderr);
		fputs("\n",stderr);
		fprintf(stderr,"s_end: ",t);
		jhjr_fputs(s_end,stderr);
		fputs("\n",stderr);
#endif
		
		retval=1;
	}
	rminteger(t0);
	rminteger(max);
	rminteger(one);
	rminteger(t_init);
	rminteger(t_fini);

	return retval;
}


void
status(int N)
{
	memset((void *)status_report,1,sizeof(int)*n_thread);
}

/*
 * TODO: Accomodate variable terminal widths.
 * Can't be bothered just now.
 */
void
dump(struct clause_te * psi,int n_clause)
{
	struct clause_te *	p;
	char			tmp[256];
	int			j,i=0;

	if (!n_clause)
		return;

	for(j=0;j<n_clause;j++) {
		p=psi+j;

		memset((void *)tmp,0,256);
		sprintf(tmp+strlen(tmp),"(");
		if (p->inv & 1)
			sprintf(tmp+strlen(tmp),"\\");
		sprintf(tmp+strlen(tmp),"%c",'a'+(((p->v1-1)&0xf000)>>12));
		sprintf(tmp+strlen(tmp),"%c",'a'+(((p->v1-1)&0xf00)>>8));
		sprintf(tmp+strlen(tmp),"%c",'a'+(((p->v1-1)&0xf0)>>4));
		sprintf(tmp+strlen(tmp),"%c +",'a'+(((p->v1-1)&0x0f)>>0));
		sprintf(tmp+strlen(tmp)," ");
		if (p->inv & 2)
			sprintf(tmp+strlen(tmp),"\\");
		sprintf(tmp+strlen(tmp),"%c",'a'+(((p->v2-1)&0xf000)>>12));
		sprintf(tmp+strlen(tmp),"%c",'a'+(((p->v2-1)&0xf00)>>8));
		sprintf(tmp+strlen(tmp),"%c",'a'+(((p->v2-1)&0xf0)>>4));
		sprintf(tmp+strlen(tmp),"%c +",'a'+(((p->v2-1)&0x0f)>>0));
		sprintf(tmp+strlen(tmp)," ");
		if (p->inv & 4)
			sprintf(tmp+strlen(tmp),"\\");
		sprintf(tmp+strlen(tmp),"%c",'a'+(((p->v3-1)&0xf000)>>12));
		sprintf(tmp+strlen(tmp),"%c",'a'+(((p->v3-1)&0xf00)>>8));
		sprintf(tmp+strlen(tmp),"%c",'a'+(((p->v3-1)&0xf0)>>4));
		sprintf(tmp+strlen(tmp),"%c",'a'+(((p->v3-1)&0x0f)>>0));
		sprintf(tmp+strlen(tmp),")");

		if (i==0) {
			printf("\t");
		} else
		if (i==4) {
			printf("\n\t");
			i=0; /* ...b/c only 4 clauses fit/80-byte text line */
		}
		i++;
		printf("%s",tmp);
	}
	printf("\n");
}

void
freeze(struct clause_te * psi,int n_var,int n_clause,FILE * fp)
{
	struct clause_te *	p;
	int			j;

	assert(NULL!=fp);

	if (!n_clause)
		return;

	fprintf(fp,"p cnf %d %d\n",n_var,n_clause);
	for(j=0;j<n_clause;j++) {
		p=psi+j;
		fprintf(fp,"%d %d %d 0\n",
			p->v1*((p->inv & 0x4) ? -1 : 1),
			p->v2*((p->inv & 0x2) ? -1 : 1),
			p->v3*((p->inv & 0x1) ? -1 : 1));
	}
}


long long
choose(int n,int r)
{
	long long	numerator=1;
	long long	denominator=1;
	int	q=((n-r < r) ? (n-r) : r);
	int	i;

	for(i=0;i<q;i++) {
		numerator*=(n-i);
		denominator*=(q-i);
	}

	return (numerator/denominator);
}

long long
ipow(int x,int y)
{
	int	retval=1;
	int	i=0;

	if (!y)
		return retval;
	else
	if (y & 1)
		retval*=x;

	y>>=1;
	while (y) {
		int	j;
		int	ntrm=x;

		i++;
		if (y & 1) {
			for(j=0;j<i;j++)
				ntrm*=ntrm;
		} else {
			ntrm=1;
		}
		y>>=1;
		retval*=ntrm;
	}

	return retval;	
}

size_t
strcnt(const char * s,char c)
{
	size_t	count=0;
	int	i;

	for(i=0;s[i]!=0;i++)
		if (s[i]==c) count++;

	return count;
}
