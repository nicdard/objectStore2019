#!/bin/bash

FILENAME=testout.log;

# Inizializzazione array 
for ((i = 1; i < 4; i++)) do
    #totale test effettuati per tipologia
    test_total[$i]=$((0));
    #totale test con successo (senza alcun errore nell'intera batteria)
    testg[$i]=$((0));
    #totali operazioni effettuate per batteria di test
    testb[$i]=$((0));
    #totali operazioni fallite per batteria di test
    testf[$i]=$((0));
    #tempo totale di esecuzione delle batterie andate a buon fine
    test_times[$i]=$((0));
done

# Estrae da un riga del file di log (=una esecuzione di client.exe) le informazioni
function extract_info() {
    IFS="," read -r client testn start_time exit_time connection disconnection operations succeded failed <<< "$1"
    operations=$((${operations##*:}));
    failed=$((${failed##*:}));
    succeded=$((${succeded##*:}));
    # Mi serve dopo per il calcolo
    op_failed=$(($failed));
    test_number=$((${testn##*:}));
    # Totale per singola operazione: N.B. aggiorno ora per evitare di contare connect e leave
    testb[$test_number]=$((${testb[$test_number]} + $succeded + $failed));
    if [ $test_number -gt 0 ] && [ $test_number -lt 4 ]; then
        if [ ${connection##*:} = "Succeded" ]; then
            succeded=$(($succeded + 1));
        else
            failed=$(($failed + 1));
        fi
        if [ ${disconnection##*:} = "Succeded" ]; then
            succeded=$(($succeded + 1));
        else
            failed=$(($failed + 1));
        fi
        # Totale aggiornato per tipologia di batteria
        test_total[$test_number]=$((${test_total[$test_number]} + 1));
        #Se tutte le operazioni sono andate a buon fine il test è passato
        if [ $succeded -eq $operations ]; then
            testg[$test_number]=$((${testg[$test_number]} + 1));
            # Timing
            # Sommo per ogni batteria di test andata bene il tempo di esecuzione per calcolare il tempo medio di elaborazione sui test riusciti
            # Estraggo Dt
            start_time=$((${start_time##*:}));
            exit_time=$((${exit_time##*:}));
            dt=$(($exit_time - $start_time));
            test_time[$test_number]=$((${test_time[$test_number]} + $dt));
        else 
            # Altimenti guardo quante operazioni non sono andate a buon fine e segno
            testf[$test_number]=$((${testf[$test_number]} + $op_failed));
        fi
    fi
    IFS=
};

# Leggo il file di log riga per riga e estraggo le informazioni
while IFS= read -r line; do 
    extract_info "$line";
done < "$FILENAME";

echo "============================";
echo "============================";
echo "      Results summary";
echo "============================";
# Stampo le informazioni generali
total_clients=$(grep -c "Client" testout.log);
echo "Client lanciati in totale: $((total_clients))";
gconn=$(grep -c "Connection:Succeded" testout.log);
echo "-> Percentuale di connessioni eseguite con successo: $((gconn * 100 / total_clients))%";
gleave=$(grep -c "Disconnection:Succeded" testout.log);
echo "-> Percentuale di disconnessioni eseguite con successo: $((gleave * 100 / total_clients))%";
# Stampo per ogni tipologia di test
for ((i = 1; i < 4; i++)) do
    echo "======"
    echo "Test $i: superate ${testg[$i]} su ${test_total[$i]} batterie di test";
    if [ ${testg[$i]} -gt 0 ]; then
        echo "Tempo medio di elaborazione per le batterie superate: $((${test_time[$i]} / ${testg[$i]}))ms";
    fi
    echo "Operazioni totali effettuate (escluse connessioni e disconnessioni): ${testb[$i]}";
    if [ ${testb[$i]} -gt 0 ]; then
        echo "-> Percentuale di singole operazioni effettuate fallite sul totale: $((${testf[$i]} * 100 / ${testb[$i]}))%";
    fi
done
# Stoppo il server con un segnale di statistica (N.B. vedi testo progetto)
echo "";
echo "=======";
echo "Killing server";
kill -SIGUSR1 $(pidof server.exe);
