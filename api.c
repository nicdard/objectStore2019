#include "./api.h"


// Dichiaro globalmente nella libreria la socket
// così la posso usare senza farla vedere all'esterno (dal client)
static int sktfd = -1; 
// N.B. non funziona con client multithread

// Legge dalla socket la risposta del server, parsa e salva nel caso in remainingData i restanti dati
// ritorna 1 se l'header segnala che il server sta rispondendo correttamente (OK, DATA)
// 0 per KO o errore di parsing della risposta
static int processResponseHeader(char *name, dataBuffer_t *remainingData, size_t *left_data_lenn);

int os_connect(char *name) {
    // Creo indirizzo
    struct sockaddr_un sa;
    strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;
    // Creo la socket e salvo il desrittore nella variabile globale
    CHECK_EQS_NOEXIT(sktfd = socket(AF_UNIX, SOCK_STREAM, 0), (-1), "[CLIENT]: Error calling socket\n", return 0);
    // Provo a connettermi
    CHECK_EQS_NOEXIT(connect(sktfd, (struct sockaddr *) &sa, sizeof(sa)), (-1), "[CLIENT]: Error calling connect\n", return 0) 

    char _msg[BUFFER_SIZE] = {'\0'};
    dataBuffer_t msg = {
        .buffer = _msg,
        .bufferSize = BUFFER_SIZE,
        .dataLen = 0
    };
    CHECK_EQS_NOEXIT(msg.dataLen = snprintf(msg.buffer, msg.bufferSize - 1, "REGISTER %s \n", name), (-1), "[CLIENT]: Writing register header\n", return 0);
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG CLIENT]: Sto per scrivere %s sulla socket di lunghezza %ld\n", msg.buffer, msg.dataLen);
    #endif
    CHECK_EQS_NOEXIT(write(sktfd, msg.buffer, msg.dataLen), (-1), "[CLIENT]: Writing register header on socket\n", return 0);
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG CLIENT]: Ho scritto sulla socket\n");
    #endif
    
    return processResponseHeader("REGISTER", NULL, NULL); 
}


int os_store(char *name, void *block, size_t len) {
    char _msg[BUFFER_SIZE] = {'\0'};
    dataBuffer_t msg = {
        .buffer = _msg,
        .bufferSize = BUFFER_SIZE,
        .dataLen = 0
    };
    CHECK_EQS_NOEXIT(msg.dataLen = snprintf(msg.buffer, msg.bufferSize - 1, "STORE %s %ld \n ", name, len), (-1), "[CLIENT]: Writing store\n", return 0);
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG CLIENT]: Sto per scrivere %s sulla socket di lunghezza %ld\n", msg.buffer, msg.dataLen);
    #endif
    CHECK_EQS_NOEXIT(write(sktfd, msg.buffer, msg.dataLen), (-1), "[CLIENT]: Writing store header on socket\n", return 0);
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG CLIENT]: Ho scritto sulla socket\n");
    #endif
    CHECK_EQS_NOEXIT(write(sktfd, block, len), (-1), "[CLIENT]: Writing store data on socket\n", return 0);
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG CLIENT]: Ho scritto sulla socket i dati\n");
    #endif
    return processResponseHeader("STORE", NULL, NULL);
}

void* os_retrieve(char* name) {
    char _msg[BUFFER_SIZE] = {'\0'};
    dataBuffer_t msg = {
        .buffer = _msg,
        .bufferSize = BUFFER_SIZE,
        .dataLen = 0
    };
    CHECK_EQS_NOEXIT(msg.dataLen = snprintf(msg.buffer, msg.bufferSize - 1, "RETRIEVE %s \n", name), (-1), "[CLIENT]: Writing retrieve\n", return NULL);
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG CLIENT]: Sto per scrivere %s sulla socket di lunghezza %ld\n", msg.buffer, msg.dataLen);
    #endif
    CHECK_EQS_NOEXIT(write(sktfd, msg.buffer, msg.dataLen), (-1), "[CLIENT]: Writing retrieve header on socket\n", return NULL);
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG CLIENT]: Ho scritto sulla socket\n");
    #endif
    char _data[BUFFER_SIZE] = {'\0'};
    dataBuffer_t data = {
        .buffer = _data,
        .bufferSize = BUFFER_SIZE,
        .dataLen = 0
    };
    // 1. Processo l'header
    size_t left_data_len = 0;
    int res = processResponseHeader("RETRIEVE", &data, &left_data_len);
    if (res == 0) {
        // C'è stato un errore
        return NULL;
    }
    // Alloco il buffer con i dati da ritornare
    size_t retrieve_len = left_data_len + data.dataLen;
    void *retrieveData = malloc(sizeof(char) * retrieve_len);
    if (retrieveData == NULL) {
        fprintf(stderr, "[CLIENT]: Allocando memoria nella funzione di retrieve\n");
        return NULL;
    }
    bzero(retrieveData, retrieve_len);
    memcpy(retrieveData, data.buffer, data.dataLen);

    char _buffer[BUFFER_SIZE] = {'\0'};
    dataBuffer_t buffer = {
        .buffer = _buffer,
        .bufferSize = BUFFER_SIZE,
        .dataLen = 0
    };
    size_t left_index = data.dataLen; // Usato per calcolare l'offset dal quale memorizzare i dati letti con il buffer a ogni ciclo
    // 2. Leggo i dati mancanti
    while (left_data_len > 0) {
        CHECK_EQS_NOEXIT(buffer.dataLen = read(sktfd, buffer.buffer, buffer.bufferSize), (-1), "[CLIENT]: Reading Retrieve left data\n", return NULL);
        if (buffer.dataLen == 0) {
            #ifdef DEBUG
            fprintf(stderr, "[DEBUG CLIENT]: An unaxpected error occurred while reading data in retrieve\n");
            return NULL;
            #endif
        }
        #ifdef DEBUG
        fprintf(stderr, "[DEBUG CLIENT]: Retrieve Letti %ld da socket: %s\n", buffer.dataLen, buffer.buffer);
        #endif
        memcpy((char *)retrieveData + left_index, buffer.buffer, buffer.dataLen);
        left_data_len -= buffer.dataLen;
        left_index += buffer.dataLen;
    }
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG CLIENT]: Retrieve Letti in totale da socket:\n%s\n", (char *)retrieveData);
    #endif
    return retrieveData;
}

