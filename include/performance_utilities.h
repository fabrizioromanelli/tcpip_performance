#ifndef PERFORMANCE_UTILITIES_H
#define PERFORMANCE_UTILITIES_H

#include <iostream>
#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#define SERVER_NAME "server-dev"
#define FILE_NAME   "/debs/stable/dls-legacy-pods_4.0.0.deb"

#define MB_SIZE_IN_KB 1024
#define MB_SIZE_IN_BYTES MB_SIZE_IN_KB*MB_SIZE_IN_KB
#define BUF_SIZE 2*MB_SIZE_IN_BYTES // 2MB
#define SKIPS 10 // initial measurement skips - skip the first slow-start values + initial burst
#define ROUNDS 4 // measurement rounds - number of file downloads
#define TIMEOUT_S 0 // socket timeout in s
#define TIMEOUT_US 200000 // socket timeout in us

#endif // PERFORMANCE_UTILITIES_H
