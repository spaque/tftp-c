/**
 * Definiciones para el cliente y el servidor TFTP.
 * Sergio Paque Martin
 */

#ifndef _DEFS_H_
#define _DEFS_H_

#include 	<stdio.h>
#include 	<sys/types.h>
#include 	<sys/wait.h>
#include	<sys/times.h>
#include	<sys/param.h>	// definicion de HZ
#include	<string.h>
#include	<stdarg.h>
#include	<unistd.h>
#include	<errno.h>
#include	<signal.h>
#include	<sys/stat.h>
#include	<ctype.h>
#include	<stdlib.h>

#define		TICKS	HZ

#define	MAXBUFF		 2048
#define	MAXDATA		  512
#define	MAXFILENAME	  128
#define	MAXHOSTNAME	  128
#define	MAXLINE		  512
#define	MAXTOKEN	  128

#define	MODE_ASCII	0
#define	MODE_BINARY	1	// binary = octet

#define	NCMDS	12

#define TIMEOUT 1
#define MAXRETRIES 5
#define FAILPROB 20

#define MAXARG 8

#define PORT 50005

extern int	lastsend;
extern  FILE *localfp;
extern int	modetype;
extern int	nextblknum;
extern int	port;
extern long totnbytes;
extern int	traceflag;
extern int debugflag;
extern int failflag;

extern char	recvbuff[];
extern char	sendbuff[];
extern int	sendlen;
extern int	op_sent;
extern int	op_recv;

extern int errno;

// TFTP opcodes
#define	OP_RRQ		1	/* Read Request */
#define	OP_WRQ		2	/* Write Request */
#define	OP_DATA		3	/* Data */
#define	OP_ACK		4	/* Acknowledgment */
#define	OP_ERROR	5	/* Error, see error codes below also */

#define	OP_MIN		1	/* minimum opcode value */
#define	OP_MAX		5	/* maximum opcode value */

/*
 * Define the tftp error codes.
 */
#define	ERR_UNDEF	0	/* not defined, see error message */
#define	ERR_NOFILE	1	/* File not found */
#define	ERR_ACCESS	2	/* Access violation */
#define	ERR_NOSPACE	3	/* Disk full or allocation exceeded */
#define	ERR_BADOP	4	/* Illegal tftp operation */
#define	ERR_BADID	5	/* Unknown TID (port#) */
#define	ERR_FILE	6	/* File already exists */
#define	ERR_NOUSER	7	/* No such user */

// Debug macros
#define	DEBUG(fmt)	if (debugflag) { \
					fprintf(stderr, fmt); \
					fputc('\n', stderr); \
					fflush(stderr); \
				} else ;
#define	DEBUG1(fmt, arg1)	if (debugflag) { \
					fprintf(stderr, fmt, arg1); \
					fputc('\n', stderr); \
					fflush(stderr); \
				} else ;

#define	DEBUG2(fmt, arg1, arg2)	if (debugflag) { \
					fprintf(stderr, fmt, arg1, arg2); \
					fputc('\n', stderr); \
					fflush(stderr); \
				} else ;
				
#define	DEBUG3(fmt, arg1, arg2, arg3)	if (debugflag) { \
					fprintf(stderr, fmt, arg1, arg2, arg3); \
					fputc('\n', stderr); \
					fflush(stderr); \
				} else ;

// Trace macros
#define	TRACE(fmt)	if (traceflag) { \
					fprintf(stdout, fmt); \
					fflush(stdout); \
				} else ;

#define	TRACE1(fmt, arg1)	if (traceflag) { \
					fprintf(stdout, fmt, arg1); \
					fflush(stdout); \
				} else ;

#define	TRACE2(fmt, arg1, arg2)	if (traceflag) { \
					fprintf(stdout, fmt, arg1, arg2); \
					fflush(stdout); \
				} else ;
#define	TRACE3(fmt, arg1, arg2, arg3)	if (traceflag) { \
					fprintf(stdout, fmt, arg1, arg2, arg3); \
					fflush(stdout); \
				} else ;
				
/*
 * Macros para cargar y almacenar enteros de 2 bytes.
 * Usadas para en la cabecera TFTP para el opcode, #bloque y #error.
 * Maneja la conversion entre formato de host y de red.
 */
#define ldshort(addr)		( ntohs (*( (u_short *)(addr) ) ) )
#define	stshort(sval,addr)	( *( (u_short *)(addr) ) = htons(sval) )

FILE	*file_open();

#endif
