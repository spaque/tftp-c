/**
 * Funciones comunes del cliente y el servidor tftp.
 * Sergio Paque Martin
 */

#include	"defs.h"

extern int client;

int	recv_ACK(), recv_DATA(), recv_RQERR(), 
	recv_RRQ(), recv_WRQ(), recv_ACK(), recv_DATA();

/// #bytes de datos en el ultimo paquete.
int	lastsend = 0;
/// Puntero al fichero local para leer o escribir.
FILE *localfp = NULL;
int	modetype = MODE_BINARY;
/// Siguiente #bloque para enviar/recibir.
int	nextblknum = 0;
/// #puerto
int	port = 0;
/// Para las estadisticas de get/put.
long totnbytes = 0;
/// Se activa con la opcion -t de la linea de comandos o el comando "trace".
int	traceflag = 0;
/// Se activa con la opcion -d de la linea de comandos.
int debugflag = 0;
/// Se activa con la opcion -f de la linea de comandos.
int failflag = 0;
int retransmit = 0;

/// Ultimo opcode enviado.
int	op_sent = 0;
/// Ultimo opcode recibido.
int	op_recv	= 0;
char recvbuff[MAXBUFF]	= { 0 };
char sendbuff[MAXBUFF]	= { 0 };
/// #bytes en sendbuff
int	sendlen			= 0;
char client_tracebuff[MAXLINE] = { 0 };
char server_tracebuff[MAXLINE] = { 0 };
// se pone a 1 en el manejador de SIGALRM
int	tout_flag;
int tout_retries;

/*
 * Array indexado por el opcode enviado y el recibido.
 * El resultado es la direccion de la funcion que procesa
 * en el cliente el opcode recibido.
 */
