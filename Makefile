CC = gcc -g
CFLAGS = -Wall -pedantic
LIBS = -lpthread
AR = ar
ARFLAGS = rvs

# all is an alias for production
.PHONY: all, clean, cleanall, test, testd, debug, production, devtest

# Target di default, crea tutti gli eseguibili in modalità produzione
all : production

# Crea eseguibile del server
server.exe : ./liste.o ./server.o ./messages.o ./serverapi.o
	@$(CC) $(CFLAGS) -D$(DEFINES) $^ -o $@ $(LIBS)

# Crea eseguibile del client
client.exe : ./client.o ./messages.o ./libapi.a 
	@$(CC) $(OPTFLAGS) $(LIBS) $^ -L -lapi -o $@

# Tests di parsing dei messaggi e altre funzionalità
devtest : clean
	make --eval=DEFINES=DEBUG mytest.exe

mytest.exe : ./mytest.o ./liste.o ./serverapi.o ./messages.o
	@$(CC) $(CFLAGS) -D$(DEFINES) $^ -o $@ $(LIBS)

# Library
libapi.a : ./api.o
	@$(AR) $(ARFLAGS) $@ $^ 

% : %.c
	@$(CC) $(CFLAGS) -D$(DEFINES) $< -o $@ $(LIBS)

%.o : %.c
	@$(CC) $(CFLAGS) -D$(DEFINES) $< -c -o $@

# Rimuove tutti i file di compilazione librerie e esegubili
clean :
	@-rm -r *.exe *.o *.a

# Comando per il test, fa prima la build del progetto e lancia lo script di test
test : cleanall production
	@chmod +x ./test_script.sh
	@./test_script.sh

# Comando per il test in modalità debug, in test_script si deve disabilitare la redirezione dello stderr per vedere i log
testd : cleanall debug
	@chmod +x ./test_script.sh
	@./test_script.sh
	
# Comando per la build di debug
debug : clean
	@chmod +x ./build.sh
	@./build.sh debug

# Comando per build di produzione
production : clean
	@chmod +x ./build.sh
	@./build.sh production

# Rimuove eseguibili, file di compilazione, librerie statiche, file di log, e cartella dei dati dell'objectstore
cleanall :
	@-rm -r *.exe *.o *.a *.log data
	