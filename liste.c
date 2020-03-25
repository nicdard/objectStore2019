#include "liste.h"

// strdup non e' C99, e' necessario il prototipo
// altrimenti non viene riconosciuta dal compilatore
char *strdup(const char *s);

list_t *createList() {
    list_t *L = (list_t*) malloc(sizeof(list_t));
    if (L == NULL) {
	    perror("createList:malloc");
	    return NULL;
    }
    elem_t *p1 = malloc(sizeof(elem_t));
    if (p1 == NULL) {
	    perror("createList:malloc");
	    return NULL;
    }
    p1->key         = NULL;
    p1->next        = NULL;
    L->head         = p1;
    L->compare      = strcmp;
    return L;
}

int insertList(list_t *L, const char *str) {
    if (!str || !L) {
	    errno = EINVAL;   // setto errno per segnalare che i parametri sono errati
	    return -1;
    }
    elem_t *p1 = malloc(sizeof(elem_t));
    if (p1 == NULL) return -1;
    p1->key = strdup(str);
    int r = 1;
    elem_t *p = L->head, *prev = NULL;
    while(p->key && (r=L->compare(p->key, str))<=0) {
        prev = p;
        p = p->next;  
    }
    // Inserisco
    p1->next = p;
    if (prev != NULL) prev->next = p1;
    // Aggiorno la testa se occorre
    if (L->head == p) L->head = p1;
    return 0;
}

int deleteList(list_t *L, const char *str) {
    if (!str || !L) {
	    errno = EINVAL;   // setto errno per segnalare che i parametri sono errati
	    return -1;
    }

    int r = 0;
    elem_t *p = L->head, *prev = NULL;
    while(p->key && (r=L->compare(p->key, str))<0) {
        #ifdef DEBUG
        fprintf(stderr, "[lib liste]: deleteList: p->key = %s\n", p->key);
        #endif
        prev = p;
        p=p->next;   
    }
    #ifdef DEBUG
    fprintf(stderr, "[lib liste]: deleteList: p->key = %s\n", p->key);
    #endif
    if (p->key && (r=L->compare(p->key, str) == 0)) {
        // Ho trovato l'elemento
         #ifdef DEBUG
        fprintf(stderr, "[lib liste]: deleteList: Trovato l'elemento\n");
        #endif
        // Se è la testa aggiorno head
        if (p == L->head) {
            L->head = p->next;
        }
        // Se sta nel mezzo aggiorno il precedente
        if (prev != NULL) {
            prev->next = p->next;
        }
        free(p->key);
        free(p);
        return 0;
    } else {
        return -1;
    }
}

// Stampa gli elementi della lista sullo stream passato o su stderr
void printList(list_t *L, FILE *fd) {
    FILE *iostream = fd != NULL ? fd : stderr;
    if (!L) {
	    errno=EINVAL;
	    return;	
    }
    long count = 0;
    elem_t *p = L->head;
    while(p->key) {
	    fprintf(iostream, "\t\t\t\t\t\t%ld:\t%s\n",count++, p->key);
	    p=p->next;
    }
}

int  destroyList(list_t *L) {
    if (L == NULL) {
	    errno = EINVAL;
	    return -1;
    }
    elem_t *p = L->head, *t = NULL;
    while(p != NULL && p->key) {
	    t = p->next;
	    free(p->key);
	    free(p);
	    p=t;
    }
    free(p);
    free(L);
    return 0;
}

int isInList(list_t *L, const char *str) {
    if (L == NULL || str == NULL) {
        fprintf(stderr, "%p %s", (void *)L, str);
	    errno=EINVAL;
	    return 0;
    }
    elem_t *p = L->head;
    // Finché ho una chiave e non trovo quella cercata, mi fermo se la compare ritorna >0 -> ho superato la chiave
    while(p->key && L->compare(p->key, str) < 0) {
        p = p->next;
    }
    // Se ho ua chiave ed è quella cercata
    if (p->key && L->compare(p->key, str) == 0) {
        return 1;
    }
    return (0);
}