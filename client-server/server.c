#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_CLIENTS	5
#define BUFLEN 256

enum boolean {false,true};
enum errors {NAC = -1,
			SAO = -2,
			WPIN = -3,
			CNF = -4,
			CBK = -5,
			OPF = -6,
			UBF = -7,
			IFU = -8,
			OPC = -9,
			FERR = -10};

typedef struct user {
	char nume[13];
	char prenume[13];
	int numar_card;
	int pin;
	char parola_secreta[9];
	double sold;
	int blocat;
	int autentificat;
	int cardunlock;
} user;

typedef struct totransfer {
	struct user usertotransfer;
	double value;
} totransfer;

typedef struct opencli {
	int sock;
	char last_command[BUFLEN];
	int contor;
	struct totransfer transfer;
	struct user utilizator;
} opencli;

opencli clients[MAX_CLIENTS];

static const user emptyuser;
static const opencli emptyclient;
static const totransfer emptytransfer;

char buffer[BUFLEN], reply[BUFLEN];

int exitflag;

void error(int code, char *msg) {
	switch (code) {
		case NAC:
			printf(" -1 : The client is not authenticated\n");
			break;
		case SAO:
			printf("-2 : Session already opened\n");
			break;
		case WPIN:
			printf(" -3 : Wrong PIN\n");
			break;
		case CNF:
			printf(" -4 : Card number doesn't exist\n");
			break;
		case CBK:
			printf(" -5 : Card number blocked\n");
			break;
		case OPF:
			printf(" -6 : Operation failed\n");
			break;
		case UBF:
			printf(" -7: Unlocking failed\n");
			break;
		case IFU:
			printf(" -8 Insuficient funds\n");
			break;
		case OPC:
			printf(" -9 : Operation canceled\n");
			break;
		case FERR:
			printf(" -10: Function error: %s\n",msg);
			exit(1);
			break;
		default: break;
	}
}

void updatecli (int sock, user u, char* last_command, int contor, char* msg) {
	if (strcmp(msg,"clear") == 0) {
		for (int i = 0; i < MAX_CLIENTS; ++i) {
				clients[i].sock = -1;
				clients[i].utilizator = emptyuser;
				clients[i].contor = 0;
				memset(clients[i].last_command, 0, BUFLEN);
				clients[i].transfer = emptytransfer;
		}
	}
	if (strcmp(msg,"delete") == 0) {
		for (int i = 0; i < MAX_CLIENTS; ++i) {
			if (clients[i].sock == sock) {
				clients[i].sock = -1;
				clients[i].utilizator = emptyuser;
				clients[i].contor = 0;
				memset(clients[i].last_command, 0, BUFLEN);
				clients[i].transfer = emptytransfer;
				break;
			}
		}
	}
	if (strcmp(msg,"initialize") == 0) {
		for (int i = 0; i < MAX_CLIENTS; ++i) {
			if (clients[i].sock == -1) {
				clients[i].sock = sock;
				clients[i].utilizator = emptyuser;
				clients[i].contor = 0;
				memset(clients[i].last_command, 0, BUFLEN);
				clients[i].transfer = emptytransfer;
				break;
			}
		}
	}
}

void login (int sock, user *u, int lenu) {

	char *token;

	char cpy[BUFLEN];
    strcpy(cpy,reply);

    token = strtok (cpy, " \n");

	token = strtok (NULL, " \n");
	if (token == NULL) {
		strcpy(buffer,"login command incomplete\n");
		return;
	}

	int nrcard = atoi(token);

	token = strtok (NULL, " \n");
	if (token == NULL) {
		strcpy(buffer,"login command incomplete\n");
		return;
	}

	int pin = atoi(token);

	int indexu = -1;
	int indexc = -1;

	for(int i = 0; i < lenu; ++i)
		if (u[i].numar_card == nrcard) {
			indexu = i;
			break;
		}
	for(int i = 0; i < MAX_CLIENTS; ++i)
		if (clients[i].sock == sock) {
			indexc = i;
			break;
		}

	memset(clients[indexc].last_command, 0, BUFLEN);
	strcpy(clients[indexc].last_command,reply);
	clients[indexc].transfer = emptytransfer;

	if (indexu == -1) {
		strcpy(buffer,"IBANK> -4 : Card number doesn't exist\n");
		clients[indexc].contor = 0;
		clients[indexc].utilizator = emptyuser;
		return;
	}

	int flag = false;

	if (u[indexu].blocat == true) {
		strcpy(buffer,"IBANK> -5 : Card number blocked\n");
		clients[indexc].utilizator = emptyuser;
		return;
	}

	if (u[indexu].autentificat == true) {
		strcpy(buffer,"IBANK> -2 : Session already opened\n");
		clients[indexc].contor = 0;
		clients[indexc].utilizator = emptyuser;
		return;
	}

	if (pin != u[indexu].pin) {

		if (strcmp(clients[indexc].last_command, reply) == 0 || strcmp(clients[indexc].last_command, "") == 0) {
			clients[indexc].contor++;
			if (clients[indexc].contor == 3) {
				strcpy(buffer,"IBANK> -5 : Card number blocked\n");
				u[indexu].blocat = true;
			}
			else
				strcpy(buffer,"IBANK> -3 : Wrong PIN\n");
		} else {
			clients[indexc].contor = 1;
			strcpy(buffer,"IBANK> -3 : Wrong PIN\n");
		}
		clients[indexc].utilizator = emptyuser;
		return;
	}

	clients[indexc].contor = 0;
	u[indexu].autentificat = true;
	clients[indexc].utilizator = u[indexu];
	sprintf(buffer, "IBANK> Welcome %s %s\n", clients[indexc].utilizator.nume, clients[indexc].utilizator.prenume);
}

