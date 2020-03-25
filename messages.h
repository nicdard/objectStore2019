/*
 Libreria contente i tipi e le funzioni di utilità per la creazione 
 e il parsing di messaggi della api
*/
#ifndef _MESSAGES_H_
#define _MESSAGES_H_

#include <stddef.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "./myerror.h"
#include "./config.h"

/* Stampa in un formato leggibile l'errore di parsing dell'header di una request */
#define perror_header_parser(e, header_str) \
    char *message = NULL; \
    switch(e) { \
        case NO_HEADER: { message =  "Parsing header: \n-> No Header"; break;} \
        case NO_COMMAND: { message =  "Parsing header: \n-> No Command"; break;} \
        case UNKOWN_COMMAND: { message =  "Parsing header: \n-> Unknown Command"; break;} \
        case NO_NAME: { message =  "Parsing header: \n-> No Name"; break;} \
        case NO_LEN: { message =  "Parsing header: \n-> No Len (should occur only when a STORE or a DATA message is sent"; break;} \
        case MALFORMED_HEADER: { message =  "Parsing header: \n-> Malformed Header (may occur if len is not a number [see code for more details])"; break;} \
        case BUFFER_OVERFLOW: { message = "Parsing header:\n-> Parsing data or name parameter in header, buffer overflow"; break;} \
        case READ_ERROR: {message = "Parsing header: \n-> Got EOF from socket"; break;} \
        case NO_MSG_ERROR: default: { message = "Parsing header: No error"; break; }\
    } \
    fprintf(stderr, "\n%s\n-> %s\n", message, header_str); \

// Risposta semplice di operazione eseguita con successo
#define SIMPLE_OK_RESPONE "OK \n"
// Risposta semplice di operazione fallita
#define SIMPLE_KO_RESPONE "KO \n"
// Risposta che indica un errore non gestibile nel server 
// che ne provoca generalmente lo shutdown (del server o del solo thread servente)
#define FATAL_KO_RESPONSE "KO Fatal internal error \n"
// Risposta a una richiesta di REGISTER da parte di un client con un nome che è già nella lista dei registrati
#define CLIENT_IN_USE_KO_RESPONSE "KO Client name is already in use (registered) \n"
// Risposta inviata dal server se l'oggetto su cui eseguire le operazioni richieste non è presente nella cartella
#ifndef DEBUG
#define FILE_NOT_FOUND_KO_RESPONSE "KO File not found \n"
#endif
#ifdef DEBUG
#define FILE_NOT_FOUND_KO_RESPONSE "KO File not found, sending very long message to client to test if it is all right, aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa aaaaaaaaaaaaaaaaaaaaaaaaaa aaaa\n"
#endif

// ===========================
// TYPE DEFINITIONS

/* Errori che si possono verificare durante la ricezione di un header dal server o dal client 
   @link perror_header_parser per maggiori info sul significato e thread worker del server
*/
typedef enum headerParsingError {
    NO_MSG_ERROR = 0,       // Indica che il messaggio è stato parsato correttamente
    NO_HEADER,
    NO_COMMAND,
    UNKOWN_COMMAND,
    NO_NAME,
    NO_LEN,
    MALFORMED_HEADER,
    BUFFER_OVERFLOW,
    READ_ERROR
} headerParsingError_t;

/* Rappresentazione interna dei comandi da client a server */
typedef enum requestCommand {
    UNKOWN = 0,     // Rappresenta un comando non riconosciuto, segnala errore
    REGISTER,
    STORE,
    RETRIEVE,
    DELETE,
    LEAVE
} requestCommand_t;

typedef enum responseType {
    UNKNOWN_R = 0,
    KO,     // failed
    OK,     // success
    DATA    // data in response (exected only in response to RETRIEVE request)
} responseType_t;

/* Tipo del buffer, mantiene lunghezza del buffer e spazio occupato */
typedef struct dataBuffer {
    char            *buffer;    // Buffer 
    size_t          bufferSize; // Lunghezza del buffer
    long            dataLen;    // Spazio occupato, se < 0 indica un errore e il contenuto del buffer è inutilizzabile
} dataBuffer_t;

/* Tipo dell'Header parsato */
typedef struct requestHeader {
    requestCommand_t    cmd;            // Comando del client in rappresentazione interna 
    dataBuffer_t        *nameParam;     // Parametro del nome
    long                dataLength;     // Parametro len
} requestHeader_t;

/* Tipo dell'header in risposta dal server */
typedef struct responseHeader {
    responseType_t  response_cmd;       // Risposta del server in rappresentazione interna
    long            data_length;        // Parametro len (solo per DATA)
    dataBuffer_t    error_message;      // Messaggio di errore (buffer di lunghezza BUFFER_RESPONSE_SIZE @link config.h)
} responseHeader_t;

// =========================
// FUNCTIONS DEFINITIONS

/* Scrive size bytes sul descrittore fd da buff */
int writen(long fd, void *buff, size_t size);

/* Parsa l'header della richiesta a partire dal buffer
   Se ci sono dati oltre l'header li salva in remainingData (solo richiesta STORE)
   Se fallisce setta error coerentemente 
   Modifica il puntatore buff
*/
void parseRequestHeader(char *buff, requestHeader_t *parsed_header, dataBuffer_t *remainingData, headerParsingError_t *error);

/* Legge dalla socket e parsa l'header della risposta scritto dal server su socket_fd
   Se ci sono dati oltre l'header li salva in remainingData (solo risposta DATA)
   Se fallisce setta error
   Modifica il puntatore buff
*/
void getResponseHeader(int socket_fd, responseHeader_t *parsedHeader, dataBuffer_t *remainingData, headerParsingError_t *error);

#endif