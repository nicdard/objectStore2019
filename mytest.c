/*
    File di test in fase di sviluppo
    Test di singole funzionalità supportato con define da passare da riga di comando
*/

#include <stdio.h>
#include <string.h>
#include "./messages.h"
#include "./serverapi.h"

#define REGISTER_OK "REGISTER name \n"
#define LEAVE_OK "LEAVE \n"
#define STORE_OK "STORE filename 12 \n ciao ciaociaociaociaociao"
#define DELETE_OK "DELETE filename \n"
#define RETRIEVE_OK "RETRIEVE filename \n"
#define UNKNOW_CMD "SOME name \n"
#define MALFORMED_LEAVE "LEAVE name \n"
#define MALFORMED_STORE "STORE filename a23 \n ciaociaociaocaicoaicoa"

void testHeaderParser();
void testRegisterWorkerFunMain();
void testLeaveWorkerFun(requestHeader_t *header);
void testStats();

int main(void) {

    atexit(cleanup);

    testHeaderParser(REGISTER_OK);
    testHeaderParser(STORE_OK);
    testHeaderParser(LEAVE_OK);
    testHeaderParser(DELETE_OK);
    testHeaderParser(RETRIEVE_OK);
    testHeaderParser(UNKNOW_CMD);
    testHeaderParser(MALFORMED_LEAVE);
    testHeaderParser(MALFORMED_STORE);

    testStats();
    
    testRegisterWorkerFunMain();

    testStats();

    exit(EXIT_SUCCESS);
}

void testHeaderParser(char* test_str) {
    #if  defined DEBUG_HEADER_PARSING || defined DEBUG
    char cmd[256];
    strncpy(cmd, test_str, 256);
    char nbuffer[BUFFER_SIZE] = "";
    dataBuffer_t nameBuffer = {
        .buffer = nbuffer,
        .bufferSize = BUFFER_SIZE,
        .dataLen = 0
    };
    requestHeader_t header = {
        .cmd = NO_MSG_ERROR,
        .nameParam = &nameBuffer,
        .dataLength = 0
    };
    headerParsingError_t error = NO_MSG_ERROR;
    dataBuffer_t dataBuffer;
    char buffer[BUFFER_SIZE] = "";
    dataBuffer.buffer = buffer;
    dataBuffer.bufferSize = BUFFER_SIZE;
    dataBuffer.dataLen = 0;
    parseRequestHeader((void*)cmd, &header, &dataBuffer, &error);

    perror_header_parser(error, cmd)
           

    fprintf(stderr, "%d\n", header.cmd);
    fprintf(stderr, "%s\n", header.nameParam->buffer);
    fprintf(stderr, "%ld\n", header.dataLength);
    fprintf(stderr, "%s\n", dataBuffer.buffer);
    #endif
}

void testRegisterWorkerFun(requestHeader_t *header) {
    workerCmdError_t error = NO_SERVERW_ERROR;
    registerFun(header, &error);
    fprintf(stderr, "\nerror value %d\n", error);
}

void testLeaveWorkerFun(requestHeader_t *header) {
    workerCmdError_t error = NO_SERVERW_ERROR;
    //leaveFun(header, &error);
    fprintf(stderr, "\nerror value %d\n", error);
}

void testDeleteWorkerFun(requestHeader_t *header) {
    workerCmdError_t error = NO_SERVERW_ERROR;
    deleteFun(header, "pippo", &error);
    fprintf(stderr, "\nerror value %d\n", error);
}

void testRegisterWorkerFunMain() {
    #if defined DEBUG || defined DEBUG_REGISTER_WFUN
    dataBuffer_t dataBuffer = {
        .buffer = "ciao",
        .dataLen = 5,
        .bufferSize = BUFFER_NAME_SIZE
    };
    requestHeader_t header = {
        .cmd = REGISTER,
        .nameParam = &dataBuffer,
        .dataLength = 0
    };
    testRegisterWorkerFun(&header);
    header.cmd = LEAVE;
    testRegisterWorkerFun(&header);
    testLeaveWorkerFun(&header);
    header.cmd = DELETE;
    header.nameParam->buffer = "a";
    testDeleteWorkerFun(&header);
    
    #endif
}

void testStats() {
    #if defined DEBUG || defined DEBUG_STATS
    initializeStatistics(); 
    testRegisterWorkerFunMain();
    printStatistics(stderr); 
    #endif 
}