void logout (int sock, user *u, int lenu) {

	int indexu = -1;
	int indexc = -1;

	for(int i = 0; i < MAX_CLIENTS; ++i)
		if (clients[i].sock == sock) {
			indexc = i;
			break;
		}

	for(int i = 0; i < lenu; ++i)
		if (u[i].numar_card == clients[indexc].utilizator.numar_card) {
			indexu = i;
			break;
		}

	clients[indexc].contor = 0;
	memset(clients[indexc].last_command, 0, BUFLEN);
	strcpy(clients[indexc].last_command,reply);
	clients[indexc].transfer = emptytransfer;
	clients[indexc].utilizator = emptyuser;
	u[indexu].autentificat = false;

	strcat(buffer,"IBANK> The client is not authenticated\n");
}

void listsold (int sock, user *u, int lenu) {

	int indexu = -1;
	int indexc = -1;

	for(int i = 0; i < MAX_CLIENTS; ++i)
		if (clients[i].sock == sock) {
			indexc = i;
			break;
		}

	for(int i = 0; i < lenu; ++i)
		if (u[i].numar_card == clients[indexc].utilizator.numar_card) {
			indexu = i;
			break;
		}

	memset(clients[indexc].last_command, 0, BUFLEN);
	strcpy(clients[indexc].last_command,reply);
	clients[indexc].utilizator = u[indexu];

	sprintf(buffer, "IBANK> %.2f\n", clients[indexc].utilizator.sold);
}

void transfer (int sock, user *u, int lenu) {

	char *token;

	char cpy[BUFLEN];
    strcpy(cpy,reply);

    token = strtok (cpy, " \n");

	token = strtok (NULL, " \n");
	if (token == NULL) {
		strcpy(buffer,"Transfer command incomplete\n");
		return;
	}

	int nrcard = atoi(token);

	token = strtok (NULL, " \n");
	if (token == NULL) {
		strcpy(buffer,"Transfer command incomplete\n");
		return;
	}

	double val = atof(token);

	int indexu = -1;
	int indexc = -1;

	for(int i = 0; i < lenu; ++i)
		if (u[i].numar_card == nrcard) {
			indexu = i;
			break;
		}
	for(int i = 0; i < MAX_CLIENTS; ++i)
		if (clients[i].sock == sock) {
			indexc = i;
			break;
		}

	memset(clients[indexc].last_command, 0, BUFLEN);
	strcpy(clients[indexc].last_command,reply);

	if (indexu == -1) {
		strcpy(buffer,"IBANK> -4 : Card number doesn't exist\n");
		return;
	}

	if (clients[indexc].utilizator.sold < val) {
		strcpy(buffer,"IBANK> -8 : Insuficient funds\n");
		clients[indexc].utilizator = emptyuser;
		return;
	}

	clients[indexc].transfer.usertotransfer = u[indexu];
	clients[indexc].transfer.value = val;
	sprintf(buffer, "IBANK> Transfer %.2f to %s %s ? [y /n]\n", val, u[indexu].nume, u[indexu].prenume);
}

void administrationtransfer (int sock, user *u, int lenu) {

	int indexc = -1;
	int indexu = -1;

	for(int i = 0; i < MAX_CLIENTS; ++i)
		if (clients[i].sock == sock) {
			indexc = i;
			break;
		}
	for(int i = 0; i < lenu; ++i)
		if (u[i].numar_card == clients[indexc].transfer.usertotransfer.numar_card) {
			indexu = i;
			break;
		}


	if (reply[0] != 'y') {
		strcpy(buffer,"IBANK> -9 : Operation canceled\n");
		clients[indexc].transfer = emptytransfer;
	} else {
		u[indexu].sold += clients[indexc].transfer.value;

		indexu = -1;
		for(int i = 0; i < lenu; ++i)
			if (u[i].numar_card == clients[indexc].utilizator.numar_card) {
				indexu = i;
				break;
			}

		u[indexu].sold -= clients[indexc].transfer.value;
		clients[indexc].transfer = emptytransfer;

		strcpy(buffer,"IBANK> Transfer made\n");
	}
}

