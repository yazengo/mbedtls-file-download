#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include "system_abstraction.h"
#include "https_download.h"

// Test configuration
#define TEST_FILE_PATH "./test_download.tmp"
#define TEST_URL_SMALL "https://httpbin.org/json"
#define TEST_URL_LARGER "https://raw.githubusercontent.com/curl/curl/master/README.md"

// Test result tracking
static int tests_passed = 0;
static int tests_failed = 0;

// Test utility functions
void test_assert(int condition, const char* test_name)
{
    if (condition) {
        printf("✓ PASS: %s\n", test_name);
        tests_passed++;
    } else {
        printf("✗ FAIL: %s\n", test_name);
        tests_failed++;
    }
}

int file_exists(const char* filename)
{
    struct stat buffer;   
    return (stat(filename, &buffer) == 0);
}

long get_file_size(const char* filename)
{
    FILE* fp = fopen(filename, "rb");
    if (!fp) return -1;
    
    fseek(fp, 0L, SEEK_END);
    long size = ftell(fp);
    fclose(fp);
    
    return size;
}

void cleanup_test_files()
{
    if (file_exists(TEST_FILE_PATH)) {
        unlink(TEST_FILE_PATH);
    }
}

// Test system abstraction layer
void test_system_abstraction()
{
    printf("\n=== Testing System Abstraction Layer ===\n");
    
    // Test memory allocation
    void* ptr = sys_malloc(1024);
    test_assert(ptr != NULL, "sys_malloc allocation");
    sys_free(ptr);
    
    // Test calloc
    ptr = sys_calloc(10, 100);
    test_assert(ptr != NULL, "sys_calloc allocation");
    
    // Verify memory is zeroed
    char* char_ptr = (char*)ptr;
    int is_zeroed = 1;
    for (int i = 0; i < 1000; i++) {
        if (char_ptr[i] != 0) {
            is_zeroed = 0;
            break;
        }
    }
    test_assert(is_zeroed, "sys_calloc zeros memory");
    sys_free(ptr);
    
    // Test random number generation
    unsigned char random_buf[32];
    int rand_result = sys_get_random_bytes(random_buf, sizeof(random_buf));
    test_assert(rand_result == 0, "sys_get_random_bytes returns success");
    
    // Check that random bytes are not all zeros (very unlikely)
    int has_non_zero = 0;
    for (size_t i = 0; i < sizeof(random_buf); i++) {
        if (random_buf[i] != 0) {
            has_non_zero = 1;
            break;
        }
    }
    test_assert(has_non_zero, "sys_get_random_bytes generates non-zero data");
    
    // Test logging (visual check)
    printf("Testing logging functions (visual check):\n");
    SYS_LOG_INFO("This is an info message");
    SYS_LOG_ERROR("This is an error message");
    SYS_LOG_DEBUG("This is a debug message");
    
    // Test file operations
    sys_file_t test_file;
    const char* test_data = "Hello, World!\nThis is a test file.\n";
    uint32_t test_data_len = strlen(test_data);
    uint32_t written = 0;
    
    sys_file_result_t result = sys_file_open(&test_file, "test_sys_file.tmp", SYS_FILE_CREATE_ALWAYS | SYS_FILE_WRITE);
    test_assert(result == SYS_FILE_OK, "sys_file_open creates file");
    
    result = sys_file_write(&test_file, test_data, test_data_len, &written);
    test_assert(result == SYS_FILE_OK && written == test_data_len, "sys_file_write writes correct amount");
    
    sys_file_close(&test_file);
    
    // Verify file was created and has correct size
    test_assert(file_exists("test_sys_file.tmp"), "File was created on disk");
    test_assert(get_file_size("test_sys_file.tmp") == test_data_len, "File has correct size");
    
    // Cleanup
    unlink("test_sys_file.tmp");
}

