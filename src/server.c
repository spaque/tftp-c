/**
 * tftp - Trivial File Transfer Program.  Servidor.
 * Sergio Paque Martin
 */

#include	"defs.h"
#include	<sys/socket.h>
#include	<netinet/in.h>
#include	<arpa/inet.h>
#include	<netdb.h>

#define	MAXHOSTNAMELEN	64
#define	MAXBUFF			2048
#define	MAXCLIENTS		20

struct {
	pid_t pid;
	struct sockaddr_in udp_client;
} clients[MAXCLIENTS];

int client = 0;

/// descriptor de fichero para el socket
int		sockfd = -1;
/// Nombre del servidor.
char	openhost[MAXHOSTNAMELEN] = { 0 };

/// Direccion del servidor.
struct sockaddr_in	udp_srv_addr;
/// Direccion local.
struct sockaddr_in	udp_cli_addr;
static int recv_first = 0;
static int recv_nbytes = -1;

struct sigaction sa, old_sa;

/**
 * Manejador de la señal SIGCHLD.
 */
void 
sigchild (int sig)
{
	pid_t pid;
	int i = 0;
	
	while ((i < MAXCLIENTS) && (pid = waitpid(clients[i].pid, NULL, WNOHANG) <= 0)) {
		i++;
	}
	if (i < MAXCLIENTS) {
		clients[i].pid = -1;
		bzero((char *) &clients[i].udp_client, sizeof(clients[i].udp_client));
	}
	DEBUG1("\nFin del proceso %d", pid);
}

/**
 * Manejador de la señal SIGINT.
 */
void 
sigctrlc (int sig)
{
	pid_t pid;
	sigaction(SIGCHLD, &old_sa, NULL);
	pid = wait(NULL);
	if (pid > 0 )
		DEBUG1("\nFin del proceso %d", pid);
	exit(0);
}

/**
 * Inicializa la conexion del servidor.
 */
net_init ()
{
	// Creamos el socket
	if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		err_sys("net_init: no se pudo crear el socket");

	// Configurar la direccion local
	bzero((char *) &udp_srv_addr, sizeof(udp_srv_addr));
	udp_srv_addr.sin_family      = AF_INET;
	udp_srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	udp_srv_addr.sin_port 		 = htons(port);
	// Asociar el socket a la direccion local
	if (bind(sockfd, (struct sockaddr *) &udp_srv_addr,
					sizeof(udp_srv_addr)) < 0)
		err_sys("net_init: error al asociar el socket");
}

/**
 * Espera a que llegue un cliente y crea un nuevo servidor hijo.
 */
int
net_open ()
{
	int	childpid, nbytes, i = 0;

	// Recibir el primer mensaje del cliente.
	recv_first  =  1;	// indicar a net_recv que guarde la direccion
	recv_nbytes = -1;	// indicar a net_recv que lea el mensaje
	nbytes = net_recv(recvbuff, MAXBUFF);

	// Crear un nuevo hijo para atender las peticiones del cliente.
	if ( (childpid = fork()) < 0)
		err_sys("no se pudo crear el hijo");
	else if (childpid > 0) {	// padre
		// Buscamos el primer registro libre
		while((i < MAXCLIENTS) && (clients[i].pid > 0)) { i++; }
		if (i == MAXCLIENTS) {
			DEBUG("net_open: No se admiten mas peticiones al servidor");
			return -1;
		}
		bzero((char *) &clients[i].udp_client, sizeof(clients[i].udp_client));
		clients[i].pid = childpid;		
		bcopy((char *) &udp_cli_addr, (char *) &clients[i].udp_client, sizeof(udp_cli_addr));

		DEBUG1("\ncreado servidor hijo, pid = %d", childpid);
		DEBUG3("\n*********Puerto = %d | Direccion = %s I= %d",
			clients[i].udp_client.sin_port, 
			inet_ntoa(clients[i].udp_client.sin_addr), i);
		return(childpid);
	}

	// Cerrar el socket del padre
	close(sockfd);
	// Por si close lo modifica
	errno = 0;

	// Crear un nuevo socket
	if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		err_sys("net_open: no se pudo crear el socket");
	// Configurar la nueva direccion para el hijo
	bzero((char *) &udp_srv_addr, sizeof(udp_srv_addr));
	udp_srv_addr.sin_family      = AF_INET;
	udp_srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	udp_srv_addr.sin_port        = htons(0);
	if (bind(sockfd, (struct sockaddr *) &udp_srv_addr, sizeof(udp_srv_addr)) < 0)
		err_sys("net_open: error al asociar el socket");

	// Indicar a net_recv que recvbuff ya contiene el mensaje
	recv_nbytes = nbytes;

	return(0);
}

/**
 * Cierra el socket.
 */
net_close()
{
	DEBUG2("\nnet_close: host = %s, fd = %d", openhost, sockfd);

	close(sockfd);

	sockfd = -1;
}

/**
 * Envia datos al cliente.
 */