void unclock (user *u, int lenu) {

	char *token;

	char cpy[BUFLEN];
    strcpy(cpy,reply);

    token = strtok (cpy, " \n");

	token = strtok (NULL, " \n");
	if (token == NULL) {
		strcpy(buffer,"Unlock command incomplete\n");
		return;
	}

	int nrcard = atoi(token);

	int indexu = -1;

	for(int i = 0; i < lenu; ++i)
		if (u[i].numar_card == nrcard) {
			indexu = i;
			break;
		}

	if (indexu == -1) {
		strcpy(buffer,"UNLOCK> -4 : Card number doesn't exist\n");
		return;
	}

	if (u[indexu].blocat == false) {
		strcpy(buffer,"UNLOCK> −6 : Operation failed\n");
		return;
	}

	for(int i = 0; i < lenu; ++i)
		if (u[i].cardunlock == nrcard) {
			strcpy(buffer,"UNLOCK> −7 : Unlocking failed\n");
			return;
		}

	u[indexu].cardunlock = nrcard;
	sprintf(buffer, "UNLOCK> Send secret password\n");
}

void administrationunlock (user *u, int lenu) {

	char *token;

	char cpy[BUFLEN];
    strcpy(cpy,reply);

    token = strtok (cpy, " \n");
	if (token == NULL) {
		strcpy(buffer,"Unlocking command incomplete\n");
		return;
	}

	int nrcard = atoi(token);

	token = strtok (NULL, " \n");
	if (token == NULL) {
		strcpy(buffer,"Unlocking command incomplete\n");
		return;
	}

	char parola[9];
	strcpy(parola,token);

	int indexu = -1;

	for(int i = 0; i < lenu; ++i)
		if (u[i].numar_card == nrcard) {
			indexu = i;
			break;
		}

	if (indexu == -1) {
		strcpy(buffer,"UNLOCK> -4 : Card number doesn't exist\n");
		return;
	}

	if (strcmp(u[indexu].parola_secreta,parola) != 0) {
		u[indexu].cardunlock = 0;
		strcpy(buffer,"UNLOCK> −7 : Unlocking failed\n");
		return;
	}

	u[indexu].cardunlock = 0;
	u[indexu].blocat  = false;
	sprintf(buffer, "UNLOCK> Card unblocked\n");
}

void mastercommands (int sock, user *u, int lenu) {

	char cpy[BUFLEN];
    strcpy(cpy,reply);

    char *option;
    option = strtok (cpy, " \n");

	memset(buffer, 0, BUFLEN);

	int indexc = -1;

	for(int i = 0; i < MAX_CLIENTS; ++i)
		if (clients[i].sock == sock) {
			indexc = i;
			break;
		}

    if (strcmp(option,"login") == 0) {
		login(sock, u, lenu);
		return;
	}

	if (strcmp(option,"logout") == 0) {
		logout(sock, u, lenu);
		return;
	}

	if (strcmp(option,"listsold") == 0) {
		listsold(sock, u, lenu);
		return;
	}

	if (strcmp(option,"transfer") == 0) {
		transfer(sock, u, lenu);
		return;
	}

	if (strcmp(option,"unlock") == 0) {
		unclock(u, lenu);
		return;
	}

	if (clients[indexc].transfer.value != 0)
		administrationtransfer(sock, u, lenu);

	if (isdigit(reply[0]))
		administrationunlock(u, lenu);
}

void readinfo (FILE* database, user* u, int len) {
	char buffer[BUFLEN];
	int contor = 0;

	while (fgets(buffer, BUFLEN, database) != NULL) {
		char *token;
    	char delimiters[2] = " ";

    	token = strtok (buffer, delimiters);
		if (token == NULL) error(-10,"strtok()");
		strcpy(u[contor].nume, token);

		token = strtok (NULL, delimiters);
		if (token == NULL) error(-10,"strtok()");
		strcpy(u[contor].prenume, token);

		token = strtok (NULL, delimiters);
		if (token == NULL) error(-10,"strtok()");
		u[contor].numar_card = atoi(token);

		token = strtok (NULL, delimiters);
		if (token == NULL) error(-10,"strtok()");
		u[contor].pin = atoi(token);;

		token = strtok (NULL, delimiters);
		if (token == NULL) error(-10,"strtok()");
		strcpy(u[contor].parola_secreta, token);

		token = strtok (NULL, delimiters);
		if (token == NULL) error(-10,"strtok()");
		u[contor].sold = atof(token);

		u[contor].blocat = false;
		u[contor].autentificat = false;
		contor++;
	}

	if (contor != len) error(-10,"fgets()");
}

