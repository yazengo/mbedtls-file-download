#define _GNU_SOURCE  // for asprintf
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include "https_download.h"
#include "system_abstraction.h"

void print_usage(const char* program_name)
{
    printf("用法: %s [选项] <下载链接> [保存路径]\n", program_name);
    printf("\n");
    printf("参数:\n");
    printf("  <下载链接>    要下载的 HTTPS URL\n");
    printf("  [保存路径]    可选，指定保存文件的路径\n");
    printf("                如果不指定，将使用 URL 中的文件名保存到当前目录\n");
    printf("\n");
    printf("选项:\n");
    printf("  -h, --help    显示此帮助信息\n");
    printf("  -v, --verbose 显示详细信息\n");
    printf("  -o <文件>     指定输出文件名\n");
    printf("\n");
    printf("示例:\n");
    printf("  %s https://httpbin.org/json\n", program_name);
    printf("  %s https://httpbin.org/json ./data.json\n", program_name);
    printf("  %s -o myfile.json https://httpbin.org/json\n", program_name);
    printf("  %s -v https://raw.githubusercontent.com/curl/curl/master/README.md\n", program_name);
}

char* extract_filename_from_url(const char* url)
{
    // 查找最后一个 '/' 后的内容作为文件名
    const char* last_slash = strrchr(url, '/');
    if (!last_slash) {
        return strdup("downloaded_file");
    }
    
    const char* filename = last_slash + 1;
    
    // 如果文件名为空或只有查询参数，使用默认名称
    if (!filename[0] || filename[0] == '?') {
        return strdup("downloaded_file");
    }
    
    // 移除查询参数
    char* result = strdup(filename);
    char* query = strchr(result, '?');
    if (query) {
        *query = '\0';
    }
    
    // 如果结果为空，使用默认名称
    if (!result[0]) {
        free(result);
        return strdup("downloaded_file");
    }
    
    return result;
}

int file_exists(const char* filename)
{
    return (access(filename, F_OK) == 0);
}

char* get_unique_filename(const char* original_filename)
{
    if (!file_exists(original_filename)) {
        return strdup(original_filename);
    }
    
    // 文件已存在，生成唯一的文件名
    char* base_name = strdup(original_filename);
    char* extension = strrchr(base_name, '.');
    
    if (extension) {
        *extension = '\0';
        extension++; // 跳过 '.'
    }
    
    for (int i = 1; i < 1000; i++) {
        char* new_filename = NULL;
        int ret;
        if (extension) {
            ret = asprintf(&new_filename, "%s_%d.%s", base_name, i, extension);
        } else {
            ret = asprintf(&new_filename, "%s_%d", base_name, i);
        }
        
        if (ret == -1) {
            free(base_name);
            return strdup("downloaded_file_unique");
        }
        
        if (!file_exists(new_filename)) {
            free(base_name);
            return new_filename;
        }
        
        free(new_filename);
    }
    
    free(base_name);
    return strdup("downloaded_file_unique");
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

void format_file_size(long bytes, char* buffer, size_t buffer_size)
{
    const char* units[] = {"B", "KB", "MB", "GB"};
    int unit_index = 0;
    double size = (double)bytes;
    
    while (size >= 1024.0 && unit_index < 3) {
        size /= 1024.0;
        unit_index++;
    }
    
    if (unit_index == 0) {
        snprintf(buffer, buffer_size, "%ld %s", bytes, units[unit_index]);
    } else {
        snprintf(buffer, buffer_size, "%.2f %s", size, units[unit_index]);
    }
}

int main(int argc, char* argv[])
{
    char* url = NULL;
    char* output_file = NULL;
    int verbose = 0;
    int show_help = 0;
    
    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_help = 1;
            break;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                output_file = argv[++i];
            } else {
                fprintf(stderr, "错误: -o 选项需要一个文件名参数\n");
                return 1;
            }
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "错误: 未知选项 %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else if (!url) {
            url = argv[i];
        } else if (!output_file) {
            output_file = argv[i];
        } else {
            fprintf(stderr, "错误: 参数过多\n");
            print_usage(argv[0]);
            return 1;
        }
    }
    
    if (show_help) {
        print_usage(argv[0]);
        return 0;
    }
    
    if (!url) {
        fprintf(stderr, "错误: 请提供下载链接\n");
        print_usage(argv[0]);
        return 1;
    }
    
    // 检查是否为 HTTPS URL
    if (strncmp(url, "https://", 8) != 0) {
        fprintf(stderr, "错误: 只支持 HTTPS 协议的 URL\n");
        return 1;
    }
    
    // 确定输出文件名
    char* final_output_file = NULL;
    if (output_file) {
        final_output_file = get_unique_filename(output_file);
    } else {
        char* extracted_name = extract_filename_from_url(url);
        final_output_file = get_unique_filename(extracted_name);
        free(extracted_name);
    }
    
    if (verbose) {
        printf("下载 URL: %s\n", url);
        printf("保存到: %s\n", final_output_file);
        printf("开始下载...\n");
    } else {
        printf("正在下载 %s ...\n", url);
    }
    
    // 执行下载
    int result = https_download(url, final_output_file);
    
    if (result == 0) {
        long file_size = get_file_size(final_output_file);
        char size_str[64];
        format_file_size(file_size, size_str, sizeof(size_str));
        
        printf("✓ 下载完成!\n");
        printf("文件保存为: %s\n", final_output_file);
        printf("文件大小: %s\n", size_str);
        
        if (verbose) {
            printf("下载状态: 成功\n");
        }
    } else {
        fprintf(stderr, "✗ 下载失败 (错误代码: %d)\n", result);
        fprintf(stderr, "请检查:\n");
        fprintf(stderr, "  - 网络连接是否正常\n");
        fprintf(stderr, "  - URL 是否正确\n");
        fprintf(stderr, "  - 是否有写入文件的权限\n");
        
        // 清理可能创建的空文件
        if (file_exists(final_output_file) && get_file_size(final_output_file) == 0) {
            unlink(final_output_file);
        }
    }
    
    free(final_output_file);
    return result;
}