int	(*cli_func_ptr [ OP_MAX + 1 ] [ OP_MAX + 1 ] ) () = {
	NULL,		/* [sent = 0]        [recv = 0]			*/
	NULL,		/* [sent = 0]        [recv = OP_RRQ]		*/
	NULL,		/* [sent = 0]        [recv = OP_WRQ]		*/
	NULL,		/* [sent = 0]        [recv = OP_DATA]		*/
	NULL,		/* [sent = 0]        [recv = OP_ACK]		*/
	NULL,		/* [sent = 0]        [recv = OP_ERROR]		*/

	NULL,		/* [sent = OP_RRQ]   [recv = 0]			*/
	NULL,		/* [sent = OP_RRQ]   [recv = OP_RRQ]		*/
	NULL,		/* [sent = OP_RRQ]   [recv = OP_WRQ]		*/
    recv_DATA,	/* [sent = OP_RRQ]   [recv = OP_DATA]		*/
	NULL,		/* [sent = OP_RRQ]   [recv = OP_ACK]		*/
    recv_RQERR,	/* [sent = OP_RRQ]   [recv = OP_ERROR]		*/

	NULL,		/* [sent = OP_WRQ]   [recv = 0]			*/
	NULL,		/* [sent = OP_WRQ]   [recv = OP_RRQ]		*/
	NULL,		/* [sent = OP_WRQ]   [recv = OP_WRQ]		*/
	NULL,		/* [sent = OP_WRQ]   [recv = OP_DATA]		*/
    recv_ACK,	/* [sent = OP_WRQ]   [recv = OP_ACK]		*/
    recv_RQERR,	/* [sent = OP_WRQ]   [recv = OP_ERROR]		*/

	NULL,		/* [sent = OP_DATA]  [recv = 0]			*/
	NULL,		/* [sent = OP_DATA]  [recv = OP_RRQ]		*/
	NULL,		/* [sent = OP_DATA]  [recv = OP_WRQ]		*/
	NULL,		/* [sent = OP_DATA]  [recv = OP_DATA]		*/
    recv_ACK,	/* [sent = OP_DATA]  [recv = OP_ACK]		*/
	NULL,		/* [sent = OP_DATA]  [recv = OP_ERROR]		*/

	NULL,		/* [sent = OP_ACK]   [recv = 0]			*/
	NULL,		/* [sent = OP_ACK]   [recv = OP_RRQ]		*/
	NULL,		/* [sent = OP_ACK]   [recv = OP_WRQ]		*/
    recv_DATA,	/* [sent = OP_ACK]   [recv = OP_DATA]		*/
	NULL,		/* [sent = OP_ACK]   [recv = OP_ACK]		*/
	NULL,		/* [sent = OP_ACK]   [recv = OP_ERROR]		*/

	NULL,		/* [sent = OP_ERROR] [recv = 0]			*/
	NULL,		/* [sent = OP_ERROR] [recv = OP_RRQ]		*/
	NULL,		/* [sent = OP_ERROR] [recv = OP_WRQ]		*/
	NULL,		/* [sent = OP_ERROR] [recv = OP_DATA]		*/
	NULL,		/* [sent = OP_ERROR] [recv = OP_ACK]		*/
	NULL		/* [sent = OP_ERROR] [recv = OP_ERROR]		*/
};
// Igual que el de arriba, pero para el servidor.
int	(*serv_func_ptr [ OP_MAX + 1 ] [ OP_MAX + 1 ] ) () = {
	NULL,		/* [sent = 0]        [recv = 0]			*/
    recv_RRQ,	/* [sent = 0]        [recv = OP_RRQ]		*/
    recv_WRQ,	/* [sent = 0]        [recv = OP_WRQ]		*/
	NULL,		/* [sent = 0]        [recv = OP_DATA]		*/
	NULL,		/* [sent = 0]        [recv = OP_ACK]		*/
	NULL,		/* [sent = 0]        [recv = OP_ERROR]		*/

	NULL,		/* [sent = OP_RRQ]   [recv = 0]			*/
	NULL,		/* [sent = OP_RRQ]   [recv = OP_RRQ]		*/
	NULL,		/* [sent = OP_RRQ]   [recv = OP_WRQ]		*/
	NULL,		/* [sent = OP_RRQ]   [recv = OP_DATA]		*/
	NULL,		/* [sent = OP_RRQ]   [recv = OP_ACK]		*/
	NULL,		/* [sent = OP_RRQ]   [recv = OP_ERROR]		*/

	NULL,		/* [sent = OP_WRQ]   [recv = 0]			*/
	NULL,		/* [sent = OP_WRQ]   [recv = OP_RRQ]		*/
	NULL,		/* [sent = OP_WRQ]   [recv = OP_WRQ]		*/
	NULL,		/* [sent = OP_WRQ]   [recv = OP_DATA]		*/
	NULL,		/* [sent = OP_WRQ]   [recv = OP_ACK]		*/
	NULL,		/* [sent = OP_WRQ]   [recv = OP_ERROR]		*/

	NULL,		/* [sent = OP_DATA]  [recv = 0]			*/
	NULL,		/* [sent = OP_DATA]  [recv = OP_RRQ]		*/
	NULL,		/* [sent = OP_DATA]  [recv = OP_WRQ]		*/
	NULL,		/* [sent = OP_DATA]  [recv = OP_DATA]		*/
    recv_ACK,	/* [sent = OP_DATA]  [recv = OP_ACK]		*/
	NULL,		/* [sent = OP_DATA]  [recv = OP_ERROR]		*/

	NULL,		/* [sent = OP_ACK]   [recv = 0]			*/
	NULL,		/* [sent = OP_ACK]   [recv = OP_RRQ]		*/
	NULL,		/* [sent = OP_ACK]   [recv = OP_WRQ]		*/
    recv_DATA,	/* [sent = OP_ACK]   [recv = OP_DATA]		*/
	NULL,		/* [sent = OP_ACK]   [recv = OP_ACK]		*/
	NULL,		/* [sent = OP_ACK]   [recv = OP_ERROR]		*/

	NULL,		/* [sent = OP_ERROR] [recv = 0]			*/
	NULL,		/* [sent = OP_ERROR] [recv = OP_RRQ]		*/
	NULL,		/* [sent = OP_ERROR] [recv = OP_WRQ]		*/
	NULL,		/* [sent = OP_ERROR] [recv = OP_DATA]		*/
	NULL,		/* [sent = OP_ERROR] [recv = OP_ACK]		*/
	NULL		/* [sent = OP_ERROR] [recv = OP_ERROR]		*/
};

/**
 * Imprime un mensaje de error seguido del mensaje de error del sistema.
 */
