#include "messages.h"

static requestCommand_t strCommandToEnum(char *strCommand, headerParsingError_t *error);
static responseType_t strResponseCommandToEnum(char *strCommand, headerParsingError_t *error);
static void copyInDataBuffer(dataBuffer_t *dataBuffer, char *rawdata, int len);
static long isNumber(char *s);

// =================
// Implementazione 

int writen(long fd, void *buff, size_t size) {
  size_t left = size;
  char *bufptr = (char*) buff;
  int r;

  while (left > 0) {
    if ( (r = write((int) fd, bufptr, left) ) == -1 ) {
      if (errno == EINTR) continue;
      return -1;
    }

    if (r == 0) return 0;

    left -= r;
    bufptr += r;
  }

  return 1;
}

void parseRequestHeader(char* buff, requestHeader_t *parsed_header, dataBuffer_t *dataBuffer, headerParsingError_t *error) {
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG SERVER]: Messages.c -> parseRequestHeader called %s\n===========", buff);
    #endif

    // Inizializzo
    parsed_header->cmd = UNKOWN;
    parsed_header->dataLength = 0;
    
    // 1. Check se ho ricevuto tutto l'header
    int hasnewline = strchr(buff, '\n') != NULL;
    if (!hasnewline) {
        // Ho un problema di lettura dalla socket probabilmente 
        // (potrebbe essere anche un problema di sincronismo, 
        // ma non è supportata la lettura bufferizzata dell'header,
        // considerato anche che la socket è locale alla stessa macchina)
        *error = NO_HEADER;   
        return;
    }

    // 2. Inizio a parsare l'header
    char *tmpstr = NULL;
    // 2.1 Parso il comando
    char *tmpcommand;
    CHECK_EQS_NOEXIT(tmpcommand = strtok_r(buff, " ", &tmpstr), NULL, "[SERVER]: Parsing STORE header, command parameter not found", *error = NO_COMMAND; return);
    parsed_header->cmd = strCommandToEnum(tmpcommand, error);
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG SERVER]: Ho parsato il comando %s\n", tmpcommand);
    #endif

    // 2.2 A seconda del comando ricevuto estraggo gli altri parametri dell'header
    switch (parsed_header->cmd) {
        case LEAVE: {
            // Controllo che non ci sia altro che il terminatore dell'header
            char *checker = strtok_r(NULL, " ", &tmpstr);
            if (checker != NULL && strcmp(checker, "\n") != 0) {
                *error = MALFORMED_HEADER;
            }
            return;
        }
        case STORE:
        case REGISTER:
        case DELETE:
        case RETRIEVE: {
            // Estraggo il nome
            char *tmpname;
            CHECK_EQS_NOEXIT(tmpname = strtok_r(NULL, " ", &tmpstr), NULL, "[SERVER]: Parsing STORE header, name parameter not found", *error = NO_NAME; return);
            size_t namelen = strlen(tmpname);
            if (namelen > parsed_header->nameParam->bufferSize) {
                // Può accadere se il nome o se la lunghezza del buffer instanziato (@link config.h, server.c)  
                // non rispettano lo standard POSIX per la lunghezza
                *error = BUFFER_OVERFLOW;
                return;
            }
            strncpy(parsed_header->nameParam->buffer, tmpname, namelen);
            parsed_header->nameParam->dataLen = namelen;
            break;
        }
        case UNKOWN:
        default: {
            return;
        }
    }
    // 3. Operazioni aggiuntive della richiesta di STORE
    if (parsed_header->cmd == STORE) {
        // 3.1 Parso il parametro len della richiesta
        char *tmplen, *tmpdata;
        CHECK_EQS_NOEXIT(tmplen = strtok_r(NULL, " ", &tmpstr), NULL, "[SERVER]: Parsing STORE header, len parameter not found", *error = NO_LEN; return);
        CHECK_EQS_NOEXIT(parsed_header->dataLength = isNumber(tmplen), -1, "[SERVER]: Parsing STORE header, len parameter not a number", *error = MALFORMED_HEADER; return);
        #ifdef DEBUG 
        fprintf(stderr, "[DEBUG SERVER] Store, Letta lunghezza %ld\n", parsed_header->dataLength);
        #endif
        // 3.2 Estrapolo la parte rimasta di dati e li metto in un buffer temporaneo
        tmpdata = strtok_r(NULL, " ", &tmpstr);     // Consumo '\n'
        tmpdata = strtok_r(NULL, "", &tmpstr);      // Prendo i dati
        if (tmpdata == NULL) {
            #ifdef DEBUG
            fprintf(stderr, "[SERVER]: Parsing STORE header, dati non trovati");
            #endif
            return;
        }
        #ifdef DEBUG 
        fprintf(stderr, "[DEBUG SERVER] Store, Lettura di tempdata %s\n", tmpdata);
        #endif
        size_t datasize = strlen(tmpdata);
        // Controllo che non siano arrivati più dati di quanti debbano effettivamente arrivare
        // N.B. controllo per la sicurezza!
        if (datasize > parsed_header->dataLength) {
            *error = MALFORMED_HEADER;
            return;
        }
        #ifdef DEBUG 
        fprintf(stderr, "[DEBUG SERVER] Store, Len dei dati letti dall'header %ld\n", datasize);
        #endif
        // Se ho spazio memorizzo i dati nel buffer passato
        if (datasize > dataBuffer->bufferSize) {
            // Può accadere se il buffer per i dati in eccesso non è grande almeno quanto il buffer di lettura
            // dell'header dalla socket (@link server.c, config.h)
            // N.B. se si riscontra questo errore, basta settare il buffer remainingData di 
            // lunghezza == a quella di lettura dell'header dalla socket (@link server.c)
            *error = BUFFER_OVERFLOW;
            return;
        }
        strncpy(dataBuffer->buffer, tmpdata, datasize);
        dataBuffer->dataLen = datasize;
        #ifdef DEBUG
        fprintf(stderr, "[DEBUG SERVER] Dati rimasti nel buffer dell'header: %s\n", dataBuffer->buffer);
        #endif
    }
}


