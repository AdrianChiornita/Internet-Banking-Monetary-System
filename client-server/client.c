#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

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

int exitflag;
int logged;
int transferop;
int unlockop;
int lastcard;
char buffer[BUFLEN], reply[BUFLEN];
FILE *clientlog;

void error(int code, char *msg) {
    memset(reply, 0 , BUFLEN);
	switch (code) {
		case NAC:
            fprintf(clientlog,"-1 : The client is not authenticated\n");
			printf("-1 : The client is not authenticated\n");
			break;
		case SAO:
            fprintf(clientlog,"-2 : Session already opened\n");
			printf("-2 : Session already opened\n");
			break;
		case WPIN:
            fprintf(clientlog,"-3 : Wrong PIN\n");
			printf("-3 : Wrong PIN\n");
			break;
		case CNF:
            sprintf(reply,"-4 : Card number doesn't exist\n");
			printf("-4 : Card number doesn't exist\n");
			break;
		case CBK:
            fprintf(clientlog,"-5 : Card number blocked\n");
			printf("-5 : Card number blocked\n");
			break;
		case OPF:
            fprintf(clientlog,"-6 : Operation failed\n");
			printf("-6 : Operation failed\n");
			break;
		case UBF:
            fprintf(clientlog,"-7: Unblocking failed\n");
			printf("-7: Unblocking failed\n");
			break;
		case IFU:
            fprintf(clientlog,"-8 Insuficient funds\n");
			printf("-8 Insuficient funds\n");
			break;
		case OPC:
            fprintf(clientlog,"-9 : Operation canceled\n");
			printf("-9 : Operation canceled\n");
			break;
		case FERR:
            fprintf(clientlog,"-10: Function error: %s\n",msg);
			printf("-10: Function error: %s\n",msg);
			exit(1);
			break;
		default: break;
	}
}

