//
// Created by andrea on 25/05/26.
//
#include "protocolSocket.h"
#include "error.h"
#include <errno.h>

int sendAll(int fd, const void* buffer, size_t size) {
    if (fd < 0 || buffer == NULL) return INVALID_PARAMS;

    const char* ptr = buffer;
    size_t sent = 0;

    while (sent < size) {
        ssize_t n = send(fd, ptr + sent, size - sent, 0);//manda byte partendo dalla base = ptr sommando offest = sent, i byte rimanenti size -sent
        if (n < 0) {
            if (errno == EINTR)continue;// se interrotto da un segnale mandato allora continua
            return SOCKET_ERROR;
        }
        if (n == 0) {
            return SOCKET_ERROR;
        }

        sent+=(size_t)n;//aggiorno il numero dei byte mandati
    }
    return 0;
}

int recvAll(int fd, void* buffer, size_t size){
    if (fd < 0 || buffer == NULL) return INVALID_PARAMS;

    char *ptr = buffer;
    size_t received = 0;

    while (received < size) {
        ssize_t n = recv(fd, ptr + received, size - received, 0);

        if (n < 0) {
            if (errno == EINTR) {// se interrotto da un segnale mandato allora continua
                continue;
            }

            return SOCKET_ERROR;
        }

        if (n == 0) {
            return SOCKET_CLOSED;
        }

        received += (size_t)n;//aggiorno il numero dei byte mandati
    }

    return 0;
}
