/**
 * tftp - Trivial File Transfer Program.  Cliente.
 * Sergio Paque Martin
 */

#include	"defs.h"
#include	<sys/socket.h>
#include	<netinet/in.h>
#include	<arpa/inet.h>
#include	<netdb.h>

#define	MAXHOSTNAMELEN	  64
#define	MAXBUFF		2048

int client = 1;

static char line[MAXLINE] = { 0 };

static	long			time_start, time_stop;
static	struct tms		tms_start, tms_stop;
static	double			start, stop;

char *arguments[MAXARG];
int narg;
/// Name of host system.
char hostname[MAXHOSTNAME] = { 0 };
/// 1 si estamos conectados.
/// true if we're connected to host.
int	connected = 0;
/// Cadena para el prompt.
char *prompt = "tftp> ";

extern int h_errno;

/// descriptor de fichero para el socket
int		sockfd = -1;
/// Nombre del servidor.
char	openhost[MAXHOSTNAMELEN] = { 0 };

/// Direccion del servidor.
struct sockaddr_in	udp_srv_addr;
/// Direccion local
struct sockaddr_in	udp_cli_addr;
static int recv_first;

/**
 * Crea un socket udp y configura la direccion del servidor.
 */
int
net_open ()
{
	struct hostent	*hp;
	struct in_addr in;

	// Inicializar la direccion del servidor
	bzero((char *) &udp_srv_addr, sizeof(udp_srv_addr));
	udp_srv_addr.sin_family = AF_INET;

	if (port <= 0) {
		my_perror("debe especificar un puerto valido");
		return(-1);
	}
	udp_srv_addr.sin_port = htons(port);

	// Obtener la direccion del servidor.
	if (inet_aton(hostname, &in)) {
		bcopy(&in.s_addr, &udp_srv_addr.sin_addr, sizeof(in));
	} else {
		if ( (hp = gethostbyname(hostname)) == NULL) {
			my_perror("error al obtener la direccion del servidor: %s %s",
						hostname, hstrerror(h_errno));
			return(-1);
		}
		bcopy(hp->h_addr, (char *) &udp_srv_addr.sin_addr,
			hp->h_length);
	}

	if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		DEBUG("\nno se puede crear un socket UDP");
		return(-1);
	}

	// Configuramos la direccion local
	bzero((char *) &udp_cli_addr, sizeof(udp_cli_addr));
	udp_cli_addr.sin_family      = AF_INET;
	udp_cli_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	udp_cli_addr.sin_port        = htons(0);
	// Asociamos el socket con la direccion local
	if (bind(sockfd, (struct sockaddr *) &udp_cli_addr,
						sizeof(udp_cli_addr)) < 0) {
		DEBUG("\nnet_open: error al asociar el socket");
		close(sockfd);
		return(-1);
	}

	DEBUG2("\nnet_open: host %s, port# %d",
			inet_ntoa(udp_srv_addr.sin_addr),
			ntohs(udp_srv_addr.sin_port));

	strcpy(openhost, hostname);
	recv_first = 1;

	return(0);
}

/**
 * Cierra la conexion.
 */
net_close ()
{
	DEBUG2("\nnet_close: host = %s, fd = %d", openhost, sockfd);

	close(sockfd);

	sockfd = -1;
}

/**
 * Envia datos al servidor.
 */
net_send(	char	*buff,
			int		len )
{
	if (sendto(sockfd, buff, len, 0, (struct sockaddr *) &udp_srv_addr,
					sizeof(udp_srv_addr)) < 0)
		err_sys("send error");

	DEBUG3("\nnet_send: enviados %d bytes al host %s, #puerto %d",
			len, inet_ntoa(udp_srv_addr.sin_addr),
			ntohs(udp_srv_addr.sin_port));
}

/**
 * Recibe datos del servidor.
 * @return #bytes recibidos, o -1 si nos interrumpio una señal.
 */
