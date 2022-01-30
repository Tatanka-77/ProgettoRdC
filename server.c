/*
 * Progetto Reti di Calcolatori
 * server.c
 * 
 * Copyright 2022 Sergio Raneli<sergio.raneli@community.unipa.it>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */

#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <mariadb/mysql.h>	
#include <unistd.h>

#define MAX 10 //Dimensione massima dei messaggi
#define DATA_SIZE   ( (100 * 1024 * 1024) ) //dimensione del trasferimento 100 Mb
#define PORT 8080 //porta del server
#define SA struct sockaddr //per abbreviare...

typedef struct risultato { //struttura per memorizzare il risultato del test
    double bw_dl; // bandwidth DL in Mb/s
    double bw_ul; // bandwidth UL in Mb/s
    char* client_address; //indirizzo del server
}risultato;

void eseguitest(int connfd, struct risultato* dati_test);
void memorizzarisultato(struct risultato* dati_test);

int main()
{
	int sockfd, connfd, len;
	struct sockaddr_in servaddr, cli;
	struct risultato dati_test;

	// creazione del socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		printf("Creazione del socket fallita...\n");
		exit(0);
	}
	printf("Socket creato con successo..\n");
	bzero(&servaddr, sizeof(servaddr));

	// assegna IP e PORTA al server
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(PORT); //nel formato Endian corretto

	// Associa il socket a indirizzo IP e porta assegnati
	if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
		printf("Bind del socket fallito...\n");
		exit(1);
	}
	else
		printf("Bind del socket eseguito con successo..\n");

	//passa allo stato "in ascolto"
	if ((listen(sockfd, 0)) != 0) {
		printf("Impossibile mettere il socket in ascolto...\n");
		exit(1);
	}
	else
		printf("Server in ascolto...\n");
	len = sizeof(cli);
	for (;;) {
		// Accetta la connsessione del client
		connfd = accept(sockfd, (SA*)&cli, &len);
		if (connfd < 0) {
			printf("Connessione del client rifiutata...\n");
			exit(1);
		}
		else {
			printf("Connessione del client accettata...\n");
			dati_test.client_address =  inet_ntoa(cli.sin_addr);
			dati_test.bw_dl=0;
			dati_test.bw_ul=0;
		}
	eseguitest(connfd,&dati_test); //esegue il test 
	printf("RISULTATO TEST\nINDIRIZZO IP:%s\tDL:\%f Mb/s\tUL:%f Mb/s\n",dati_test.client_address,dati_test.bw_dl,dati_test.bw_ul);
	}
}

void eseguitest(int connfd, struct risultato* dati_test) {
	char buff[MAX];
	char* data = malloc(DATA_SIZE * sizeof(char));
	struct timeval start, end;
	int bytes=0;
	int bufsize = 512*1024; //512KB 
	char* payload = malloc(bufsize); //inizializza il buffer per il test DL
	
	for (;;) {
		bzero(buff, MAX);
		read(connfd, buff, sizeof(buff));
		if (strncmp("TEST",buff,4) == 0 ) { //se il client richiede l'inizio del test
			bzero(buff, MAX);
			strcpy(buff, "OK"); //conferma l'esecuzione del test
			for (int i=0; i < DATA_SIZE; i++)   
				  data[i] = 'S'; //prepara i dati da inviare al client
			write(connfd, buff, sizeof(buff)); //invia la conferma
			write(connfd, data, DATA_SIZE); //invia i dati di test
			free(data);
			bzero(buff, MAX);
			snprintf (buff,sizeof(buff),"UL"); //invia al client il messaggio per iniziare il test UL
			write(connfd, buff, sizeof(buff));
			int bytesdaleggere = DATA_SIZE; //imposta i bytes da leggere rimanenti
			gettimeofday(&start, NULL); //prende il timestamp iniziale
			while (bytes < DATA_SIZE) { //finchè ci sono bytes da leggere
				if (bufsize > bytesdaleggere) { //se i bytes rimanenti sono meno dello spazio del buffer 
					bufsize = bytesdaleggere; //aggiusta il numero di bytes da leggere
				}
				bytes+=read(connfd,payload,bufsize); //aggiorna il contatore dei bytes letti
				bytesdaleggere = DATA_SIZE - bytes; // e quello dei bytes da leggere
			}
			gettimeofday(&end, NULL); //prende il timestamp finale
			double sec = (((end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec));
			sec = sec / 1000000; //calcola il tempo in secondi
			dati_test->bw_ul = ((bytes/(1024*1024)) * 8)/sec; //salva il risultato del test UL
			snprintf (buff,sizeof(buff),"%f",dati_test->bw_ul);
			write (connfd,buff,sizeof(buff)); //lo invia al client
			bzero(buff, MAX);
			read(connfd, buff, sizeof(buff)); //riceve il risultato del test DL
			dati_test->bw_dl = atof(buff);
			
			
			
		}
		if (strncmp("EXIT", buff, 4) == 0) { //se il client richiede di uscire esce dalla funzione e torna in ascolto
			printf("Client disconnesso...\n");
			memorizzarisultato(dati_test);
			break;
		}
	}
}

void memorizzarisultato(struct risultato* dati_test) {
	// Initialize Connection
   MYSQL *conn;
   if (!(conn = mysql_init(0)))
   {
      fprintf(stderr, "Impossibile connettersi al DB\n");
      exit(1);
   }

   if (!mysql_real_connect(
         conn,                  // Connection
         "127.0.0.1", 			// Host DB
         "speedtest",           // Username DB
         "sp33dtest",   		//  password DB
         "speedtest",           // nome database
         3306,                  // Porta
         NULL,                  // No socket file
         0                     //  NO Additional options
      ))
   {
      // In caso di mancata connessione chiude l'handler
      fprintf(stderr, "Errore di connessione al DB: %s\n", mysql_error(conn));
      mysql_close(conn);
   }
      char query[255];
      sprintf(query, "INSERT INTO risultati(ip_address,dl,ul) VALUES ('%s',%f,%f);",dati_test->client_address,dati_test->bw_dl,dati_test->bw_ul);
      if (mysql_query(conn, query ) != 0)                   
		{                                                                                                  
		fprintf(stderr, "Query Failure\n");                                                              
		mysql_close(conn);
		}
}
