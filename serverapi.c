#include "serverapi.h"

/* Struttura dati e mutex globale per la raccolta di dati 
   da stampare alla ricezione di SIGUSR1
*/
pthread_mutex_t mutex_stats = PTHREAD_MUTEX_INITIALIZER;
statistics_t stats;

// Mutex globale e variabile condizione con contatore dei clients attivi
static pthread_cond_t active_threads_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t active_threads_mutex = PTHREAD_MUTEX_INITIALIZER;
unsigned long active_threads_counter = 0;


// ===================
// Auxiliary functions prototypes
static void createUserPath(char *buff, char *username);
static void createFilePath(char *buff, char *username, char *filename);
static void getDirCounters(char *path, statistics_t *server_stat);
// mutex stats
static int checkIfOnline(const char *username);
static void addClientOnline(const char *username);
static int removeClientOnline(const char *username);
static int removeFile(char *path);
static void addFile(size_t file_size);



// ===================
// Extern functions implementation

void writeToClient(long fd, void *buff, size_t size, workerCmdError_t* error) {
    if (writen(fd, buff, size) == -1) {
        *error = COMMUNICATION_ERROR;
        #ifdef DEBUG
        fprintf(stderr, "[SERVER] writeToClient, communication error\n");
        #endif
    }
}


void registerFun(requestHeader_t *header, workerCmdError_t *error) {

    #ifdef DEBUG
    fprintf(stderr, "[DEBUG SERVER]: Register Function Called\n");
    #endif
    
    *error = NO_SERVERW_ERROR;

    #ifdef DEBUG
    if (header->cmd != REGISTER) {
        *error = COMMAND_MISMATCH;
        return;
    }
    #endif

    // 0.1 Se non c'è creo la cartella in cui fare la store 
    // dei dati per utente
    int tmperror = 0;
    tmperror = mkdir(STORE_PATH, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (tmperror == -1 && errno != EEXIST) {
        *error = STORE_DIRECTORY_UNAVAILABLE;
        return;
    }
    
    // 0.2 Creo cartella utente
    char *username = header->nameParam->buffer;
    char userpath[BUFFER_PATH_SIZE] = {'\0'};
    createUserPath(userpath, username);
    #if defined DEBUG
    fprintf(stderr, "[DEBUG SERVER]: Register Function user path %s\n", userpath);
    #endif

    tmperror = mkdir(userpath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (tmperror == -1 && errno != EEXIST) {
        *error = CLIENT_DIRECTORY_UNAVAILABLE;
        return;
    }
    
    if (checkIfOnline(username)) {
        *error = CLIENT_ALREADY_IN_USE;
        return;
    } 
    addClientOnline(username);
}

void leaveFun(requestHeader_t *header, char* username, workerCmdError_t *error) {
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG SERVER]: Leave Function Called %s\n", username);
    if (header->cmd != LEAVE) {
        *error = COMMAND_MISMATCH;
        return;
    }
    #endif

    if (removeClientOnline(username) == -1) {
        // Errore interno da gestire con stop del server
        *error = SHARED_DATA_ERROR;
        return;
    };
}

void storeFun(int socket_fd, requestHeader_t *header, char *username, dataBuffer_t *buff, workerCmdError_t *error) {
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG SERVER]: %s, %d StoreFun Function Called %s, %ld, %s\n", username, socket_fd, header->nameParam->buffer, header->dataLength, buff->buffer);
    if (header->cmd != STORE) {
        *error = COMMAND_MISMATCH;
        return;
    } 
    #endif  
    // Costruisco il path del file
    char filepath[BUFFER_PATH_SIZE]  = {'\0'};
    createFilePath(filepath, username, header->nameParam->buffer);

    // Apro in scrittura, eventualmente sovrascrivo il file esistente
    // L'utente deve avere l'accortezza di usare retrieve sul file nello store se non vuole perderlo
    removeFile(filepath); // Serve nel caso in cui il file che si vuole memorizzare esista già
    int fd = 0;
    CHECK_EQS_NOEXIT(fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0666), (-1), "[SERVER]: Store: Aprendo il file", *error = DATA_ERROR; return);

    #ifdef DEBUG
    fprintf(stderr, "[DEBUG SERVER]: %s %d StoreFun: aperto il file %s\n", username, socket_fd, filepath);
    #endif
    // Scrivo la parte di dati che ho letto leggendo l'header
    size_t original_data_size = header->dataLength;
    if (buff->dataLen > 0) {
        write(fd, buff->buffer, buff->dataLen);
        header->dataLength -= buff->dataLen;
        buff->dataLen = 0;
        #ifdef DEBUG
        fprintf(stderr, "[DEBUG SERVER]: %s %d StoreFun -> scritto il chunck di dati proveniente dalla lettura dell'header \n%s\n", username, socket_fd, buff->buffer);
        #endif
    }
    // Finché non ho letto tutti i dati dalla socket continuo a scrivere
    while (header->dataLength > 0) {
        size_t nextbytes = header->dataLength > buff->bufferSize ? buff->bufferSize : header->dataLength; 
        #ifdef DEBUG
        fprintf(stderr, "[DEBUG SERVER]: %s %d StoreFun -> mi metto in attesa della scrittura dei dati %ld\n",  username, socket_fd, nextbytes);
        #endif
        buff->dataLen = read(socket_fd, buff->buffer, nextbytes);
        if (buff->dataLen <= 0) {
            #ifdef DEBUG
            fprintf(stderr, "[DEBUG SERVER]: %s %d StoreFun ->Letti %ld su %ld, break\n",  username, socket_fd, buff->dataLen, nextbytes);  
            #endif
            *error = COMMUNICATION_ERROR;
            close(fd);
            unlink(filepath);
            return;
        } 
        #ifdef DEBUG
        fprintf(stderr, "[DEBUG SERVER]: %s %d StoreFun ->Letti %ld su %ld\n",  username, socket_fd, buff->dataLen, nextbytes);
        #endif
        write(fd, buff->buffer, buff->dataLen);
        header->dataLength -= buff->dataLen;
        buff->dataLen = 0;
        #ifdef DEBUG
        fprintf(stderr, "[DEBUG SERVER]: %s %d StoreFun -> scritto il chunck di dati proveniente dalla lettura della socket \n%s\n",  username, socket_fd, buff->buffer);
        #endif
    }

    #ifdef DEBUG
    fprintf(stderr, "[DEBUG SERVER]: %s %d StoreFun -> Finita la scrittura, chiudo il file\n", username, socket_fd);
    #endif
    close(fd);
    addFile(original_data_size);
}