my_perror(char *str, ...)
{
	va_list		args;

	if (strcmp(str, "")) {
		va_start(args, str);
		fprintf(stderr, " ERROR: ");
		vfprintf(stderr, str, args);
		va_end(args);
		fputc('\n', stderr);
		if (errno) fprintf(stderr, " %s\n", strerror(errno));
	}
}

/**
 * Error del sistema. Imprime un mensaje y se termina el programa.
 */
err_sys(char *str, ...)
{
	va_list		args;

	va_start(args, str);
	fputc('\n', stderr);
	vfprintf(stderr, str, args);
	va_end(args);
	if (errno) fprintf(stderr, " %s", strerror(errno));
	fputc('\n', stderr);

	exit(1);
}

/**
 * Abre un fichero para lectura o escritura.
 * @param modo para fopen() - "r" o "w"
 * @return Un puntero a FILE, o NULL si hay error.
 */
FILE *
file_open ( char	*fname,
			char	*mode,
			int	initblknum )
{
	FILE *fp;

	if (strcmp(fname, "-") == 0)
		fp = stdout;
	else if ( (fp = fopen(fname, mode)) == NULL)
		return((FILE *) 0);

	// Poner el siguiente #bloque al bloque inicial.
	nextblknum = initblknum;

	DEBUG2("\nfile_open: abierto fichero %s, modo = %s", fname, mode);

	return(fp);
}

/**
 * Cierra un fichero.
 * @param fp Puntero al fichero.
 */
file_close ( FILE *fp )
{
	if (fclose(fp) == EOF)
		err_sys("error al cerrar el fichero");
	DEBUG("\nfile_close: fichero cerrado");
}

/**
 * Lee datos de un fichero local en modo binario.
 * @return Devuelve el numero de bytes leidos.
 */
int
file_read ( FILE 	*fp,
			char 	*ptr,
			int		maxnbytes )
{
	int	count;

	count = read(fileno(fp), ptr, maxnbytes);
	if (count < 0)
		err_sys("error al leer del fichero");
	DEBUG1("\nfile_read: lectura de fichero, %d bytes", count);
	return(count);
}

/**
 * Escibe datos a un fichero local en modo binario.
 */
file_write ( FILE	*fp,
			 char	*ptr,
			 int	nbytes )
{
	if (write(fileno(fp), ptr, nbytes) != nbytes)
		err_sys("error al escribir en el fichero");
	DEBUG1("\nfile_write: escritura de fichero, %d bytes", nbytes);
}

/**
 * Devuelve 1 si se pierde el paquete, 0 en caso contrario.
 */
int
lost_packet() 
{
	int rnd = (int)(100.0 * rand() / (RAND_MAX + 1.0));
	return (rnd <= FAILPROB);
}

int
network_send(int opcode, char *buff, int sendlen)
{
	if (opcode == OP_DATA || opcode == OP_ACK) {
		if (!failflag || !lost_packet()) {
			net_send(buff, sendlen);
			return 0;
		} else {
			usleep(100000);
			TRACE(" (P)");
			return -1;
		}
	} else {
		net_send(buff, sendlen);
	}
}

/**
 * Manejador de SIGALRM
 */
void
func_timeout(int sig)
{
	tout_flag = 1;
}

/**
 * Bucle para procesar los paquetes tftp que llegan.
 * @param opcode para el cliente: RRQ o WRQ - para el servidor: 0
 * @return Devuelve -1 si obtenemos un timeout, 0 en caso contrario.
 */
