#include "system_abstraction.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include <fcntl.h>

// Memory management functions
void* sys_malloc(size_t size)
{
    return malloc(size);
}

void* sys_calloc(size_t nelements, size_t elementSize)
{
    return calloc(nelements, elementSize);
}

void sys_free(void* ptr)
{
    if (ptr) {
        free(ptr);
    }
}

// Random number generation
int sys_get_random_bytes(unsigned char* output, size_t output_len)
{
    FILE* fp = fopen("/dev/urandom", "rb");
    if (!fp) {
        // Fallback to pseudo-random if /dev/urandom is not available
        srand((unsigned int)time(NULL));
        for (size_t i = 0; i < output_len; i++) {
            output[i] = (unsigned char)(rand() & 0xFF);
        }
        return 0;
    }
    
    size_t read_bytes = fread(output, 1, output_len, fp);
    fclose(fp);
    
    if (read_bytes != output_len) {
        // Fallback to pseudo-random for remaining bytes
        srand((unsigned int)time(NULL));
        for (size_t i = read_bytes; i < output_len; i++) {
            output[i] = (unsigned char)(rand() & 0xFF);
        }
    }
    
    return 0;
}

// Time/delay functions
void sys_delay_ms(uint32_t ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

// Logging functions
void sys_log(log_level_t level, const char* format, ...)
{
    const char* level_str;
    FILE* output = stdout;
    
    switch (level) {
        case LOG_LEVEL_INFO:
            level_str = "[INFO]";
            break;
        case LOG_LEVEL_ERROR:
            level_str = "[ERROR]";
            output = stderr;
            break;
        case LOG_LEVEL_DEBUG:
            level_str = "[DEBUG]";
            break;
        default:
            level_str = "[UNKNOWN]";
            break;
    }
    
    // Print timestamp
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    fprintf(output, "%02d:%02d:%02d %s ", 
            tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, level_str);
    
    // Print the actual message
    va_list args;
    va_start(args, format);
    vfprintf(output, format, args);
    va_end(args);
    
    fprintf(output, "\n");
    fflush(output);
}

// File system abstraction
sys_file_result_t sys_file_open(sys_file_t* file, const char* path, sys_file_mode_t mode)
{
    if (!file || !path) {
        return SYS_FILE_ERROR;
    }
    
    memset(file, 0, sizeof(sys_file_t));
    
    const char* fmode = "wb"; // Default to write binary mode
    if (mode & SYS_FILE_CREATE_ALWAYS) {
        fmode = "wb"; // Create new file or overwrite existing
    }
    
    file->fp = fopen(path, fmode);
    if (!file->fp) {
        return SYS_FILE_ERROR;
    }
    
    file->is_open = 1;
    return SYS_FILE_OK;
}

sys_file_result_t sys_file_write(sys_file_t* file, const void* data, uint32_t size, uint32_t* written)
{
    if (!file || !file->is_open || !file->fp || !data || !written) {
        return SYS_FILE_ERROR;
    }
    
    size_t bytes_written = fwrite(data, 1, size, file->fp);
    *written = (uint32_t)bytes_written;
    
    if (bytes_written != size) {
        return SYS_FILE_ERROR;
    }
    
    return SYS_FILE_OK;
}

void sys_file_close(sys_file_t* file)
{
    if (file && file->is_open && file->fp) {
        fclose(file->fp);
        file->fp = NULL;
        file->is_open = 0;
    }
}
