#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


#define BOOL int
#define TRUE  1
#define FALSE 0

#define PANIC(msg) { panic(msg); }
#define ASSERT(x) assert(x)


#define ERROR 1
#define INFO 2
#define DEBUG 3

#define LOG_LEVEL INFO




void panic(const char *message);
void log_error(const char* message, ...);
void log_info(const char* message, ...);
void log_debug(const char* message, ...);