int
tftp_loop ( int opcode )
{
	int nbytes;
	struct sigaction sa, old_sa;
	
	sa.sa_handler = func_timeout;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGALRM, &sa, &old_sa);

	op_sent = opcode;
	tout_retries = 0;

	for ( ; ; ) {
		tout_flag = 0;
		alarm(TIMEOUT);
		if ( (nbytes = net_recv(recvbuff, MAXBUFF)) < 0) {
			alarm(0);
			if (tout_flag) {
				if (tout_retries >= MAXRETRIES) {
					DEBUG("\nMaximo de reintentos alcanzado");
					return(-1);
				}
				tout_retries++;
			} else
				err_sys("net_recv error");
			TRACE1("%s (P)", server_tracebuff);
			TRACE1("%s timeout (R)", client_tracebuff);
			// Retransmitir el ultimo paquete
			network_send(op_sent, sendbuff, sendlen);
			continue;
		}

		// Paramos el temporizador
		alarm(0);
		tout_flag = 0;

		if (nbytes < 4)
			err_sys("recibido paquete de %d bytes", nbytes);

		op_recv = ldshort(recvbuff);

		if (op_recv < OP_MIN || op_recv > OP_MAX)
			err_sys("recibido opcode no valido: %d", op_recv);

		// Llamamos a la funcion apropiada
		if (client) {
			if (cli_func_ptr[op_sent][op_recv] != NULL) {
				// Ignoramos el opcode del buffer
				if ((*cli_func_ptr[op_sent][op_recv])(recvbuff + 2, nbytes - 2) < 0){
					sigaction(SIGALRM, &old_sa, NULL);
					return(0);
				}
			} else err_sys("paquete recibido no valido");
		} else {
			if (serv_func_ptr[op_sent][op_recv] != NULL) {
				if ((*serv_func_ptr[op_sent][op_recv])(recvbuff + 2, nbytes - 2) < 0){
					sigaction(SIGALRM, &old_sa, NULL);
					return(0);
				}
			} else err_sys("paquete recibido no valido");
		}
	}
}

/**
 * Envia un RRQ o un WRQ al otro sistema.
 */
send_RQ ( int	opcode,
		  char	*fname,
		  int	mode )
{
	int len;
	char *modestr;

	DEBUG2("\nenviando RRQ/WRQ para %s, modo = %d", fname, mode);
	sprintf(client_tracebuff, "\n ----> %s \"%s\" \"octet\"", (opcode == OP_RRQ) ? "RRQ" : "WRQ", fname);
	TRACE2("\n ----> %s \"%s\" \"octet\"", (opcode == OP_RRQ) ? "RRQ" : "WRQ", fname);

	bzero(sendbuff, MAXBUFF);
	stshort(opcode, sendbuff);

	strcpy(sendbuff+2, fname);
	len = 2 + strlen(fname) + 1; // +1 para dejar el \0 despues de fname

	switch(mode) {
		case MODE_BINARY:	modestr = "octet";	break;
		default:			err_sys("modo desconocido");
	}
	strcpy(sendbuff + len, modestr);
	len += strlen(modestr) + 1;	// +1 para dejar el \0 despues de modestr

	sendlen = len;
	net_send(sendbuff, sendlen);
	op_sent = opcode;
}

/**
 * Paquete de error recibido en respuesta a un RRQ o un WRQ.
 * @param ptr Apunta justo despues del opcode recibido.
 * @param nbytes Longitud del paquete (sin incluir opcode).
 */
int
recv_RQERR ( char *ptr, int nbytes )
{
	int ecode;

	ecode = ldshort(ptr);
	ptr += 2;
	nbytes -= 2;
	ptr[nbytes] = 0;	// nos aseguramos que termina en \0

	DEBUG3("\nERROR %d recibido, %d bytes, %s", ecode, nbytes, ptr);
	sprintf(server_tracebuff, "\n\t\t\t\t\t<---- ERROR");
	TRACE("\n\t\t\t\t\t<---- ERROR");

	return(-1);	/* terminate finite state loop */
}

/**
 * Envia un paquete ACK al otro sistema.
 */
int 
send_ACK ( int blocknum )
{
	DEBUG1("\nenviando ACK para el #bloque %d", blocknum);
	sprintf(client_tracebuff, "\n ----> ACK %d", blocknum);
	TRACE1("\n ----> ACK %d", blocknum);

	stshort(OP_ACK, sendbuff);
	stshort(blocknum, sendbuff + 2);

	sendlen = 4;
	op_sent = OP_ACK;
	return network_send(OP_ACK, sendbuff, sendlen);
}

/**
 * Envia datos al otro sistema.
 * Los datos tienen que estar almacenados en sendbuff + 4.
 * @param nbytes #bytes de datos a enviar.
 */
int 
send_DATA ( int blocknum, int nbytes )
{
	DEBUG2("\nenviando %d bytes de datos del #bloque %d", nbytes, blocknum);
	sprintf(client_tracebuff, "\n ----> DATA %d %d bytes", blocknum, nbytes);
	TRACE2("\n ----> DATA %d %d bytes", blocknum, nbytes);

	stshort(OP_DATA, sendbuff);
	stshort(blocknum, sendbuff + 2);

	sendlen = nbytes + 4;
	op_sent = OP_DATA;
	return network_send(OP_DATA, sendbuff, sendlen);
}

