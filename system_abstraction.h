#ifndef SYSTEM_ABSTRACTION_H
#define SYSTEM_ABSTRACTION_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// Memory management functions
void* sys_malloc(size_t size);
void* sys_calloc(size_t nelements, size_t elementSize);
void sys_free(void* ptr);

// Random number generation
int sys_get_random_bytes(unsigned char* output, size_t output_len);

// Time/delay functions
void sys_delay_ms(uint32_t ms);

// Logging functions
typedef enum {
    LOG_LEVEL_INFO,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_DEBUG
} log_level_t;

void sys_log(log_level_t level, const char* format, ...);

#define SYS_LOG_INFO(fmt, ...) sys_log(LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define SYS_LOG_ERROR(fmt, ...) sys_log(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define SYS_LOG_DEBUG(fmt, ...) sys_log(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)

// File system abstraction
typedef struct {
    FILE* fp;
    int is_open;
} sys_file_t;

typedef enum {
    SYS_FILE_OK = 0,
    SYS_FILE_ERROR = -1
} sys_file_result_t;

typedef enum {
    SYS_FILE_CREATE_ALWAYS = 1,
    SYS_FILE_WRITE = 2
} sys_file_mode_t;

sys_file_result_t sys_file_open(sys_file_t* file, const char* path, sys_file_mode_t mode);
sys_file_result_t sys_file_write(sys_file_t* file, const void* data, uint32_t size, uint32_t* written);
void sys_file_close(sys_file_t* file);

#ifdef __cplusplus
}
#endif

#endif // SYSTEM_ABSTRACTION_H