// Test HTTPS download functionality
void test_https_download()
{
    printf("\n=== Testing HTTPS Download Functionality ===\n");
    
    cleanup_test_files();
    
    // Test 1: Download a small JSON file
    printf("Test 1: Downloading small JSON file...\n");
    int result = https_download(TEST_URL_SMALL, TEST_FILE_PATH);
    test_assert(result == 0, "Small file download succeeds");
    test_assert(file_exists(TEST_FILE_PATH), "Downloaded file exists");
    
    long file_size = get_file_size(TEST_FILE_PATH);
    test_assert(file_size > 0, "Downloaded file has content");
    printf("Downloaded file size: %ld bytes\n", file_size);
    
    cleanup_test_files();
    
    // Test 2: Download a larger file
    printf("\nTest 2: Downloading larger file...\n");
    result = https_download(TEST_URL_LARGER, TEST_FILE_PATH);
    test_assert(result == 0, "Larger file download succeeds");
    test_assert(file_exists(TEST_FILE_PATH), "Downloaded larger file exists");
    
    file_size = get_file_size(TEST_FILE_PATH);
    test_assert(file_size > 1000, "Downloaded larger file has substantial content");
    printf("Downloaded file size: %ld bytes\n", file_size);
    
    cleanup_test_files();
    
    // Test 3: Invalid URL handling
    printf("\nTest 3: Testing invalid URL handling...\n");
    result = https_download("https://this-domain-should-not-exist-12345.com/test", TEST_FILE_PATH);
    test_assert(result != 0, "Invalid URL properly fails");
    
    // Test 4: Invalid path handling
    printf("\nTest 4: Testing invalid path handling...\n");
    result = https_download(TEST_URL_SMALL, "/root/cannot_write_here.tmp");
    test_assert(result != 0, "Invalid write path properly fails");
}

// Performance and stress tests
void test_performance()
{
    printf("\n=== Performance Tests ===\n");
    
    // Test download performance
    printf("Testing download performance...\n");
    
    cleanup_test_files();
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    int result = https_download(TEST_URL_SMALL, TEST_FILE_PATH);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    
    test_assert(result == 0, "Performance test download succeeds");
    printf("Download completed in %.2f seconds\n", elapsed);
    
    long file_size = get_file_size(TEST_FILE_PATH);
    if (file_size > 0 && elapsed > 0) {
        printf("Download speed: %.2f bytes/second\n", file_size / elapsed);
    }
    
    cleanup_test_files();
}

// URL parsing tests
void test_url_parsing()
{
    printf("\n=== URL Parsing Tests ===\n");
    
    // These tests verify that various URL formats work
    const char* test_urls[] = {
        "https://httpbin.org/json",
        "https://raw.githubusercontent.com/curl/curl/master/README.md",
        NULL
    };
    
    for (int i = 0; test_urls[i] != NULL; i++) {
        printf("Testing URL: %s\n", test_urls[i]);
        
        cleanup_test_files();
        
        int result = https_download((char*)test_urls[i], TEST_FILE_PATH);
        
        char test_name[256];
        snprintf(test_name, sizeof(test_name), "URL parsing and download for URL %d", i + 1);
        test_assert(result == 0, test_name);
        
        if (result == 0) {
            long size = get_file_size(TEST_FILE_PATH);
            printf("  Downloaded %ld bytes\n", size);
        }
        
        cleanup_test_files();
    }
}

void print_test_summary()
{
    printf("\n==================================================\n");
    printf("Test Summary:\n");
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("  Total:  %d\n", tests_passed + tests_failed);
    
    if (tests_failed == 0) {
        printf("\nAll tests passed!\n");
    } else {
        printf("\nSome tests failed. Please check the output above.\n");
    }
    printf("==================================================\n");
}

int main(int argc, char* argv[])
{
    printf("HTTPS Download Library Test Suite\n");
    printf("==================================================\n");
    
    // Parse command line arguments
    int run_performance_tests = 1;
    int run_url_tests = 1;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-performance") == 0) {
            run_performance_tests = 0;
        } else if (strcmp(argv[i], "--no-url-tests") == 0) {
            run_url_tests = 0;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --no-performance  Skip performance tests\n");
            printf("  --no-url-tests    Skip URL parsing tests\n");
            printf("  --help            Show this help message\n");
            return 0;
        }
    }
    
    // Run tests
    test_system_abstraction();
    test_https_download();
    
    if (run_performance_tests) {
        test_performance();
    }
    
    if (run_url_tests) {
        test_url_parsing();
    }
    
    print_test_summary();
    
    // Return appropriate exit code
    return (tests_failed == 0) ? 0 : 1;
}