/**
 * Recibe un paquete de datos y envia un ACK.
 * @param ptr Apunta justo despues del opcode recibido.
 * @param nbytes Longitud del paquete (sin incluir opcode).
 */
int
recv_DATA ( char *ptr, int nbytes )
{
	int recvblknum;

	recvblknum = ldshort(ptr);
	ptr += 2;
	nbytes -= 2;

	DEBUG2("\n%d bytes del #bloque %d de datos recibidos", nbytes, recvblknum);
	sprintf(server_tracebuff, "\n\t\t\t\t\t<---- DATA %d %d bytes", recvblknum, nbytes);
	TRACE2("\n\t\t\t\t\t<---- DATA %d %d bytes", recvblknum, nbytes);

	if (nbytes > MAXDATA)
		err_sys("Recibido paquete demasiado grande (%d bytes)", nbytes);
	if (recvblknum == nextblknum) {
		// El numero de bloque es el esperado
		nextblknum++;
		totnbytes += nbytes;
		if (nbytes > 0) {
			// Escribir fichero si hay algo que escribir
			file_write(localfp, ptr, nbytes);
		}

		// Si la longitud es menor que MAXDATA este es el
		// ultimo bloque y cerramos el fichero en el servidor.
		if (!client && nbytes < MAXDATA)
			file_close(localfp);
		send_ACK(recvblknum);
		tout_retries = 0;
	} else if (recvblknum == (nextblknum - 1)) {
		// Se perdio el ACK del ultimo paquete -> retransmitimos el ACK.
		if (tout_retries < MAXRETRIES) {
			TRACE(" (R)");
			send_ACK(recvblknum);
			TRACE(" (R)");
			tout_retries++;
		} else {
			return -1;
		}
	} else {
		err_sys("El #bloque recibido (%d) no es el esperado (%d)\n", recvblknum, nextblknum);
	}

	// ultimo bloque?
	if (nbytes == MAXDATA || retransmit)
		return 0;
	else
		return -1;
}

/**
 * Recibe un paquete ACK y envia mas datos.
 * @param ptr Apunta justo despues del opcode recibido.
 * @param nbytes Longitud del paquete (sin incluir opcode).
 */
int
recv_ACK ( char *ptr, int nbytes )
{
	int recvblknum;

	recvblknum = ldshort(ptr);
	if (nbytes != 2)
		err_sys("Recibido paquete ACK con longitud = %d bytes",	nbytes + 2);

	DEBUG1("\nACK recibido, #bloque %d", recvblknum);
	sprintf(server_tracebuff, "\n\t\t\t\t\t<---- ACK %d", recvblknum);
	TRACE1("\n\t\t\t\t\t<---- ACK %d", recvblknum);

	if (recvblknum == nextblknum) {
		// El ACK es el esperado
		if ( (nbytes = file_read(localfp, sendbuff + 4,	MAXDATA)) == 0) {
			if (lastsend < MAXDATA)
				return(-1);
			// sino enviamos un un paquete de datos 0 bytes
		}
		lastsend = nbytes;
		nextblknum++;
		totnbytes += nbytes;
		send_DATA(nextblknum, nbytes);
		tout_retries = 0;

		return(0);
	} else if (recvblknum == (nextblknum - 1)) {
		// Hemos recibido un ACK duplicado
		if (tout_retries < MAXRETRIES) {
			TRACE(" (R)");
			if (send_DATA(nextblknum, lastsend) < 0)
				TRACE(" (R)");
			tout_retries++;
		} else {
			return -1;
		}
		return(0);
	} else {
		err_sys("El ACK recibido (%d) no es el esperado (%d)\n", recvblknum, nextblknum);
	}
}

/**
 * Recibe un paquete RRQ.
 * @param ptr Apunta justo despues del opcode recibido.
 * @param nbytes Longitud del paquete (sin incluir opcode).
 */
int
recv_RRQ ( char *ptr, int nbytes )
{
	char	ackbuff[2];

	// vefificar el paquete RRQ
	recv_xRQ(OP_RRQ, ptr, nbytes);

	lastsend = MAXDATA;
	stshort(0, ackbuff);

	// Hacemos como si hubieramos recibido un ACK
	// para iniciar la transferencia al cliente
	recv_ACK(ackbuff, 2);

	return(0);
}

