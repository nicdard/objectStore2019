/*
 Header contente macro per gestione dell'errore 
*/
#ifndef _MY_ERROR_H_
#define _MY_ERROR_H_

#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>


// Se X == val stampa un messaggio di errore (str e indicazione di linea e file) e stoppa
#define CHECK_EQS(X, val, str) \
    if ((X)==val) { \
	    perror(#str); \
	    fprintf(stderr, "ERRORE ALLA LINEA %d FILE %s\n", __LINE__, __FILE__); \
        exit(EXIT_FAILURE); \
    }
// Se X != val stampa un messaggio di errore (str e indicazione di linea e file) e stoppa
#define CHECK_NEQS(X, val, str)	\
    if ((X)!=val) { \
	    perror(#str); \
	    fprintf(stderr, "ERRORE ALLA LINEA %d FILE %s\n", __LINE__, __FILE__); \
	    exit(EXIT_FAILURE); \
    }
// Se X == val stampa errore e esegue cmd (può essere anche una lista di comandi separati da ';', l'ultimo senza ';' finale )
#define CHECK_EQS_NOEXIT(X, val, str, cmd) \
    if ((X)==val) { \
	    perror(#str); \
	    fprintf(stderr, "ERRORE ALLA LINEA %d FILE %s\n", __LINE__, __FILE__); \
        cmd; \
    }
// Se X != val stampa errore e esegue cmd (può essere anche una lista di comandi separati da ';', l'ultimo senza ';' finale )
#define CHECK_NEQS_NOEXIT(X, val, str, cmd) \
    if ((X)!=val) { \
	    perror(#str); \
	    fprintf(stderr, "ERRORE ALLA LINEA %d FILE %s\n", __LINE__, __FILE__); \
        cmd; \
    }

#endif