void retrieveFun(int socket_fd, requestHeader_t *header, char* username, workerCmdError_t *error) {
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG SERVER]: Retrieve Function Called\n");     
    if (header->cmd != RETRIEVE) {
        *error = COMMAND_MISMATCH;
        return;
    }
    #endif
    
    char filepath[BUFFER_PATH_SIZE] = {'\0'};
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG SERVER]: RetrieveFun: Creo il path per il file %s\n", header->nameParam->buffer);
    #endif
    createFilePath(filepath, username, header->nameParam->buffer);
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG SERVER]: RetrieveFun: Provo ad aprire il file %s\n", filepath);
    #endif
    // Apro il file in lettura
    int fd = 0;
    CHECK_EQS_NOEXIT(fd = open(filepath, O_RDONLY), (-1), "[DEBUG SERVER]: RetrieveFun: Errore nell'apertura del file", *error = FILE_NOT_FOUND; return);
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG SERVER]: RetrieveFun: Aperto il file %s\n", filepath);
    #endif
    // Trovo la lunghezza del file
    struct stat s;
    CHECK_EQS_NOEXIT(fstat(fd, &s), (-1), "[DEBUG SERVER]: Retrieve Fun: cercando lunghezza del file", *error = DATA_ERROR; return);
    long int file_len = s.st_size;
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG SERVER]: RetrieveFun: lunghezza del file %ld\n", file_len);
    #endif
    // Inizio a leggere e scrivere sulla socket i dati
    char _response_buffer[BUFFER_SIZE] = {'\0'};
    dataBuffer_t response_buffer = {
        .buffer = _response_buffer,
        .bufferSize = BUFFER_SIZE,
        .dataLen = 0
    };
    response_buffer.dataLen = snprintf(response_buffer.buffer, response_buffer.bufferSize - 1, "DATA %ld \n ", file_len);
     #ifdef DEBUG
        fprintf(stderr, "[DEBUG SERVER]: RetrieveFun: Lunghezza header %ld\n", response_buffer.dataLen);
     #endif
    if (response_buffer.dataLen < 0) {
        #ifdef DEBUG
        fprintf(stderr, "[DEBUG SERVER]: RetrieveFun: Errore nella creazione dell'header %s %ld\n", response_buffer.buffer, response_buffer.dataLen);
        #endif
        *error = DATA_ERROR;
        close(fd);
        return;
    }
    if (writen(socket_fd, response_buffer.buffer, response_buffer.dataLen) == (-1)) {
        *error = COMMUNICATION_ERROR;
        close(fd);
        return;
    }
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG SERVER]: Ho scritto l'header della risposta \n%s", response_buffer.buffer);
    #endif
    
    while(file_len > 0) {
        size_t to_read = file_len > response_buffer.bufferSize ? response_buffer.bufferSize : file_len;
        response_buffer.dataLen = read(fd, response_buffer.buffer, to_read);
        if (response_buffer.dataLen == -1) {
            *error = DATA_ERROR;
            close(fd);
            return;
        }
        #ifdef DEBUG
        fprintf(stderr, "[DEBUG SERVER]: retrieve:letto dal file %ld bytes\n %s",response_buffer.dataLen, response_buffer.buffer);
        #endif
        int written = writen(socket_fd, response_buffer.buffer, response_buffer.dataLen);
        if (written < 0) {
            *error = COMMUNICATION_ERROR;
            close(fd);
            return;
        }
        #ifdef DEBUG
        fprintf(stderr, "[DEBUG SERVER]: retrieve: scritto su socket %d\n", written);
        #endif
        file_len -= response_buffer.dataLen;
    }

    #ifdef DEBUG
    fprintf(stderr, "[DEBUG SERVER]: retrieve: finito di mandare il file\n");
    #endif
    close(fd);
}

