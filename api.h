#ifndef _OBJECT_STORE_API_H
#define _OBJECT_STORE_API_H

#include "./config.h"
#include "./myerror.h"
#include "./messages.h"
#include <sys/un.h>
#include <sys/socket.h>
#include <stddef.h>

/*  Inizia la connessione all'object store, registrando il cliente con il name dato. 
    Restituisce true se la connessione ha avuto successo, false altrimenti.
    Notate che la connessione all'object store è globale per il client.
*/
extern int os_connect(char *name);

/*  Richiede all'object store la memorizzazione dell'oggetto puntato da block,
    per una lunghezza len, con il nome name. 
    Restituisce true se la memorizzazione ha avuto successo, false altrimenti.
*/
extern int os_store(char *name, void *block, size_t len);

/*  Recupera dall'object store l'oggetto precedentementememorizzatato sotto il nome name. 
    Se il recupero ha avuto successo, restituisce un puntatore a un blocco di memoria,
    allocato dalla funzione, contenente i dati precedentemente memorizzati. 
    In caso di errore, restituisce NULL.
*/
extern void* os_retrieve(char* name);

/*  Cancella l'oggetto di nome name precedentemente memorizzato.
    Restituisce true se la cancellazione ha avuto successo, false altrimenti. 
*/
extern int os_delete(char *name);

/*
    Chiude la connessione all'object store. 
    Restituisce true se la disconnessione ha avuto successo, false in caso contrario.
*/
extern int os_disconnect();

#endif