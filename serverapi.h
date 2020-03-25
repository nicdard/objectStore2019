/*
 Libreria contente le funzioni di gestione delle richieste del client
 utilizzate dal thread servente, tutte segnalano un eventuale errore 
 settando il valore dell'apposito puntatore error in ingresso (da inizializzare nel chiamante)
*/
#ifndef _SERVER_API_H_
#define _SERVER_API_H_

#include "messages.h"
#include "config.h"
#include "myerror.h"
#include "liste.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/select.h>

/* Definizione del tipo delle statistiche */
typedef struct statistics {
    unsigned long nusers;           // n. utenti totali dello store
    unsigned long nonline;          // n. utenti connessi
    unsigned long nfiles;           // n. files totali presenti nello store
    unsigned long dimension;        // dimensioni totali dello storage (somma delle dimensioni di tutti i files)
    list_t *onlineclients;          // nomi dei clients connessi
    list_t *allclients;             // nomi di tutti i clients dello store 
} statistics_t;

/* Rappresentazione interna degli errori del thread servente */
typedef enum workerCmdError {
    NO_SERVERW_ERROR = 0,           // Non ci sono errori
    #ifdef DEBUG
    COMMAND_MISMATCH,               // Usato per debugging, si verifica solo se c'è una associazione sbagliata fra comando e funzione che gestisce
    #endif
    STORE_DIRECTORY_UNAVAILABLE,    // La directory STORE_PATH (@link config.h) non è disponibile
    CLIENT_DIRECTORY_UNAVAILABLE,   // La directory del client non è disponibile
    CLIENT_ALREADY_IN_USE,          // Il client che chiede la registrazione ha nome uguale a quello di uno già online
    SHARED_DATA_ERROR,              // Indica un errore a livello di gestione di dati o strutture del server
    FILE_NOT_FOUND,                 // Il file non esiste o non può essere aperto
    PATH_NAME_ERROR,                // Il path è sbagliato (file non esiste o buffer overflow)
    COMMUNICATION_ERROR,            // Errore di comunicazione sulla socket (write o read fallita)
    DATA_ERROR                      // Indica errore di lettura da file o di accesso a altre informazioni sui dati dello store
} workerCmdError_t;


/* Mutex globale e contatore per softwhutdown del server
   prima di chiamare exit aspetto che i threads terminino le operazioni pending in elaborazione, così mantengo consistenza
*/
extern unsigned long active_threads_counter;
extern pthread_mutex_t active_threads_mutex;
// Funzione per decremento threadsafe del contatore
void decrActiveThreads(void);
// Attende che i tutti i thread abbiano concluso le operazioni pendenti che stanno eseguendo per un soft shutdown del server
void waitActiveThreads(void);


/* Funzione di cleanup per socket, mutex e allocazioni di memoria */
void cleanup(void);


/* Gestisce la richiesta REGISTER */
void registerFun(requestHeader_t *header, workerCmdError_t *error);

/* Gestisce LEAVE
   N.B. ha sempre successo (vedi testo progetto) a meno di problemi con la socket
 */
void leaveFun(requestHeader_t *header, char *username, workerCmdError_t *error);

/* Gestisce la richiesta STORE
   Viene passato anche il puntatore al buffer che potrebbe 
   contenere anche la prima parte dei dati da memorizzare
   Si occupa di leggere la restante parte di dati dalla socket 
   e memorizzare il tutto
 */
void storeFun(int socket_fd, requestHeader_t *header, char *username, dataBuffer_t *buff, workerCmdError_t *error);


/* Gestisce la richiesta DELETE */
void deleteFun(requestHeader_t *header, char *username, workerCmdError_t *error);


/* Gestisce la richiesta RETRIEVE
   Si occupa della lettura e scrittura bufferizzata dal file 
   nella socket
*/
void retrieveFun(int socket_fd, requestHeader_t *header, char *username, workerCmdError_t *error);


/* Inizializza la struttura dati statistic_t del server 
   che contiene i contatori per i log alla ricezione di SIGUSR1
   e per check interni (ad esempio non far registrare un client con lo stesso nome di uno non ancora disconnesso)
   N.B. da chiamare solo e soltanto prima di mettere in ascolto il server 
   Per l'update della struttura dati si utilizzino le funzioni apposite
   TODO disabilitare SIGUSR1 all'inizio della funzione e riabilitare alla fine
*/
void initializeStatistics();


/* Print to the given I/O opened stream FILE the server statistics
   If fd == NULL prints by default to the stderr
*/
void printStatistics(FILE *fd);

/*
 Wrapper di writen (messages.h)
 setta a COMMUNICATION_ERROR error se c'è un errore durante la scrittura sulla socket
*/
void writeToClient(long fd, void *buff, size_t size, workerCmdError_t* error);

/*
 Esporto questa funzione ausiliaria perché se ho un errore nel thread servente devo sempre 
 rimuovere dalla lista dei client online il thread che sta morendo
*/
void forceRemoveClientOnline(const char *username);

#endif