#ifndef _LISTA_H_
#define _LISTA_H_   

#include <errno.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct elem {
  char *key;       
  struct elem *next; 
} elem_t;
 
// tipo della funzione utilizzata per fare il compare
typedef int (*compare_t)(const char *, const char *);

typedef struct {
    /** la testa della lista */
    elem_t *head;
    /** questa e' la funzione per confrontare due chiavi */
    compare_t compare;
} list_t;

// Inizializza una nuova lista
list_t *createList();
// Distrugge una lista
int     destroyList(list_t *);
// Inserisce secondo ordine della compare un elemento nella lista
int     insertList(list_t *, const char *);
// Rimuove un elemento dalla lista
int     deleteList(list_t *, const char *);
// Ritorna true se l'elemento appartiene alla lista
int     isInList(list_t *, const char *);
// Stampa tutti gli elementi della lista
void    printList(list_t *, FILE *fd);

#endif /* LISTA_H_ */