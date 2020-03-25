#include <sys/time.h>
#include "./api.h"

// Tipo delle statistiche
typedef struct client_stats {
	long long start_time; 	// time in ms
	long long log_time; 	// time in ms 
	char *name;				// nome del client
	long test;				// numero di batteria di test
	int ok_connection;		// boolean: 1 se connessione eseguita con successo
	int ok_disconnection;	// boolean: 1 se disconnessione eseguita con successo
	int operations;			// numero di operazioni richieste al server (comprese connessioni e disconnessioni)
	int ok_operations;		// numero di operazioni del test eseguite con successo
	int ko_operations;		// numero di operazioni del test fallite
} client_stats_t;

// Statistiche del client
client_stats_t stats = {
	.start_time = 0,
	.log_time = 0,
	.name = NULL,
	.test = 0,
	.ok_connection = 0,
	.ok_disconnection = 0,
	.operations = 0,
	.ok_operations = 0,
	.ko_operations = 0
};

// Stampa messaggio d'uso e esce
#define USAGE_MESSAGE  {printf("Usage: client [NAME] [TESTCODE]:1=store|2=retrieve|3=delete \n"); \
		return 1;}

// Oggetto da inviare, una stringa di 10 caratteri
#define TEST_OBJ "NicoladarD"



// ======================
// Funzioni ausiliarie

// Converte una stringa in numero, se non è possibile ritorna -1
static long isNumber(char *s);
// Compara byte a byte due blocchi di memoria fino all'indice len
static int bytesncmp(void *s1, void *s2, size_t len);
// Restituisce now() in ms
static long long timeInMilliseconds(void);

// =====================================
/* 
 Test 1
 Memorizza sequenze di "NicoladarD" di dimensione crescente.
 */
static void test1(void);
/* 
 Test 2 
 Recupera gli oggetti del Test 1 verificando che i contenuti siano corretti
*/
static void test2(void);

/*
 Test 3
 Cancella oggetti
*/
static void test3(void);


static void test1(void) {
	// dichiaro un buffer di grandezza massima, incrementalmente aggiungo pezzi di dati e mando un blocco sempre più lungo
	char obj[MAX_OBJ_DIM] = {'\0'};
	char name[3] = {'\0'};
	int i;
	for (i = 1; i <= OBJ_NUMBER; ++i) {
		stats.operations++;
		// Scrivo il nome in stringa per poterlo passare come parametro
		CHECK_EQS_NOEXIT(snprintf(name, 3, "%d", i), (-1), "[CLIENT Test 1]: Errore nella creazione del nome utente", 
			stats.ko_operations++; continue);
		// Aggiungo al buffer la stringa di prova ripetuta
		int start_index = (i - 1) * OBJ_CHUNCK_SIZE;
		int end_index = start_index + OBJ_CHUNCK_SIZE;
		// Mi assicuro che anche cambiando massima dimensione all'oggetto, questo non provochi errori nella scrittura sul buffer
		int j;
		size_t TEST_OBJ_LEN = strlen(TEST_OBJ);
		for (j = start_index; j < end_index && j <= MAX_OBJ_DIM - TEST_OBJ_LEN; j += TEST_OBJ_LEN) {
			strncpy(obj + j, TEST_OBJ, TEST_OBJ_LEN);
		}
		// Mi assicuro che anche con config.h diverso la dimensione sia quella giusta
		size_t len = MAX_OBJ_DIM < i * OBJ_CHUNCK_SIZE ? MAX_OBJ_DIM : i * OBJ_CHUNCK_SIZE;
		int retval = os_store(name, (void *) obj, len);
		//int retval = os_store(name, (void *) obj, 0);
		stats.ok_operations += retval;
		stats.ko_operations += 1 - retval;
	}
}

