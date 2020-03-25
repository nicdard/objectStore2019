#!/bin/bash

#nasconde stderr
exec 3>&2
exec 2> /dev/null

# Resetta il file di log
echo -n "" > testout.log

echo "";
echo "LAUNCHING TESTS";
echo "";

echo "Starting server"
echo ""
./server.exe &

#comandi eseguiti in una subshell in modo che
#la wait non consideri il processo store
(
    echo "======";
    echo "Test 1 - 50 clients running test 1 (generate objects and store them)";
    for ((i = 0; i < 50; i++)); do
        ./client.exe $i 1 >> testout.log &
    done
    wait;

    echo "======";
    echo "Test 2 - 30 clients running test 2 (retrieve objects and checking them)";
    echo "         20 clients running test 3 (delete objects)";
    for ((i = 0; i < 30; i++)); do
        ./client.exe $i 2 >> testout.log &
    done
    for ((i = 30; i < 50; i++)); do
        ./client.exe $i 3 >> testout.log &
    done
    wait;
)

#ripristina stderr
exec 2>&3

# Eseguo script che fa analisi del log
chmod +x ./testsum.sh
./testsum.sh