int mastercommands (char *command) {

    char *option;
    char *nr;

    char cpy[BUFLEN];
    strcpy(cpy,command);

    option = strtok (command, " \n");

    if (strcmp(option,"quit") == 0) {
        exitflag = true;
        return 1;
    }
    if (unlockop) {
        char cp[BUFLEN];
        strcpy(cp,buffer);
        memset(buffer, 0 , BUFLEN);
        sprintf(buffer, "%d %s", lastcard,cp);
        unlockop = false;
        return 2;
    }

    if (strcmp(option,"unlock") == 0) {
        if (logged) return SAO;
        if (lastcard == -1) return OPF;

        unlockop = true;
        memset(buffer, 0 , BUFLEN);
        sprintf(buffer, "unlock %d\n", lastcard);
        return 2;
    }

    if (strcmp(option,"login") != 0 && strcmp(option,"logout") != 0 && strcmp(option,"listsold") != 0 &&
        strcmp(option,"transfer") != 0 && strcmp(option,"unlock") != 0 && strcmp(option,"quit") != 0 && !transferop && !unlockop) {
            printf("CLIENT> Invalid command.\n");
            return 0;
        }

    if (strcmp(option,"login") == 0) {
        nr = strtok (NULL, " \n");
        lastcard = atoi(nr);
    }

    if (strcmp(option,"login") != 0 && logged == false && !unlockop) return NAC;
    if (strcmp(option,"login") == 0 && logged == true && !unlockop) return SAO;

    if (strcmp(option,"transfer") == 0 ) transferop = true;

    return 1;
}
int main(int argc, char *argv[]) {

    lastcard = -1;
    int tcpsock, udpsock, result;
    struct sockaddr_in serv_addr;

    char filename[BUFLEN];
    sprintf(filename, "client-%d.log", getpid());

    fd_set read_fds, tmp_fds;
    int fdmax;

    if (argc < 3) {
       fprintf(stderr,"CLIENT> Usage client: %s <IP_server> <port_server>\n", argv[0]);
       exit(0);
    }

    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    if ((tcpsock = socket(AF_INET, SOCK_STREAM, 0)) < 0) error(-10,"socket()");
    if ((udpsock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)  error(-10,"socket()");

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    inet_aton(argv[1], &serv_addr.sin_addr);

    if ( (clientlog = fopen (filename, "w")) == NULL)
		error(-10,"fopen()");

    if (connect(tcpsock,(struct sockaddr*) &serv_addr,sizeof(serv_addr)) < 0)
       error(-10,"connect()");

    FD_SET(0, &read_fds);
    FD_SET(tcpsock, &read_fds);
    fdmax = tcpsock;

    FD_SET(udpsock, &read_fds);
	if (fdmax < udpsock) fdmax = udpsock;

    while (true) {
        tmp_fds = read_fds;

        if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1)
			error(-10,"select()");

		if (FD_ISSET(0, &tmp_fds)) {

            memset(buffer, 0 , BUFLEN);
            fgets(buffer, BUFLEN - 1, stdin);

            char cpy[BUFLEN];
            strcpy(cpy,buffer);
            result = mastercommands(cpy);

            if (result < 0) error(result,"");

            if (result == 2) {
                result = sendto(udpsock, buffer, BUFLEN, 0, (struct sockaddr*) &serv_addr, sizeof(struct sockaddr));
                if (result < 0) error(-10,"sendto()");
            }
            else {
                if (result == 1) {
                    result = send(tcpsock, buffer, BUFLEN, 0);
                    if (result < 0) error(-10,"send()");
                }
            }
            if (strstr(buffer, "unlock") != NULL) {
                memset(buffer, 0 , BUFLEN);
                sprintf(buffer,"unlock\n");
            }
        }

        if (FD_ISSET(tcpsock, &tmp_fds)) {

            memset(reply, 0 , BUFLEN);
            result = recv(tcpsock, reply, BUFLEN, 0);

            if (result <= 0) {
                if (result == 0) {
                    printf("CLIENT> Server is closed\n");
                    if (close(tcpsock) < 0) error(-10,"close()");
                    if (close(udpsock) < 0) error(-10,"close()");
                    if (fclose(clientlog) < 0) error(-10,"fclose()");
                    return 0;
                }
                else
                    error(-10,"recv()");
            }

            char cpy[BUFLEN];
            strcpy(cpy,reply);

            char *token;
            token = strtok(cpy," \n");
            if (strcmp(token,"IBANK>") == 0) {
                token = strtok(NULL," \n");

                if (strcmp(token,"Welcome") == 0)
                    logged = true;
                if (strcmp(reply,"IBANK> Client disconnected\n") == 0)
                    logged = false;
                if (strcmp(reply,"IBANK> Transfer made\n") == 0 && transferop)
                    transferop = false;
                if (strcmp(reply,"IBANK> -9 : Operation failed\n") == 0 && transferop)
                    transferop = false;
                 if (strcmp(reply,"IBANK> The server will go offline\n") == 0 && transferop)
                    exitflag = true;
            }

            printf("%s", reply);
            if (strcmp(reply,"") != 0) {
                char cpy[BUFLEN];
                char *header;
                strcpy(cpy,reply);

                header = strtok (cpy, " \n");
                if (strcmp(header,"IBANK>") == 0) {
                        fprintf(clientlog,"%s",buffer);
                        fprintf(clientlog,"%s",reply);
                }
                if (strcmp(buffer,"quit\n") == 0) {
                        fprintf(clientlog,"%s",buffer);
                }
            }
        }

        if (FD_ISSET(udpsock, &tmp_fds)) {

            memset(reply, 0 , BUFLEN);
            int size = sizeof(struct sockaddr);
            result = recvfrom(udpsock, reply, BUFLEN, 0, (struct sockaddr*) &serv_addr, &size);

             if (result <= 0) {
                if (result == 0) {
                    printf("CLIENT> Server is closed\n");
                    exitflag = true;
                }
                else
                    error(-10,"recvfrom()");
             }

            if (strcmp(reply,"UNLOCK> Card number unblocked\n") == 0 && unlockop)
                    unlockop = false;
            if (strcmp(reply,"UNLOCK> −7 : Unblocking failed\n") == 0 && unlockop)
                    unlockop = false;
            if (strcmp(reply,"UNLOCK> −6 : Operation failed\n") == 0 && unlockop)
                    unlockop = false;

            printf("%s", reply);
            if (strcmp(reply,"") != 0) {
                char cpy[BUFLEN];
                char *header;
                strcpy(cpy,reply);

                header = strtok (cpy, " \n");
                if (strcmp(header,"UNLOCK>") == 0) {
                        fprintf(clientlog,"%s",buffer);
                        fprintf(clientlog,"%s",reply);
                }
                if (strcmp(buffer,"quit\n") == 0) {
                        fprintf(clientlog,"%s",buffer);
                }
            }
        }
        if (exitflag) break;
    }

    if (close(tcpsock) < 0) error(-10,"close()");
    if (close(udpsock) < 0) error(-10,"close()");
    if (fclose(clientlog) < 0) error(-10,"fclose()");
    return 0;
}