static void test2() {
	// Ricreo le stringhe in modo da controllarne il contenuto (vedi test1)
	char obj[MAX_OBJ_DIM] = {'\0'};
	char name[3] = {'\0'};
	int i;
	for (i = 1; i <= OBJ_NUMBER; ++i) {
		stats.operations++;
		CHECK_EQS_NOEXIT(snprintf(name, 3, "%d", i), (-1), "[CLIENT Test 1]: Errore nella creazione del nome utente", 
			stats.ko_operations++; continue);
		int start_index = (i - 1) * OBJ_CHUNCK_SIZE;
		int end_index = start_index + OBJ_CHUNCK_SIZE;
		// Mi assicuro che anche cambiando massima dimensione all'oggetto, questo non provochi errori nella scrittura sul buffer
		int j;
		size_t TEST_OBJ_LEN = strlen(TEST_OBJ);
		for (j = start_index; j < end_index && j <= MAX_OBJ_DIM - TEST_OBJ_LEN; j += TEST_OBJ_LEN) {
			strncpy(obj + j, TEST_OBJ, TEST_OBJ_LEN);
		}
		void *retrieveData = os_retrieve(name);
		if (retrieveData == NULL) {
			stats.ko_operations++;
			continue;
		}
		#ifdef DEBUG
		fprintf(stderr, "[DEBUG CLIENT]: obj %d retrieveData %d", strlen(obj), strlen((char *) retrieveData));
		#endif
		if (bytesncmp(retrieveData, (void *) obj, MAX_OBJ_DIM < end_index ? MAX_OBJ_DIM : end_index) == 1) {
			stats.ok_operations++;
		} else {
			stats.ko_operations++;
		}
		free(retrieveData);
	}
}

static void test3() {
	char name[3] = {'\0'};
	int i;
	for (i = 1; i <= OBJ_NUMBER; ++i) {
		stats.operations++;
		CHECK_EQS_NOEXIT(snprintf(name, 3, "%d", i), (-1), "[CLIENT Test 1]: Errore nella creazione del nome utente", 
			stats.ko_operations++; continue);
		int retval = os_delete(name);
		stats.ok_operations += retval;
		stats.ko_operations += 1 - retval;
	}
}

int main (int argc, char* argv[]) {
	// Segno il tempo di inizio
	stats.start_time = timeInMilliseconds();
	// Controllo che ci siano i parametri e che siano corretti
	if (argc != 3) USAGE_MESSAGE
	stats.test = isNumber(argv[2]);
	if (stats.test == -1) USAGE_MESSAGE
	stats.name = argv[1];

	// Mi connetto allo store
	stats.ok_connection += os_connect(stats.name);
	stats.operations++;

	// Se sono riuscito a connettermi eseguo il test
	if (stats.ok_connection == 1) {
		switch (stats.test) {
			case 1: {
				test1();
				break;
			}
			case 2: {
				test2();
				break;
			}
			case 3: {
				test3();
				break;
			}
			default:
				break;
		}

		// Mi disconnetto
		stats.ok_disconnection += os_disconnect();
		stats.operations++;
	}

	// ==============
	// LOG
	// Registro tempo di fine esecuzione dei test
	stats.log_time = timeInMilliseconds();
	// Stampo le statistiche
	printf("Client:%s,Test:%ld,StartTime:%lld,ExitTime:%lld", 
		stats.name, 
		stats.test, 
		stats.start_time,
		stats.log_time
	);
	if (stats.ok_connection) printf(",Connection:Succeded");
	else printf(",Connection:Error");
	if (stats.ok_connection) printf(",Disconnection:Succeded");
	else printf(",Disconnection:Error");
	printf(",Operations:%d,Succeded:%d,Failed:%d\n", 
		stats.operations,
		stats.ok_operations, 
		stats.ko_operations
	);
	fflush(stdout);

	return 0;
}

static long isNumber(char *s) {
   char* e = NULL;
   long val = strtol(s, &e, 0);
   if (e != NULL && *e == (char)0) return val; 
   return -1;
}

static int bytesncmp(void *s1, void *s2, size_t len) {
	int equal = 1;
	int i;
	for (i = 0; i < len && equal == 1; i++) {
		equal = *((char *) s1 + i) == *((char *) s2 + i) ? 1 : 0;
		#ifdef DEBUG
		if (equal == 0) {
			fprintf(stderr, "[DEBUG CLIENT] trovati bytes diversi: %c %c", *((char *) s1 + i), *((char *) s2 + i));
		}
		#endif
	}
	return equal; 
}

static long long timeInMilliseconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (( (long long) tv.tv_sec) * 1000) + (tv.tv_usec / 1000);
}