int main(int argc, char *argv[]) {

	FILE *users_data_file;
	int tcplistener, udpsock, newsockcon, portno, clilen;

	struct sockaddr_in serv_addr, cli_addr;

	fd_set read_fds, tmp_fds;
	int fdmax;
	int result;

	if (argc < 3) {
		fprintf(stderr, "Usage server: %s <port_server> <users_data_file>\n", argv[0]);
		exit(1);
	}

	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	if ( (tcplistener = socket(AF_INET, SOCK_STREAM, 0)) < 0) error(-10,"socket()");

	if ( (udpsock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) error(-10,"socket()");


	if ( (users_data_file = fopen (argv[2], "r")) == NULL)
		error(-10,"fopen()");

	memset(buffer, 0, BUFLEN);
	if (fgets(buffer, BUFLEN, users_data_file) == NULL) error(-10,"fgets()");

	int max_users = atoi(buffer);

	user utilizatori[max_users];
	readinfo (users_data_file, utilizatori, max_users);

	if (fclose(users_data_file) == EOF) error(-10,"fclose()");

	portno = atoi(argv[1]);

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);

	if (bind(tcplistener, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0)
			error(-10,"bind()");

	if (bind(udpsock, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0)
			error(-10,"bind()");

	listen(tcplistener, MAX_CLIENTS);

	FD_SET(0, &read_fds);
	FD_SET(tcplistener, &read_fds);
	fdmax = tcplistener;

	FD_SET(udpsock, &read_fds);
	if (fdmax < udpsock) fdmax = udpsock;

	updatecli(0,emptyuser,"",0,"clear");

	while (true) {
		tmp_fds = read_fds;

		if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1)
			error(-10,"select()");

		for(int sock = 0; sock <= fdmax; ++sock) {

			if (FD_ISSET(sock, &tmp_fds)) {
				if (sock == 0) {
					memset(buffer, 0 , BUFLEN);
            		fgets(buffer, BUFLEN - 1, stdin);

					if (strcmp(buffer,"quit\n")  == 0) {
						memset(buffer, 0 , BUFLEN);
						sprintf(buffer, "IBANK> The server will go offline\n");

						for(int s = 1; s <= fdmax; ++s)
							if (FD_ISSET(s, &read_fds)) {
								if (s != tcplistener && s != udpsock) {
									result = send(s, buffer, BUFLEN, 0);
                    				if (result < 0) error(-10,"send()");
								}
							}
						exitflag = true;
					}
				} else {
					if (sock == tcplistener) {

						clilen = sizeof(cli_addr);

						if ((newsockcon = accept(tcplistener, (struct sockaddr *)&cli_addr, &clilen)) == -1) {
							error(-10,"accept()");
						} else {
							FD_SET(newsockcon, &read_fds);
							if (newsockcon > fdmax) fdmax = newsockcon;
						}
						updatecli(newsockcon,emptyuser,"",0,"initialize");

						printf("New connection from [IP -> %s]   [PORT -> %d]   [SOCKET -> %d]\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), newsockcon);
					} else {

						if (sock == udpsock) {
							memset(reply, 0 , BUFLEN);
            				int size = sizeof(struct sockaddr);
							result = recvfrom(udpsock, reply, BUFLEN, 0, (struct sockaddr*) &serv_addr, &size);

            				if (result < 0) error(-10,"recvfrom()");

							printf ("(UDP) Client sent message: %s", reply);
							mastercommands(sock,utilizatori, max_users);

							result = sendto(udpsock, buffer, BUFLEN, 0, (struct sockaddr*) &serv_addr, sizeof(struct sockaddr));
                			if (result < 0) error(-10,"sendto()");

						} else {
							memset(reply, 0, BUFLEN);
							result = recv(sock, reply, BUFLEN, 0);

							if (result <= 0) {
								if (result == 0) {
									updatecli(sock, emptyuser,"",0,"delete");
									printf("SERVER> The client connected on %d leaves\n", sock);
								} else {
									error(-10,"recv()");
								}

								if (close(sock) < 0) error(-10,"close()");
								FD_CLR(sock, &read_fds);
							} else {
								printf ("(TCP) The client connected on %d, sent the command: %s", sock, reply);

								if (strcmp(reply,"quit\n") == 0) {
									printf ("CLIENT> The client connected on  %d will leave soon\n", sock);
								}

								mastercommands(sock,utilizatori, max_users);

								result = send(sock, buffer, BUFLEN, 0);
								if (result < 0) error(-10,"send()");
							}
						}
					}
				}
			}
		}

		if (exitflag) break;
	}

	if (close(tcplistener) < 0) error(-10,"close()");
	if (close(udpsock) < 0) error(-10,"close()");
	return 0;
}