int
net_recv (	char	*buff,
			int		maxlen )
{
	int	nbytes;
	int	fromlen;
	struct sockaddr_in	from_addr;	// direccion del remitente

	fromlen = sizeof(from_addr);
	nbytes = recvfrom(sockfd, buff, maxlen, 0,
			  	(struct sockaddr *) &from_addr, &fromlen);
	// Si nos interrumpe una señal devolvemos -1
	if (nbytes < 0) {
		if (errno == EINTR)
			return(-1);
		else
			err_sys("net_recv: error al recibir");
	}

	DEBUG3("\nnet_recv: recibidos %d bytes del host %s, #puerto %d",
		      nbytes, inet_ntoa(from_addr.sin_addr),
		      ntohs(from_addr.sin_port));

	/*
	 * El primer paquete hay que enviarlo a un puerto conocido anteriormente.
	 * Los siguientes paquetes se enviaran por el puerto que nos diga el servidor.
	 */
	if (recv_first) {
		if (udp_srv_addr.sin_port == from_addr.sin_port)
			err_sys("primer paquete por el puerto %d", ntohs(from_addr.sin_port));
		// Guardamos el nuevo #puerto
		udp_srv_addr.sin_port = from_addr.sin_port;
		recv_first = 0;
	} else if (udp_srv_addr.sin_port != from_addr.sin_port) {
		DEBUG2("\nse recibieron datos por el puerto %d, pero se esperaban por el puerto %d",
				ntohs(from_addr.sin_port),
				ntohs(udp_srv_addr.sin_port));
		return -1;
	}

	return(nbytes);
}

/**
 * connect <host>  [<puerto>]
 */
cmd_connect()
{
	int	val;

	if (narg < 2 || narg > 3) {
		fprintf(stdout, " uso: connect <host> [<puerto>]\n");
		return;
	}
	strncpy(hostname, arguments[1], MAXHOSTNAME);

	if (narg == 3) {
		val = atoi(arguments[2]);
		if (val <= 0) {
			my_perror("#puerto invalido");
		} else {
			port = val;
			connected = 1;
		}
	} else {
		port = PORT;
		connected = 1;
	}
}

cmd_fail()
{
	failflag = !failflag;
	printf(" fallos = %s\n", failflag ? "on" : "off");
}

/**
 * get <FicheroRemoto> <FicheroLocal>
 */
cmd_get()
{
	char remfname[MAXFILENAME], locfname[MAXFILENAME];

	if (narg != 3) {
		fprintf(stdout, " uso: get <FicheroRemoto> <FicheroLocal>\n");
		fprintf(stdout, "      (especificar camino desde la raiz)\n");
		return;
	}
	strncpy(remfname, arguments[1], MAXFILENAME);
	strncpy(locfname, arguments[2], MAXFILENAME);

	if (!connected) {
		my_perror("no esta conectado");
	} else {
		do_get(remfname, locfname);
	}
}

/**
 * help
 */
cmd_help()
{
	printf("\n  ----------------\n");
	printf("    Cliente TFTP\n");
	printf("  ----------------\n");
	printf("    ?			esta ayuda\n");
	printf("    connect		configura la direccion del servidor\n");
	printf("    fail		modo de funcionamiento con perdida de paquetes\n");
	printf("    get			obtiene un fichero del servidor\n");
	printf("    help		esta ayuda\n");
	printf("    put			manda un fichero al servidor\n");
	printf("    quit		salir\n");
	printf("    status		muestra el estado del cliente\n");
	printf("    trace		muestra la traza de transferencia al transmitir\n\n");
}

/**
 * put <FicheroLocal> <FicheroRemoto>
 */
cmd_put()
{
	char	remfname[MAXFILENAME], locfname[MAXFILENAME];

	if (narg != 3) {
		fprintf(stdout, " uso: put <FicheroLocal> <FicheroRemoto>\n");
		fprintf(stdout, "      (especificar camino desde la raiz)\n");
		return;
	}
	strncpy(locfname, arguments[1], MAXFILENAME);
	strncpy(remfname, arguments[2], MAXFILENAME);

	if (!connected) {
		my_perror("no esta conectado");
	} else {
		do_put(remfname, locfname);
	}
}

/**
 * Muestra el estado actual.
 */
cmd_status()
{
	if (connected)
		printf("Conectado\n");
	else
		printf("No conectado\n");

	printf("modo = ");
	switch (modetype) {
		case MODE_BINARY:	printf("octet (binary)");	break;
		default:			err_sys("modo desconocido");
	}
	printf(", traza = %s", traceflag ? "on" : "off");
	printf(", fallos = %s\n", failflag ? "on" : "off");
}

cmd_trace()
{
	traceflag = !traceflag;
	printf(" traza = %s\n", traceflag ? "on" : "off");
}

/**
 * Inicia el temporizador.
 */
void
t_start()
{
	if ( (time_start = times(&tms_start)) == -1)
		err_sys("t_start: times() error");
}

/**
 * Detiene el temporizador.
 */
