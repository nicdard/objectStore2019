// To avoid intellisense problems with visual studio
#define _GNU_SOURCE 

#include "serverapi.h"
#include <signal.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>


// Funzione del thread worker
static void *workerFunction(void *arg);
// Crea un thread e lo mette in ascolto di un dato client
static void createWorker(int* cfd);

// Gestore dei segnali (SIGUSR1, SIGTERM, SIGKILL...)
// Quando riceve un qualunque segnale setta la variabile di fine ciclo per stoppare il server che esegue il cleanup (registrato con atexit)
// Se riceve SIGUSR1 stampa a schermo prima di chiudere il thread le informazioni statistiche
static void *sighandler(void* arg);
// Variabile di fine ciclo del server, non protetta da mutex
// perché scritta una sola volta dal thread dei segnali e letta da tutti gli altri
// che la controllano periodicamente, non serve sincornizzazine precisa
// Voglio atomicità fra i processi
static volatile sig_atomic_t loop_breaker = 0;
// Variabile bool che indica un errore non gestibile
// una volta settata a true chiude tutti i processi chiamando la funzione di cleanup
// e ritornando FAILURE come exit status
// Voglio atomicità fra i processi
static volatile sig_atomic_t global_failure = 0;


int main() {
    // Registro la funzione di cleanup
    atexit(cleanup);
    // =======
    // Segnali
    sigset_t sigmask;
    // Blocco i segnali nella mashera dei clients (viene ereditata)
    sigfillset(&sigmask);
    CHECK_NEQS(pthread_sigmask(SIG_BLOCK, &sigmask, NULL), 0, "[SERVER]: Errore pthread_sigmask");
    // Prima di qualsisi altra cosa, subito dopo aver bloccato i segnali nel main, non safe (utilizza getDirCounters per ls dei file nella cartella data)
    initializeStatistics();
    // Creo un solo thread gestore dei segnali dove faccio sigwait per controllarli
    pthread_t sigthread;
    pthread_attr_t sigthattr;
    CHECK_NEQS(pthread_attr_init(&sigthattr), 0, "[SERVER]: Errore inizializzazione di attributi");
    CHECK_NEQS(pthread_attr_setdetachstate(&sigthattr, PTHREAD_CREATE_DETACHED), 0, "[SERVER]: Errore nel settaggio degli attributi del thread gestore dei segnali");
    CHECK_NEQS(pthread_create(&sigthread, &sigthattr, &sighandler, (void *) &sigmask), 0, "[SERVER]: Errore nella creazione del thread dei segnali");
    pthread_attr_destroy(&sigthattr);
    // =======
    // Connessioni
    // Creo indirizzo
    struct sockaddr_un sa;
    strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;
    // Creo la socket e mi metto in ascolto
    int sfd = -1;
    CHECK_EQS((sfd = socket(AF_UNIX, SOCK_STREAM, 0)), -1, "[SERVER]: Errore creazione del socket");
    CHECK_EQS(bind(sfd, (struct sockaddr *) &sa, sizeof(sa)), -1, "[SERVER]: Errore binding del socket");
    CHECK_EQS(listen(sfd, SOCKMAXCONN), -1, "[SERVER]: Errore socket listen");
    
    // Rimango in attesa di un client e se mi arriva una 
    // connessione creo un worker servente
    // Tutto con select con timeout in modo da controllare se ho ricevuto un segnale
    // e poter terminare il main anche se in attesa sulla accept

    // Timeout 10ms
    struct timeval timeout;
    // Readset, contiene solo il sfd per la accept
    fd_set readfds;
    // Finché non ricevo un segnale o non si verifica un errore globale non gestibile sto in ascolto delle connessioni
    // e spawno i thread serventi
    while (!loop_breaker && !global_failure) {
        // Inizializzo nel ciclo perché la SC select modifica i valori del timeout e dei sets
        timeout.tv_sec = 0;
        timeout.tv_usec = CHECK_TIMEUOT;
        FD_ZERO(&readfds);
        FD_SET(sfd, &readfds);
        // Mi metto in attesa di connessioni al server
        CHECK_EQS(select(sfd+1, &readfds, NULL, NULL, &timeout), -1, "[SERVER]: Errore nella select della accept");
        if (FD_ISSET(sfd, &readfds)) {
            int accepted_fd = (-1);
            int *socket_fd = NULL;
            // Alloco sulla heap per non avere problemi di memoria fra threads
            CHECK_EQS_NOEXIT(socket_fd = malloc(sizeof(int)), NULL, "[SERVER]: Errore nella malloc della accept", continue);
            // Se non riesco ad accettare semplicemente ignoro e mi rimetto in ascolto
            CHECK_EQS_NOEXIT(accepted_fd = accept(sfd, NULL, 0), (-1), "[SERVER]: Errore nella accept", continue);
            *socket_fd = accepted_fd;
            // Creo il thread servernte del nuovo client
            createWorker(socket_fd);
        }

    }

    // Chiudo e esco
    close(sfd);
    // Mi metto in attesa in modo che tutti i threads possano terminare correttamente le loro operazione
    waitActiveThreads();
    //Wait for all the worker threads to shutdown
    if (global_failure) {
        // Segnalo errore
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
    return 0;
}


static void createWorker(int* cfd) {
    pthread_attr_t thattr;
    pthread_t thid;
    if (pthread_attr_init(&thattr) != 0) {
        perror("Attempting to create a worker: pthread attribute init failed");
        close(*cfd);
        return;
    }
    // Voglio evitare di salvarmi i thread_id e joinarli in chiusura
    if (pthread_attr_setdetachstate(&thattr, PTHREAD_CREATE_DETACHED) != 0) {
        perror("Attempting to create a worker: pthread attribute setdetached failed");
        close(*cfd);
        return;
    }
    // In modo atomico creo il thread e nel caso incremento il contatore
    pthread_mutex_lock(&active_threads_mutex);
    if (pthread_create(&thid, &thattr, workerFunction, (void*)cfd) != 0) {
        perror("Attempting to create a worker: pthread create failed");
        pthread_mutex_unlock(&active_threads_mutex);
        close(*cfd);
        return;
    }
    active_threads_counter++;
    pthread_mutex_unlock(&active_threads_mutex);
}


static void *sighandler(void *arg) {
    sigset_t *set = arg;
    int sig;
    // Mi sospendo per ascoltare i segnali
    CHECK_NEQS(sigwait(set, &sig), 0, "[SERVER] Errore nel gestore dei segnali");
    // Ho ricevuto un segnale -> stoppo tutto all'uscita
    loop_breaker = 1;
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG SERVER] Il gestore dei segnali ha ricevuto %d\n", sig);
    #endif
    if (sig == SIGUSR1) {
        printStatistics(stdout);
    }  
    // cleanup and term N.B il cleanup viene fatto una sola volta dal main 
    return NULL;        
}