/**
 * Recibe un paquete WRQ
 * @param ptr Apunta justo despues del opcode recibido.
 * @param nbytes Longitud del paquete (sin incluir opcode).
 */
int
recv_WRQ ( char *ptr, int nbytes )
{
	// verificar el paquete WRQ
	recv_xRQ(OP_WRQ, ptr, nbytes);

	// Enviamos un ACK para que el cliente inicie la transferencia.
	nextblknum = 1;
	send_ACK(0);

	return(0);
}

/**
 * Procesa un paquete RRQ o WRQ recibido.
 * @param opcode OP_RRQ o OP_WRQ
 * @param ptr Apunta justo despues del opcode recibido.
 * @param nbytes Longitud del paquete (sin incluir opcode).
 */
int
recv_xRQ ( int opcode, char *ptr, int nbytes )
{
	int i;
	char *saveptr;
	char filename[MAXFILENAME], 
		 dirname[MAXFILENAME], 
		 mode[MAXFILENAME];
	struct stat	statbuff;
	int ok = 0;

	// Nos aseguramos de que filename y mode terminan en \0
	saveptr = ptr;		// ptr apunta al principio de filename
	for (i = 0; i < nbytes; i++)
		if (*ptr++ == '\0') {
			ok = 1;
			break;
		}
	if (!ok) err_sys("filename no valido");
	ok = 0;

	strcpy(filename, saveptr);
	saveptr = ptr;		// ahora ptr apunta al principio de mode
	for ( ; i < nbytes; i++)
		if (*ptr++ == '\0') {
			ok = 1;
			break;
		}
	if (!ok) err_sys("mode no valido");

	// pasar a minusculas y copiar
	strlccpy(mode, saveptr);

	if (!strcmp(mode, "octet"))	modetype = MODE_BINARY;
	else send_ERROR(ERR_BADOP, "mode no es octect");

	// filename debe contener el camino completo al fichero
	if (filename[0] != '/')
		send_ERROR(ERR_ACCESS, "filename debe contener el camino completo");

	if (opcode == OP_RRQ) {
		// Verificar que el fichero existe y tenemos permiso para leer
		if (stat(filename, &statbuff) < 0)
			send_ERROR(ERR_ACCESS, strerror(errno));
		if ((statbuff.st_mode & S_IRUSR) == 0)
			send_ERROR(ERR_ACCESS, "El fichero no tiene permisos de lectura");
	} else if (opcode == OP_WRQ) {
		// Verificar que el directorio tiene permiso para escritura
		strcpy(dirname, filename);
		*(rindex(dirname, '/') + 1) = '\0';
		if (stat(dirname, &statbuff) < 0)
			send_ERROR(ERR_ACCESS, strerror(errno));
		if ((statbuff.st_mode & S_IWUSR) == 0)
			send_ERROR(ERR_ACCESS, "El directorio no tiene permisos de escritura");

	} else
		err_sys("opcode desconocido");

	localfp = file_open(filename, (opcode == OP_RRQ) ? "r" : "w", 0);
	if (localfp == NULL)
		send_ERROR(ERR_NOFILE, strerror(errno));
}

/**
 * Envia un paquete de error.
 * @param ecode Codigo de error, ERR_xxx
 * @param stirng Informacion adicional (no puede ser NULL)
 */
send_ERROR ( int ecode, char *string )
{
	DEBUG2("\nenviando ERROR, errcode = %d, string = %s", ecode, string);
	sprintf(client_tracebuff, "\n ----> ERROR %d - %s", ecode, string);
	TRACE2("\n ----> ERROR %d - %s", ecode, string);

	stshort(OP_ERROR, sendbuff);
	stshort(ecode, sendbuff + 2);

	strcpy(sendbuff + 4, string);

	sendlen = 4 + strlen(sendbuff + 4) + 1;
	net_send(sendbuff, sendlen);

	net_close();

	exit(0);
}

/**
 * Copia una cadena en minusculas.
 */
strlccpy(char *dest, char *src)
{
	char c;

	while ( (c = *src++) != '\0') {
		if (isupper(c))
			c = tolower(c);
		*dest++ = c;
	}
	*dest = 0;
}
