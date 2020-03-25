/* 
    Header di configurazione
*/
#ifndef _CONFIG_H_
#define _CONFIG_H_

#define SOCKNAME                "./objstore.sock"
#define SOCKMAXCONN             32                      
#define STORE_PATH              "./data/"
#define UNIX_PATH_MAX           108
#define MAXCONNECTIONS          64
#define CHECK_TIMEUOT           10000   // Timeout in microseconds, usato per le select in modo da controllare se è arrivato un segnale
                                        // e per l'internvallo di controllo sullo shutdown dei threads (euristica di ottimizzazione)

#define BUFFER_SIZE             1024    // Buffer generico e per scambio di dati
#define BUFFER_NAME_SIZE        512     // Buffer per un nome (Deve essere almeno 256 -> standard POSIX per i nomi)
#define BUFFER_PATH_SIZE        4096    // Buffer per il path dei file
#define BUFFER_RESPONSE_SIZE    256     // DATA len, KO o OK e descrizione di errore dal server

// ======================
// Test definitions
#define MAX_OBJ_DIM             100000  // Massima dimensione di un oggetto da memorizzare nello store con i test
#define OBJ_CHUNCK_SIZE         5000    // Incremento di dimensione degli oggetti da memorizzare
#define OBJ_NUMBER              20      // Numero di oggetti da creare, ogni oggetto ha dimensioni +=OBJ_CHUNK_SIZE dal precedente, fino a MAX_OBJ_DIM

#endif