void deleteFun(requestHeader_t *header, char* username, workerCmdError_t *error) {
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG SERVER]: Delete Function Called\n");
    if (header->cmd != DELETE) {
        *error = COMMAND_MISMATCH;
        return;
    }
    #endif

    *error = NO_SERVERW_ERROR;

    char buff[BUFFER_PATH_SIZE] = {'\0'};
    dataBuffer_t filepath = {
        .buffer = buff,
        .dataLen = 0,
        .bufferSize = BUFFER_PATH_SIZE 
    };
    createUserPath(filepath.buffer, username);
    filepath.dataLen += strlen(filepath.buffer);
    DIR * d = opendir(filepath.buffer);
    if (d == NULL) {
        *error = CLIENT_DIRECTORY_UNAVAILABLE;
        return;
    }

    struct dirent * dir;
    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
            continue;
        }

        if (strcmp(dir->d_name, header->nameParam->buffer) == 0) {
            size_t len = filepath.dataLen + strlen(header->nameParam->buffer);
            if (filepath.bufferSize > len) {
                strncat(filepath.buffer, header->nameParam->buffer, strlen(header->nameParam->buffer));
                filepath.dataLen = len;
                int res = removeFile(filepath.buffer);
                if (res != 0) {
                    *error = SHARED_DATA_ERROR;
                }
                closedir(d);
                return;
            } else {
                *error = PATH_NAME_ERROR;
                closedir(d);
                return;
            }
        }
    }

    // Se non ho trovato il file nella cartella
    closedir(d);
    *error = FILE_NOT_FOUND;
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG SERVER]: FILE NOT FOUND\n");
    #endif
}