void
t_stop()
{
	if ( (time_stop = times(&tms_stop)) == -1)
		err_sys("t_stop: times() error");
}

/**
 * Devuelve el tiempo transcurrido en segundos.
 */
double
t_getrtime()
{
	return ((double) (time_stop - time_start) / (double) TICKS);
}

/**
 * Ejecuta un comando get.
 */
do_get ( char *remfname, char *locfname )
{
	if ( (localfp = file_open(locfname, "w", 1)) == NULL) {
		my_perror("no se puede abrir %s para escritura", locfname);
		return;
	}

	if (net_open() < 0)
		return;

	totnbytes = 0;
	
	t_start();	// Iniciar el temporizador para estadisticas
	send_RQ(OP_RRQ, remfname, modetype);
	tftp_loop(OP_RRQ);
	t_stop();	// Detener el temporizador

	net_close();

	file_close(localfp);

	printf("\n\n P: paquete perdido; R: Paquete retransmitido");
	printf("\n\n\t%ld bytes recibidos en %.1f segundos\n", totnbytes, t_getrtime());
}

/**
 * Ejecuta un comando put.
 */
do_put ( char *remfname, char *locfname )
{
	if ( (localfp = file_open(locfname, "r", 0)) == NULL) {
		my_perror("no se puede abrir %s para lectura", locfname);
		return;
	}

	if (net_open() < 0)
		return;

	totnbytes = 0;
	
	t_start();	// Iniciar el temporizador para estadisticas
	lastsend = MAXDATA;
	send_RQ(OP_WRQ, remfname, modetype);
	tftp_loop(OP_WRQ);
	t_stop();	// Detiene el temporizador

	net_close();

	file_close(localfp);

	printf("\n\n P: paquete perdido; R: Paquete retransmitido");
	printf("\n\n\t%ld bytes enviados en %.1f segundos\n", totnbytes, t_getrtime());
}

/**
 * Ejecuta un comando.
 */
docmd ()
{
	if (!strcmp("?", arguments[0])) 			cmd_help();
	else if (!strcmp("connect", arguments[0]))	cmd_connect();
	else if (!strcmp("fail", arguments[0]))		cmd_fail();
	else if (!strcmp("get", arguments[0]))		cmd_get();
	else if (!strcmp("help", arguments[0]))		cmd_help();
	else if (!strcmp("put", arguments[0]))		cmd_put();
	else if (!strcmp("quit", arguments[0]))		exit(0);
	else if (!strcmp("status", arguments[0]))	cmd_status();
	else if (!strcmp("trace", arguments[0]))	cmd_trace();
	else my_perror("El comando %s no esta implementado", arguments[0]);
}

parseline()
{
	char *token;
	narg = 0;
	// quitar el \n de la entrada
	line[strlen(line)-1] = '\0';

	// busca una palabra separada por espacios
	if( (token=strtok(line," ")) != NULL ) {
		arguments[narg]=(char *)strdup(token);	// copia la palabra 
		narg++;
		// busca mas palabras en la misma linea
		while (narg < MAXARG && ((token=strtok(NULL," "))!=NULL)) {
			arguments[narg]=(char *)strdup(token);
			narg++;
		} 
	}
	arguments[narg]=NULL;
}

freeargs()
{
	int i;

	for ( i = 0; i < narg; i++ ) {
		if ( arguments[i] != NULL ) {
			free(arguments[i]);
			arguments[i] = NULL;
		}
	}
	narg = 0;
}

main ( int argc, char **argv )
{
	int	i;
	char	*s;
	FILE	*fp;

	while (--argc > 0 && (*++argv)[0] == '-')
		for (s = argv[0]+1; *s != '\0'; s++)
			switch (*s) {
			case 't':
				traceflag = 1;
				break;
			case 'd':
				debugflag = 1;
				break;
			case 'f':
				failflag = 1;
				break;
			default:
				fprintf(stderr, "unknown command line option: %c", *s);
			}

	srand(time(NULL));
	signal(SIGINT, SIG_IGN);
	printf("\n  ----------------\n");
	printf("    Cliente TFTP\n");
	printf("  ----------------\n");
	printf(" Introduzca 'help' o '?' para ver una lista de comandos\n\n");
	
	printf("%s", prompt);
	// Bucle principal. Lee un comando y lo ejecuta.
	while (fgets(line, MAXLINE, stdin)) {
		parseline();
		if (narg > 0) {
			docmd();
			freeargs();
		}
		printf("%s", prompt);
	}

	exit(0);
}
