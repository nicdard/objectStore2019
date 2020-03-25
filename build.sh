#!/bin/bash

#Nascondo stdout dei comandi del make
exec 3>&1
exec 1> /dev/null

if [ $# -lt 1 ]; then
    echo "Usage: ./build.sh [production|debug]";
fi

if [ $1 = "production" ]; then
    echo "Building production, libapi.a client.exe server.exe";
    make --eval=DEFINES=PRODUCTION libapi.a;
	make --eval=DEFINES=PRODUCTION client.exe;
	make --eval=DEFINES=PRODUCTION server.exe;
fi

if [ $1 = "debug" ]; then
    echo "Building debug, libapi.a client.exe server.exe";
    make --eval=DEFINES=DEBUG libapi.a;
	make --eval=DEFINES=DEBUG client.exe;
	make --eval=DEFINES=DEBUG server.exe;
fi

exec 1>&3