net_send (	char	*buff,
			int	len )
{
	int	rc = sendto(sockfd, buff, len, 0, 
				(struct sockaddr *) &udp_cli_addr,
				sizeof(udp_cli_addr));
	if (rc != len)
		err_sys("sendto error");

	DEBUG3("\nnet_send: enviados %d bytes al host %s, #puerto %d",
			len, inet_ntoa(udp_cli_addr.sin_addr),
			ntohs(udp_cli_addr.sin_port));
}

/**
 * Recibe datos del cliente.
 * @return Devuelve la longitud del mensaje.
 */
int
net_recv (	char	*buff,
			int	maxlen )
{
	int	nbytes, fromlen, i;
	extern int	tout_flag;
	struct sockaddr_in	from_addr;

	if (recv_nbytes >= 0) {
		// El primer mensaje ya ha sido recibido y su contenido esta en recvbuff
		nbytes = recv_nbytes;
		recv_nbytes = -1;
		return(nbytes);
	}

	while (1) {
		fromlen = sizeof(from_addr);
		nbytes = recvfrom(sockfd, buff, maxlen, 0,
					(struct sockaddr *) &from_addr, &fromlen);
		// El servidor puede verse interrumpido por la alarma o por la señal SIGKCLD
		if (nbytes < 0) {
			if (errno == EINTR) {
				if (tout_flag)	// si hay un timeout devolver -1
					return(-1);
				// asumimos que llega SIGCHLD e iniciamos otro recvfrom()
				errno = 0;
				continue;
			}
			err_sys("error al recibir");
		}
		// Comprobamos que el mismo cliente no nos hace dos veces la misma peticion.
		i = 0;		
		DEBUG2("\n*********Puerto = %d | Direccion = %s",
			clients[i].udp_client.sin_port, 
			inet_ntoa(clients[i].udp_client.sin_addr));
		DEBUG2("\n*********Puerto = %d | Direccion = %s",
			from_addr.sin_port, 
			inet_ntoa(from_addr.sin_addr));
		
		//Vemos si este hijo ya tiene asignada alguna peticion.
		while((i < MAXCLIENTS) &&
				((clients[i].udp_client.sin_addr.s_addr != from_addr.sin_addr.s_addr) ||
				(clients[i].udp_client.sin_port != from_addr.sin_port))) {
			i++;
		}
		if (i < MAXCLIENTS){
				DEBUG1("net_revc: Peticion duplicada del cliente %s, se omite",
					inet_ntoa(from_addr.sin_addr));
			 	continue;
		}
		break;
	}

	DEBUG3("\nnet_recv: recibidos %d bytes desde host %s, #puerto %d",
			nbytes, inet_ntoa(from_addr.sin_addr),
			ntohs(from_addr.sin_port));

	// Si es la primera recepcion guardamos la direccion del cliente
	if (recv_first) {
		bcopy((char *) &from_addr, (char *) &udp_cli_addr,
						sizeof(from_addr));
		recv_first = 0;
	}

	// Verificar que el mensaje proviene del cliente esperado
	if (udp_cli_addr.sin_port != 0 &&
	    udp_cli_addr.sin_port != from_addr.sin_port)
		err_sys("datos recibidos por el puerto %d, esperados por el puerto %d",
		       ntohs(from_addr.sin_port), ntohs(udp_cli_addr.sin_port));

	return(nbytes);
}

main (int argc, char **argv)
{
	int	childpid;
	char *s;
	int i;

	//Inicializamos el array de posiciones
	for (i = 0; i < MAXCLIENTS; i++) {
		clients[i].pid = -1;		
		bzero((char *) &clients[i].udp_client, sizeof(clients[i].udp_client));	
	}

	while (--argc > 0 && (*++argv)[0] == '-')
		for (s = argv[0]+1; *s != '\0'; s++)
			switch (*s) {
				case 'p':	// Especifica el puerto para el servidor
					if (--argc <= 0)
					   fprintf(stderr, "-p requiere otro argumento");
					port = atoi(*++argv);
					break;
				case 'd':
					debugflag = 1;
					break;
				case 't':
					traceflag = 1;
					break;
				case 'f':
					failflag = 1;
					break;
				default:
					fprintf(stderr, "opcion desconocida: %c", *s);
			}

	DEBUG1("\npid = %d", getpid());
	
	sa.sa_handler = sigchild;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
	sigaction(SIGCHLD, &sa, &old_sa);
	signal(SIGINT, sigctrlc);
	
	if (port <= 0) {
		fprintf(stdout, "#puerto especificado no valido, se utilizara #puerto = %d\n", PORT);
		port = PORT;
	}
	srand(time(NULL));
	net_init();

	// Bucle del servidor concurrente.
	for ( ; ; ) {
		if ( (childpid = net_open()) == 0) {
			// El hijo creado por net_open maneja las peticiones del cliente.
			tftp_loop(0);
			net_close();
			exit(0);
		}
		// El padre espera a que llegue otro cliente
	}
}
