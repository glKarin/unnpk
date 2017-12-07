#pragma once

#include <stdint.h>

/* Error */
#define NPK_NO_ERROR 0
#define NPK_OTHER_ERROR 1
#define MISS_ARGUMENTS_MASK 2
#define NPK_FILE_CANNOT_OPEN 3
#define MKDIR_FOR_DEST_FAIL 4
#define MKDIR_FOR_DEST_SUB_FAIL 5
#define CANNOT_WRITE_DEST_FILE 6
#define OUT_OF_MEMORY 7
#define OUT_OF_MEMORY_FOR_READ 8
#define OUT_OF_MEMORY_FOR_WRITE 9
#define UNCOMPRESS_ERROR_FOR_OUT_OF_MEMORY 10
#define UNCOMPRESS_ERROR_FOR_MAP_IS_INVALID 11
#define UNCOMPRESS_ERROR_FOR_DATA_IS_NOT_Z 12
#define NOT_BE_COMPRESSED_DATA_SIZE_IS_NOT_EQUALS_MAP 13
#define MAY_BE_NOT_NPK_FILE 14

#define INVALID_ENUM 100

/* State */
#define NPK_DEBUG 1

typedef unsigned long mask_t;
typedef unsigned int enum_t;

_Bool unnpk(const char *npk, const char *to);
enum_t get_error(void);
void enable(enum_t e);
void disable(enum_t e);
_Bool is_enabled(enum_t e);