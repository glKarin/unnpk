#pragma once

#include <stddef.h>

#ifndef _WITHOUT_GUN
#define S_IRGRP 1
#define S_IXGRP 1
#define S_IROTH 1
#define S_IXOTH 1
#define S_IRWXU 1

typedef unsigned long mode_t;

/* win mkdir */
int mkdir_m(const char *p, mode_t mask);

/* memmem function */
void * memmem(const void *haystack_start, size_t haystack_len, const void *needle_start, size_t needle_len);

#endif