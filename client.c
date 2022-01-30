/*
 * Progetto Reti di Calcolatori
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
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
 
#define MAX_MESSAGGIO 10 //Dimensione massima dei messaggi
#define DATA_SIZE   ( (  100 * 1024 * 1024) ) //dimensione del trasferimento 100 Mb
 
typedef struct misurazione {
    double bw_dl; // bandwidth DL in Mb/s
    double bw_ul; // bandwidth UL in Mb/s
    float bytes; //bytes letti
    int bufsize; //dimensione del buffer di lettura
    char* server_address; //indirizzo del server
    int server_port; //porta del server
};
 
void misura_connessione(struct misurazione *misura);
int connetti(struct misurazione *misura);
 
 
int main(int argc, char **argv)
{
    struct misurazione st_misurazione;
    if (argc < 3) {
		puts("utilizzare client <ip server> <porta server>");
		exit(1);
	}
    st_misurazione.server_address=argv[1];
    st_misurazione.server_port=atoi(argv[2]);
    misura_connessione(&st_misurazione);
    return 0;
}
 
void misura_connessione(struct misurazione *misura) {
        
    //inizializza la misurazione
    misura->bw_dl=0;
    misura->bw_ul=0;
    misura->bytes=0;
    misura->bufsize = 512*1024; //512KB
    //misura->server_address = "192.168.1.104";
    //misura->server_port = 8081;
    struct timeval start, end;
    int sock;
    
    sock = connetti(misura); //invoca la funzione che si occupa di creare il socket
    char messaggio[MAX_MESSAGGIO]; //crea il buffer per i messaggi
    bzero(messaggio,MAX_MESSAGGIO); //inizializza il buffer per i messaggi
    char* payload = malloc(misura->bufsize); //inizializza il buffer per il test
    strcpy(messaggio, "TEST"); //prepara il messaggio per dire al server di iniziare il test
    write(sock,messaggio,sizeof(messaggio)); //invia la richiesta di test al server
    bzero(messaggio,MAX_MESSAGGIO);
    read(sock,messaggio,sizeof(messaggio));
    if (strncmp(messaggio, "OK",2) == 0) { //se la risposta è OK prepara il test di DL
		int bytesdaleggere = DATA_SIZE; //imposta i bytes da leggere rimanenti
		gettimeofday(&start, NULL); //prende il timestamp iniziale
		while (misura->bytes < DATA_SIZE) { //finchè ci sono bytes da leggere
			if (misura->bufsize > bytesdaleggere) { //se i bytes rimanenti sono meno dello spazio del buffer 
				misura->bufsize = bytesdaleggere; //aggiusta il numero di bytes da leggere
			}
			misura->bytes+=read(sock,payload,misura->bufsize); //aggiorna il contatore dei bytes letti
			bytesdaleggere = DATA_SIZE - misura->bytes; // e quello dei bytes da leggere
		}
	gettimeofday(&end, NULL); //prende il timestamp finale  
	free(payload);
	}
	bzero(messaggio,MAX_MESSAGGIO);
    read(sock,messaggio,sizeof(messaggio)); // a questo punto il test DL è finito attendiamo conferma per la prossima fase
    
    double sec = (((end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec));
    sec = sec / 1000000; //calcola il tempo necessario al trasferimento in secondi
    misura->bw_dl = (((misura->bytes/(1024*1024)) * 8)/sec); //calcola la banda in DL in Mbit/s
    if (strncmp(messaggio, "UL",2) == 0) { //se la risposta è UL prepara il test di UL
		char* data = malloc(DATA_SIZE * sizeof(char)); //prepara i dati da usare per il test di UL
		for (int i=0; i < DATA_SIZE; i++)   
				  data[i] = 'S';
		write(sock, data, DATA_SIZE); //invia i dati di test al server
		free(data); //elimina il buffer
		read(sock, messaggio,sizeof(messaggio)); //legge la risposta del server
		misura->bw_ul = atof(messaggio); //conserva il risultato nel campo apposito della misura
	}
	bzero(messaggio,MAX_MESSAGGIO);
	snprintf(messaggio,sizeof(messaggio),"%f",misura->bw_dl);
	write(sock,messaggio,sizeof(messaggio)); //notifica la misurazione rilevata al server
	bzero(messaggio,MAX_MESSAGGIO);
	strcpy(messaggio, "EXIT");
	write(sock,messaggio,sizeof(messaggio)); //esce dal test
    close(sock);
    printf("Bandwidth: DL->%f Mb/s UL->%f Mb/s\n",misura->bw_dl, misura->bw_ul); //stampa a video il risultato del test
    
}
 
int connetti(struct misurazione *misura) {
    // creazione del socket
    int sock;
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Creazione del socket fallita");
        exit(1);
    }
    struct sockaddr_in serveraddress; // inizializza la struttura con i dati per la connessione
    bzero(&serveraddress, sizeof(serveraddress));
   
    // assegna ip e porta al socket nel formato corretto
    serveraddress.sin_family = AF_INET;
    serveraddress.sin_addr.s_addr = inet_addr(misura->server_address);
    serveraddress.sin_port = htons(misura->server_port);
    
    // esegue la connessione al server
    if(connect(sock, (struct sockaddr *) &serveraddress, sizeof(serveraddress)) < 0)
    {
        perror("Impossibile collegarsi al server");
        exit(1);
    }
 
    return sock; //restituisce il descrittore del socket
}