void getResponseHeader(int socket_fd, responseHeader_t *parsedHeader, dataBuffer_t *remainingData, headerParsingError_t *error) {
    parsedHeader->response_cmd = UNKNOWN_R;
    parsedHeader->data_length = 0;
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG CLIENT]: getResponseHeader called\n");
    #endif
    int retval = 0;
    char rawresponse[BUFFER_RESPONSE_SIZE] = {'\0'};
    CHECK_EQS_NOEXIT(retval = read(socket_fd, rawresponse, BUFFER_RESPONSE_SIZE - 1), -1, "[CLIENT]: Reading rawresponse\n", *error = READ_ERROR; return);
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG CLIENT]: Letti %d in risposta dal server: %s\n", retval, rawresponse);
    #endif
    // Verify if \n is in the string
    int hasnewline = strchr(rawresponse, '\n') != NULL;
    // 1. Estrapolo il cmd
    char *svptr = NULL, *cmd = NULL;
    CHECK_EQS_NOEXIT(cmd = strtok_r(rawresponse, " ", &svptr), NULL, "[CLIENT]: Parsing response header, cmd not found", *error = NO_COMMAND; return);       
    parsedHeader->response_cmd = strResponseCommandToEnum(cmd, error);
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG CLIENT]: Response cmd: %s\n", cmd);
    #endif

    switch (parsedHeader->response_cmd) {
        case KO: {
            char *tmpmessage = NULL;
            if (!hasnewline) {
                // 2.0 Copio la parte rimasta di messaggio
                char *rawptr = NULL;
                rawptr = strtok_r(NULL, "\n", &svptr);
                #ifdef DEBUG
                fprintf(stderr, "[DEBUG CLIENT]: Response error message: %s", rawptr);
                #endif
                copyInDataBuffer(&parsedHeader->error_message, rawptr, strlen(rawptr));
                // 2.1 Devo consumare tutto il messaggio dal server
                while (!hasnewline) {
                    // Devo consumare tutto il messaggio dalla socket
                    // Copio il messaggio nel buffer finché ho spazio, se non ho pià spazio viene tagliato
                    char rawresponse2[BUFFER_RESPONSE_SIZE];
                    CHECK_EQS_NOEXIT(retval = read(socket_fd, rawresponse2, BUFFER_RESPONSE_SIZE), -1, "[CLIENT]: Reading Register error message\n", *error = READ_ERROR; return);
                    if (retval == 0) {
                        #ifdef DEBUG
                        fprintf(stderr, "[DEBUG CLIENT] Reading error, got EOF from socket\n");
                        #endif
                        *error = READ_ERROR;
                        return;
                    }
                    hasnewline = strchr(rawresponse2, '\n') != NULL;
                    rawptr = strtok_r(rawresponse2, "\n", &svptr);
                    #ifdef DEBUG
                    fprintf(stderr, "[DEBUG CLIENT]: Response error message: %s", rawptr);
                    #endif
                    if (retval > 0) {
                        copyInDataBuffer(&parsedHeader->error_message, rawptr, retval);
                    }
                }
            } else {
                // Se avevo già il \n allora non devo continuare a leggere perché ho letto già tutto il messaggio dalla socket
                // 2. Estrapolo il messaggio
                tmpmessage = strtok_r(NULL, "\n", &svptr);
                if (tmpmessage != NULL) {
                    copyInDataBuffer(&parsedHeader->error_message, tmpmessage, strlen(tmpmessage));
                }
            }
            break;
        }
        case DATA: {
            #ifdef DEBUG
            if (remainingData == NULL) {
                fprintf(stderr, "[DEBUG CLIENT]: messages.h: You have probably called getResponseHeader with wrong parameters: remaingData is NULL\n");
                *error = BUFFER_OVERFLOW;
                return;
            }
            #endif
            // 2. Estraggo la lunghezza
            char *tmp = NULL;
            CHECK_EQS_NOEXIT(tmp = strtok_r(NULL, " ", &svptr), NULL, "[CLIENT]: Parsing DATA header, len not found", *error = NO_LEN; return);       
            CHECK_EQS_NOEXIT(parsedHeader->data_length = isNumber(tmp), (-1), "[CLIENT]: Parsing DATA header, len not a number", *error = MALFORMED_HEADER; return);
            #ifdef DEBUG
            fprintf(stderr, "[DEBUG CLIENT]: Parsing DATA header, len: %ld \n", parsedHeader->data_length);
            #endif
            // 3. Estrapolo la parte rimasta di dati dentro e li metto in un buffer temporaneo
            tmp = strtok_r(NULL, " ", &svptr); // Consumo \n
            tmp = strtok_r(NULL, "", &svptr); // Leggo i dati rimasti
            if (tmp != NULL) {
                size_t datasize = strlen(tmp);
                if (datasize > remainingData->bufferSize) {
                    *error = BUFFER_OVERFLOW;
                    return;
                }
                strncpy(remainingData->buffer, tmp, datasize);
                remainingData->dataLen = datasize;
                #ifdef DEBUG
                fprintf(stderr, "[DEBUG CLIENT]: dati contenuti nell'header: %s", remainingData->buffer);
                #endif
            }
            break;
        }
        case OK: 
        default: {
            return;
        }
    }
}