int os_delete(char *name) {
    char _msg[BUFFER_SIZE] = {'\0'};
    dataBuffer_t msg = {
        .buffer = _msg,
        .bufferSize = BUFFER_SIZE,
        .dataLen = 0
    };
    CHECK_EQS_NOEXIT(msg.dataLen = snprintf(msg.buffer, msg.bufferSize - 1, "DELETE %s \n", name), (-1), "[CLIENT]: Writing delete\n", return 0);
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG CLIENT]: Sto per scrivere %s sulla socket di lunghezza %ld\n", msg.buffer, msg.dataLen);
    #endif
    CHECK_EQS_NOEXIT(write(sktfd, msg.buffer, msg.dataLen), (-1), "[CLIENT]: Writing delete header on socket\n", return 0);
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG CLIENT]: Ho scritto sulla socket\n");
    #endif
    return processResponseHeader("DELETE", NULL, NULL);
}

int os_disconnect() {
    const char *msg = "LEAVE \n";
    int retval = 0;
    CHECK_EQS_NOEXIT(retval = write(sktfd, msg, strlen(msg)), -1, "[CLIENT] Error writing LEAVE on socket\n", return 0);
    char response[BUFFER_RESPONSE_SIZE] = {'\0'};
    CHECK_EQS_NOEXIT(retval = read(sktfd, response, BUFFER_RESPONSE_SIZE), -1, "[CLIENT]: Error reading leave response", return 0);
    if (retval == 0) {
        fprintf(stderr, "[CLIENT]: Error reading leave response, got EOF");
        return 0;
    }
    // N.B. da spec la leave può ritornare solo OK \n, inoltre dopo la leave mi aspetto la chiusura della socket,
    // altri dati sarebbero dal server sarebbero sbagliati
    // quindi per brevità faccio un semplice controllo invece di parsare l'header
    if (strcmp(response, SIMPLE_OK_RESPONE) == 0) {
        return 1;
    } else {
        return 0;
    }
}

// =======================
// Funzioni ausiliarie
static int processResponseHeader(char *name, dataBuffer_t *remainingData, size_t *left_data_len) {
    char _error_message[BUFFER_RESPONSE_SIZE] = {'\0'};
    dataBuffer_t error_message = {
        .buffer = _error_message,
        .bufferSize = BUFFER_RESPONSE_SIZE,
        .dataLen = 0
    };
    responseHeader_t responseHeader = {
        .response_cmd = UNKNOWN_R,
        .data_length = 0,
        .error_message = error_message
    };
    headerParsingError_t error = NO_MSG_ERROR;
    getResponseHeader(sktfd, &responseHeader, remainingData, &error);
    if (error == NO_MSG_ERROR) {
        if (strcmp(name, "RETRIEVE") == 0) {
            // Recupero la lunghezza dei dati
            *left_data_len = responseHeader.data_length - remainingData->dataLen;
            #ifdef DEBUG 
            fprintf(stderr, "[DEBUG CLIENT]: Retrieve: dati rimasti da leggere %ld, dati nel buffer %ld\n", *left_data_len, remainingData->dataLen);   
            #endif
        }
        switch(responseHeader.response_cmd) {
            case DATA:
            #ifdef DEBUG 
            {
                fprintf(stderr, "[DEBUG CLIENT]: %s header parsed successfully\n", name);
                return 1;
            }
            #endif
            case OK: {
                #ifdef DEBUG
                fprintf(stderr, "[DEBUG CLIENT]: %s succeded\n", name);
                #endif
                return 1;
            }
            case KO: 
            #ifdef DEBUG 
            {
                fprintf(stderr, "[DEBUG CLIENT]: %s failed. Got from server: %s\n", name, responseHeader.error_message.buffer);
                return 0;
            }
            #endif
            case UNKNOWN_R: 
            default: {
                #ifdef DEBUG
                fprintf(stderr, "[DEBUG CLIENT]: FATAL ERROR\n");
                #endif
                return 0;
            }
        }
    }

    perror_header_parser(error, "");
    fprintf(stderr, "[CLIENT]: FATAL ERROR\n");
    return 0;
}