void initializeStatistics() {

    #if defined DEBUG || defined DEBUG_STATS
    fprintf(stderr, "[DEBUG SERVER] initializeStatistics called\n");
    #endif

    pthread_mutex_lock(&mutex_stats);

    stats.nusers               = 0;
    stats.nonline              = 0;
    stats.nfiles               = 0;
    stats.dimension            = 0;
    stats.allclients           = createList();
    stats.onlineclients        = createList();

    // se non c'è la cartella dello store la creo
    int tmperror = 0;
    tmperror = mkdir(STORE_PATH, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    // Se ho un errore fallisco (a meno che la cartella non esista già)
    if (tmperror == -1 && errno != EEXIST) {
        exit(EXIT_FAILURE);
    }

    getDirCounters(STORE_PATH, &stats); 

    pthread_mutex_unlock(&mutex_stats);
}

void printStatistics(FILE *fd) {
    #ifdef DEBUG
    fprintf(stderr, "[DEBUG SERVER] Printing statistics\n");
    #endif

    FILE *iostream = fd != NULL ? fd : stderr;

    pthread_mutex_lock(&mutex_stats);

    fprintf(iostream, "\n===============\nSERVER STATS\n");
    fprintf(iostream, "-> Log time: \t\t\t\t\t%lu\n", (unsigned long)time(NULL));
    fprintf(iostream, "-> N. utenti totali:\t\t\t\t%lu\n", stats.nusers);
    fprintf(iostream, "-> N. utenti online:\t\t\t\t%lu\n", stats.nonline);
    fprintf(iostream, "-> N. oggetti totale:\t\t\t\t%lu\n", stats.nfiles);
    fprintf(iostream, "-> Dimensione totale degli oggetti:\t\t%lu\n", stats.dimension);
    fprintf(iostream, "-> Lista di tutti gli utenti:\n");
    printList(stats.allclients, iostream);
    fprintf(iostream, "-> Lista degli utenti online:\n");
    printList(stats.onlineclients, iostream);

    pthread_mutex_unlock(&mutex_stats);
}


void cleanup(void) {
    // Controllo per essere sicuro che non venga chiamata più volte
    static int called = 0;
    if (!called) {
        unlink(SOCKNAME);
        pthread_mutex_lock(&mutex_stats);
        destroyList(stats.allclients);
        destroyList(stats.onlineclients);
        pthread_mutex_unlock(&mutex_stats);
        pthread_mutex_destroy(&mutex_stats);
        pthread_mutex_destroy(&active_threads_mutex);
        pthread_cond_destroy(&active_threads_cond);
        called = 1;
    }
}

void decrActiveThreads() {
    pthread_mutex_lock(&active_threads_mutex);
    --active_threads_counter;
    pthread_cond_signal(&active_threads_cond);
    pthread_mutex_unlock(&active_threads_mutex);
}

void waitActiveThreads() {
    pthread_mutex_lock(&active_threads_mutex);
    while (active_threads_counter > 0) pthread_cond_wait(&active_threads_cond, &active_threads_mutex);
    pthread_mutex_unlock(&active_threads_mutex);
}


// ========================
// PRIVATE FUNCTIONS

/* Scrive su buff il path dello storage per l'utente username */
static void createUserPath(char *buff, char *username) {
    strncpy(buff, STORE_PATH, strlen(STORE_PATH));
    strncat(buff, username, strlen(username));
    strncat(buff, "/", 1);
}

/* Scrive su buff il percorso del file (nella dir username) */
static void createFilePath(char *buff, char *username, char *filename) {
    createUserPath(buff, username);
    strncat(buff, filename, strlen(filename));
}

/* Legge le directory con percorso path collezionando le informazioni in stats
   Ci si aspetta che la directory abbia un primo livello di sottodirectory e in ognuna solo regular files
*/
static void getDirCounters(char *path, statistics_t *server_stat) {

    #ifdef DEBUG
    fprintf(stderr, "[DEBUG SERVER]: getDirCounters called\n");
    #endif
    DIR * d;
    CHECK_EQS(d = opendir(path), NULL, opendir);
    struct dirent * dir;
    // 1. Leggo tutte le directory dei client
    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;
        #ifdef DEBUG
        fprintf(stderr, "[DEBUG SERVER]: Loop over store dir entries, current dirname %s\n", dir->d_name);
        #endif
        struct stat s;
        char idirname[BUFFER_PATH_SIZE] = {'\0'};
        createUserPath(idirname, dir->d_name);
        #ifdef DEBUG
        fprintf(stderr, "[DEBUG SERVER]: current param to stat %s\n", idirname);
        #endif
        CHECK_EQS(stat(idirname, &s), (-1), stat);
        #ifdef DEBUG
        fprintf(stderr, "[DEBUG SERVER]: stat verifica Directory client eseguita\n");
        #endif
        // 1.2 controllo che sia una directory,
        // la aggiungo nella lista dei nomi dei client
        if (S_ISDIR(s.st_mode)) {
            #ifdef DEBUG
            fprintf(stderr, "[DEBUG SERVER]: è una directory: %s\n", dir->d_name);
            #endif
            insertList(server_stat->allclients, dir->d_name);
            server_stat->nusers++;
            #ifdef DEBUG
            printList(server_stat->allclients, stderr);
            #endif
           
            // 2. leggo tutti i file del client
            DIR * d_client;
            CHECK_EQS(d_client = opendir(idirname), NULL, opendir);
            struct dirent * dir_client;
            while ((dir_client = readdir(d_client)) != NULL) {
                if (strcmp(dir_client->d_name, ".") == 0 || strcmp(dir_client->d_name, "..") == 0) continue;
                char buffer[BUFFER_PATH_SIZE] = {'\0'};
                strncat(buffer, idirname, BUFFER_PATH_SIZE);
                strncat(buffer, dir_client->d_name, BUFFER_PATH_SIZE);
                #ifdef DEBUG
                fprintf(stderr, "[DEBUG SERVER]: file %s\n", buffer);
                #endif
                struct stat s;
                CHECK_EQS(stat(buffer, &s), (-1), stat);
                if (S_ISREG(s.st_mode)) {
                    server_stat->nfiles++;
                    server_stat->dimension += s.st_size;
                } else {
                    perror("not a regular file");
                    exit(EXIT_FAILURE);
                }
            }
            closedir(d_client);        
        } else {
            // Se trovo qualcosa di diverso da una directory nella cartella store c'è stato un errore
            exit(EXIT_FAILURE);
        }

       
    }
    closedir(d); // finally close the directory
}