// PRIVATE FUNCTIONS
/*
 Transforma le stringhe dei comandi in rappresentazione interna
 Se il comando non è riconosciuto setta error a UNKNOWN_COMMAND e restituisce UNKNOWN
*/
static requestCommand_t strCommandToEnum(char *strCommand, headerParsingError_t* error) {
    if (strcmp(strCommand, "LEAVE") == 0) return LEAVE;
    if (strcmp(strCommand, "REGISTER") == 0) return REGISTER;
    if (strcmp(strCommand, "STORE") == 0) return STORE;
    if (strcmp(strCommand, "RETRIEVE") == 0) return RETRIEVE;
    if (strcmp(strCommand, "DELETE") == 0) return DELETE;
    *error = UNKOWN_COMMAND;
    return UNKOWN;
} 

/*
 Transforma le stringhe delle risposte in rappresentazione interna
 Se il comando non è riconsciuto setta error a UNKNOWN_COMMAND e restituisce UNKNOWN_R
*/
static responseType_t strResponseCommandToEnum(char *strCommand, headerParsingError_t *error) {
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG CLIENT]: cmd %s\n", strCommand);
    #endif
    if (strcmp(strCommand, "OK") == 0) return OK;
    if (strcmp(strCommand, "KO") == 0) return KO;
    if (strcmp(strCommand, "DATA") == 0) return DATA;
    // Se non riconosco il comando
    *error = UNKOWN_COMMAND;
    return UNKNOWN_R;
}

/*
    Copia in dataBuffer da rawdata finché è disponibile spazio, altrimenti ignora l'operazione
*/
static void copyInDataBuffer(dataBuffer_t *dataBuffer, char *rawdata, int len) {
    size_t remainig_space = (dataBuffer->bufferSize - dataBuffer->dataLen) - 1;
    size_t max_cp_size = len >= remainig_space ? remainig_space : len;
    // Copio nel buffer finché ho spazio, se non ho pià spazio viene tagliato
    strncat(dataBuffer->buffer, rawdata, max_cp_size);
    // Aggiorno dimensione dei dati nel buffer    
    dataBuffer->dataLen += max_cp_size;
}

/*
    Converte una stringa in numero
*/
static long isNumber(char *s) {
   char* e = NULL;
   long val = strtol(s, &e, 0);
   if (e != NULL && *e == (char)0) return val; 
   return -1;
}