static void *workerFunction(void *arg) {
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG SERVER]: worker function called\n");
    #endif
    // Prendo il socket descriptor
    int socket_fd = 0;
    socket_fd = *((int *) arg);
    // Dealloco la memoria usata per passare il descrittore al thread
    free((int *) arg);
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG SERVER]: worker function socket descriptor = %d\n", socket_fd);
    #endif
    // Break interno per leave con successo o situazioni di errore gestibili: 
    volatile int own_breaker = 0;
    // Break interno per errori del thread non gestibili 
    // (Gli altri vengono gestiti mandando messaggio di errore al client senza stoppare il thread)
    volatile int own_failure = 0;
    
    // Timeout 10ms default
    struct timeval timeout;
    // Readset, contiene solo il sfd per la accept
    fd_set readfds;

    // Mi metto in ascolto su questo descrittore
    char userName[BUFFER_NAME_SIZE] = {'\0'};
    workerCmdError_t worker_error = NO_SERVERW_ERROR;
    // finché non devo killare a causa di un segnale o non ho un errore interno sto in ascolto sulla socket
    while (!loop_breaker && !global_failure && !own_breaker && !own_failure) {
        // Inizializzo nel ciclo perché la SC select modifica i valori del timeout e dei sets
        timeout.tv_sec = 0;
        timeout.tv_usec = CHECK_TIMEUOT;
        FD_ZERO(&readfds);
        FD_SET(socket_fd, &readfds);
        // Mi metto in ascolto per l'arrivo di comandi dal client
        // Se ho un errore in lettura setto variabile di fail e smetto di ascoltare
        CHECK_EQS_NOEXIT(select(socket_fd + 1, &readfds, NULL, NULL, &timeout), -1, "[SERVER]: Errore nella select del Thread servente", own_failure = 1; break);
        if (FD_ISSET(socket_fd, &readfds)) {
            // ========================
            // HEADER
            // Creo un buffer per la lettura dell'header grezzo (può contenere anche dati) dalla socket
            char rawBuffer[BUFFER_SIZE] = {'\0'};
            // Buffer dei dati in eccesso N.B ha la stessa lunghezza di rawBuffer 
            // per assicurare una corretta lettura di tutti i bytes rimanenti senza rischio di bufferoverflow
            // e conseguente chiusura della connessione 
            char _buffer[BUFFER_SIZE] = {'\0'};
            dataBuffer_t buffer = {
                .buffer = _buffer,
                .bufferSize = BUFFER_SIZE,
                .dataLen = 0
            };
            // Buffer del nome parsato
            char _name[BUFFER_NAME_SIZE] = {'\0'};
            dataBuffer_t nameBuffer = {
                .buffer = _name,
                .bufferSize = BUFFER_NAME_SIZE,
                .dataLen = 0
            };   
            // Header parsato         
            requestHeader_t parsedHeader = {
                .cmd = UNKOWN,
                .nameParam = &nameBuffer,
                .dataLength = 0
            };
            // Inizializzo la variabile di errore
            headerParsingError_t msg_error = NO_MSG_ERROR;
            int retval = 0;
            // Se ho un errore in lettura 
            CHECK_EQS_NOEXIT(retval = read(socket_fd, rawBuffer, BUFFER_SIZE), -1, "[SERVER]: Errore nella lettura della socket", continue);
            #ifdef DEBUG
            fprintf(stderr, "[DEBUG SERVER]: Letto header, bytes: %d\n", retval);
            #endif
            parseRequestHeader(rawBuffer, &parsedHeader, &buffer, &msg_error);

            if (msg_error == NO_MSG_ERROR)  {
                // Parsato con successo
                switch (parsedHeader.cmd) {
                    case REGISTER: {
                        registerFun(&parsedHeader, &worker_error);
                        switch (worker_error) {
                            case NO_SERVERW_ERROR: {
                                // Ho registrato il nuovo client all'object store, mi salvo il nome del client
                                // per gestire le chiamate alle varie funzionalità in questo thread
                                strncpy(userName, parsedHeader.nameParam->buffer, parsedHeader.nameParam->dataLen);
                                writeToClient(socket_fd, SIMPLE_OK_RESPONE, sizeof(char) * strlen(SIMPLE_OK_RESPONE), &worker_error);
                                #ifdef DEBUG
                                fprintf(stderr, "[DEBUG SERVER]: Register writing OK\n");
                                #endif
                                break;
                            }
                            case SHARED_DATA_ERROR:
                            case STORE_DIRECTORY_UNAVAILABLE: {
                                global_failure = 1; // Errore globale del server
                                #ifdef DEBUG
                                printf("[DEBUG SERVER] errore non gestibile nel thread servente %d, client %s, data directory can't be open", socket_fd, userName);
                                #endif
                                writeToClient(socket_fd, FATAL_KO_RESPONSE, sizeof(char) * strlen(FATAL_KO_RESPONSE), &worker_error);
                                break;
                            } 
                            case CLIENT_ALREADY_IN_USE: {
                                own_breaker = 1; 
                                // Stoppo questo thread, situazione di errore ma gestita senza provocare crash nel client a regola
                                // Fallendo la registrazione il client non dovrebbe procedere nel contattare il server

                                // Non posso registrare un client che è già online (da spec ogni client ha un nome suo personale)
                                writeToClient(socket_fd, CLIENT_IN_USE_KO_RESPONSE, sizeof(char) * strlen(CLIENT_IN_USE_KO_RESPONSE), &worker_error);
                                break;
                            } 
                            default: {
                                own_failure = 1; // Stoppo il thread
                                writeToClient(socket_fd, SIMPLE_KO_RESPONE, sizeof(char) * strlen(SIMPLE_KO_RESPONE), &worker_error);
                                break;
                            }
                        }
                        
                        break;
                    }
                    case LEAVE: {
                        leaveFun(&parsedHeader, userName, &worker_error);
                        if (worker_error == SHARED_DATA_ERROR) {
                            // Ho un errore non gestibile nel server
                            #ifdef DEBUG
                            printf("[DEBUG SERVER] errore globale non gestibile nel thread servente %d, client %s", socket_fd, userName);
                            #endif
                            global_failure = 1; 
                        } else {
                            own_breaker = 1; // Stoppo il thread, tutto è andato a buon fine
                        }
                        // Rispondo sempre ok al client come da protocollo
                        writeToClient(socket_fd, SIMPLE_OK_RESPONE, sizeof(char) * strlen(SIMPLE_OK_RESPONE), &worker_error);
                        break;
                    }
                    case DELETE: {
                        deleteFun(&parsedHeader, userName, &worker_error);
                        if (worker_error == NO_SERVERW_ERROR) {
                            #ifdef DEBUG
                            fprintf(stderr, "[DEBUG SERVER] %s %d Delete function, worker_error == NO_SERVERW_ERROR\n", userName, socket_fd);
                            #endif
                            writeToClient(socket_fd, SIMPLE_OK_RESPONE, sizeof(char) * strlen(SIMPLE_OK_RESPONE), &worker_error);
                            // Ok file cancellato
                        } else if (worker_error == CLIENT_DIRECTORY_UNAVAILABLE) {
                            own_failure = 1; 
                            // Preferisco considerarlo come un errore non gestibile del solo client
                            // TODO sarebbe bello fare un file di log
                            #ifdef DEBUG
                            fprintf(stderr, "[DEBUG SERVER]: %s %d Fatal error during DELETE execution -> A client (%s) is registered but its dir doesn't exists\n", userName, socket_fd, userName);
                            #endif
                            writeToClient(socket_fd, FATAL_KO_RESPONSE, sizeof(char) * strlen(FATAL_KO_RESPONSE), &worker_error);
                        } 
                        #ifdef DEBUG
                         else if (worker_error == SHARED_DATA_ERROR) {
                            global_failure = 1; // Segnalo un errore a livello di struttura dati condivisa
                            #ifdef DEBUG
                            fprintf(stderr, "[DEBUG SERVER] errore non gestibile nel thread servente %d, client %s\n", socket_fd, userName);
                            #endif
                            writeToClient(socket_fd, FATAL_KO_RESPONSE, sizeof(char) * strlen(FATAL_KO_RESPONSE), &worker_error);
                        
                        }  
                        #endif  
                        else if (worker_error == FILE_NOT_FOUND) {
                            #ifdef DEBUG
                            printf("[DEBUG SERVER]: %s %d file not found, writing on socket %s\n", userName, socket_fd, FILE_NOT_FOUND_KO_RESPONSE);
                            #endif
                            // Voglio far rimanere in piedi la connessione, rispondo solo al client
                            writeToClient(socket_fd, FILE_NOT_FOUND_KO_RESPONSE, sizeof(char) * strlen(FILE_NOT_FOUND_KO_RESPONSE), &worker_error);
                            #ifdef DEBUG
                            fprintf(stderr, "[DEBUG SERVER]: %s %d delete finished\n", userName, socket_fd);
                            #endif
                        } else {
                            //worker_error == PATH_NAME_ERROR
                            #ifdef DEBUG
                            printf("[DEBUG SERVER] %s %d DeleteFun: lunghezza del pathname del file da cancellare non supportata: client %s\n", userName, socket_fd, userName);
                            #endif
                            // Voglio far rimanere in piedi la connessione
                            // Se nello scrivere il messaggio si verifica un errore invece voglio chiudere la connessione
                            // Sotto controllo per tutti questa condizione
                            writeToClient(socket_fd, SIMPLE_KO_RESPONE, sizeof(char) * strlen(SIMPLE_KO_RESPONE), &worker_error);
                        }
                        break;
                    }
                    case STORE: {
                        storeFun(socket_fd, &parsedHeader, userName, &buffer, &worker_error);
                        if (worker_error == NO_SERVERW_ERROR) {
                            writeToClient(socket_fd, SIMPLE_OK_RESPONE, sizeof(char) * strlen(SIMPLE_OK_RESPONE), &worker_error);
                        } else {
                            writeToClient(socket_fd, SIMPLE_KO_RESPONE, sizeof(char) * strlen(SIMPLE_KO_RESPONE), &worker_error);
                        } 
                        break;
                    }
                    case RETRIEVE: {
                        // N.B. se va a buon fine la scrittura della risposta è gestita internamente
                        retrieveFun(socket_fd, &parsedHeader, userName, &worker_error);
                        if (worker_error != NO_SERVERW_ERROR) {
                            // C'è stato un errore
                            if (worker_error == FILE_NOT_FOUND) {
                                writeToClient(socket_fd, FILE_NOT_FOUND_KO_RESPONSE, sizeof(char) * strlen(FILE_NOT_FOUND_KO_RESPONSE), &worker_error);
                            } else {
                                writeToClient(socket_fd, SIMPLE_KO_RESPONE, sizeof(char) * strlen(SIMPLE_KO_RESPONE), &worker_error);
                            }
                        }
                        break;
                    }
                    default: {
                        break;
                    }
                }

                // Preferisco usare questo medoto per consistenza del codice (worker_error è la variabile usata internamente
                // dalle funzioni che contiene una descrizione più accurata dell'errore)
                // In questo punto controllo sempre se nel processo di risposta ho avuto in errore di comunicazione
                // (problema con la socket) nel caso setto la variabile di fine ciclo d'errore e stoppo perché la connessione ha problemi
                if (worker_error == COMMUNICATION_ERROR) {
                    // Questa condizione si verifica se nello scrivere la risposta al client qualcosa non va a buon fine
                    own_failure = 1;
                }
            } else {
                // Non sono riuscito a parsare l'header
                // Stampo l'errore
                perror_header_parser(msg_error, rawBuffer);
                // Signal error, close connection and kill thread
                own_failure = 1;
            }
        }
    }

    close(socket_fd);
    // Devo sempre rimuovere il client dalla lista di queli online quando stoppo il thread servente se c'è ancora
    forceRemoveClientOnline(userName);    
    // Subito prima di uscire decremento il contatore -> Se il server ha ricevuto un segnale aspetta finché non è 0 prima di chiudere e killare tutto
    decrActiveThreads(); 
    // Voglio però che non chiuda prima di aver stoppato tutte le esecuzioni a metà dei varii threads worker
    pthread_exit(NULL);
    // Così il compiltore non si lamenta
    return NULL;
}