void forceRemoveClientOnline(const char *username) {
    removeClientOnline(username);
}

// =====================================
// Funzioni ausiliarie di accesso controllato
// alle strutture dati condivise per le statistiche del server
static int checkIfOnline(const char *username) {
    int res = 0;
    pthread_mutex_lock(&mutex_stats);
    res = isInList(stats.onlineclients, username);
    pthread_mutex_unlock(&mutex_stats);
    return res;
}

static void addClientOnline(const char *username) {
    pthread_mutex_lock(&mutex_stats);
    if (!isInList(stats.allclients, username)) {
        insertList(stats.allclients, username);  
        stats.nusers++;     
    }
    insertList(stats.onlineclients, username);
    stats.nonline++;
    pthread_mutex_unlock(&mutex_stats);
}

static int removeClientOnline(const char *username) {
    int res = 0;
    pthread_mutex_lock(&mutex_stats);
    res = deleteList(stats.onlineclients, username);
    if (res == 0) {
        stats.nonline--;
    }
    pthread_mutex_unlock(&mutex_stats);
    return res;
}

static int removeFile(char *filepath) {
    int res = 0;
    struct stat s;
    pthread_mutex_lock(&mutex_stats);
    res = stat(filepath, &s);
    if (res != (-1)) {
        // In modo atomico rimuovo il file e nel caso in cui 
        // l'operazione abbia successo modifico le stats
        res = unlink(filepath);
        if (res == 0) {
            // modifico le stats solo se è andata a buon fine l'operazione
            stats.nfiles--;
            stats.dimension -= s.st_size;
        }
    }
    pthread_mutex_unlock(&mutex_stats);
    return res;
}


static void addFile(size_t file_size) {
    pthread_mutex_lock(&mutex_stats);
    stats.nfiles++;
    stats.dimension += file_size;
    pthread_mutex_unlock(&mutex_stats);
}

