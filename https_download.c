#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/platform.h"
#include "system_abstraction.h"

#define HTTPS_DOWNLOAD_BUF_SIZE    512
#define HTTPS_HEADER_BAK_LEN       32
#define HTTPS_MAX_HOST_LEN         256
#define HTTPS_MAX_RESOURCE_LEN     2048

typedef struct {
    uint32_t status_code;
    uint32_t header_len;
    uint8_t *body;
    uint32_t body_len;
    uint8_t *header_bak;
    uint32_t parse_status;
} https_response_result_t;

typedef struct {
    char *redirect;
    int redirect_len;
    uint16_t redirect_server_port;
    char *redirect_server_host;
    char *redirect_resource;
} https_redirect_info_t;

/////////////////////////////////////////////////////////////////////////
///////////////////////// HTTPS Download Functions /////////////////////
/////////////////////////////////////////////////////////////////////////

static char *https_itoa(int value)
{
    char *val_str;
    int tmp = value, len = 1;

    while((tmp /= 10) > 0)
        len++;

    val_str = (char *) sys_malloc(len + 1);
    if (val_str) {
        sprintf(val_str, "%d", value);
    }

    return val_str;
}

static int https_parse_url(char *url, char *host, uint16_t *port, char *resource)
{
    if(url){
        char *https = NULL, *pos = NULL;
        size_t len;

        https = strstr(url, "https://");
        if(https) // remove https
            url += strlen("https://");
        memset(host, 0, HTTPS_MAX_HOST_LEN);

        pos = strstr(url, ":");    // get port
        if(pos){
            len = (size_t)(pos - url);
            if(len >= HTTPS_MAX_HOST_LEN) {
                SYS_LOG_ERROR("Host name too long: %zu bytes", len);
                return -1;
            }
            memcpy(host, url, len);
            pos += 1;
            *port = atoi(pos);
        }else{
            pos = strstr(url, "/");
            if(pos){
                len = (size_t)(pos - url);
                if(len >= HTTPS_MAX_HOST_LEN) {
                    SYS_LOG_ERROR("Host name too long: %zu bytes", len);
                    return -1;
                }
                memcpy(host, url, len);
                url = pos;
            }
            *port = 443;  // HTTPS default port
        }
        printf("HTTPS server: %s\n", host);
        printf("HTTPS port: %d\n", *port);

        memset(resource, 0, HTTPS_MAX_RESOURCE_LEN);
        pos = strstr(url, "/");
        if(pos){
            len = strlen(pos + 1);
            if(len >= HTTPS_MAX_RESOURCE_LEN) {
                SYS_LOG_ERROR("Resource path too long: %zu bytes (max: %d)", len, HTTPS_MAX_RESOURCE_LEN - 1);
                return -1;
            }
            memcpy(resource, pos + 1, len);
        }
        // Only print first 100 chars of resource to avoid clutter
        if(strlen(resource) > 100) {
            printf("HTTPS resource: %.100s... (%zu bytes)\n", resource, strlen(resource));
        } else {
            printf("HTTPS resource: %s\n", resource);
        }

        return 0;
    }
    return -1;
}

static int https_parse_response(unsigned char *response, unsigned int response_len, https_response_result_t *result) 
{
    uint32_t i, p, q, m;
    uint32_t header_end = 0;

    //Get status code
    if(0 == result->parse_status){//didn't get the http response
        uint8_t status[4] = {0};
        i = p = q = m = 0;
        for (; i < response_len; ++i) {
            if (' ' == response[i]) {
                ++m;
                if (1 == m) {//after HTTP/1.1
                    p = i;
                } 
                else if (2 == m) {//after status code
                    q = i;
                    break;
                }
            }
        }
        if (!p || !q || q-p != 4) {//Didn't get the status code
            return -1;
        }
        memcpy(status, response+p+1, 3);//get the status code
        result->status_code = atoi((char const *)status);
        if(result->status_code == 200)
            result->parse_status = 1;
        else if(result->status_code == 302)
        {
            char *tmp=NULL; 
            const char *location1 = "LOCATION";
            const char *location2 = "Location";
            printf("HTTPS response 302:%s \n", response);

            if((tmp =strstr((char *)response, location1)) ||(tmp=strstr((char *)response, location2)))
            {
                // Handle redirect if needed
                printf("HTTPS redirect detected\n");
                return -1;
            }
            return -1;
        }
        else{
            SYS_LOG_ERROR("The HTTPS response status code is %d", result->status_code);
            return -1;
        }
    }

    //if didn't receive the full http header
    if(3 == result->parse_status){//didn't get the http response
        p = q = 0;
        for (i = 0; i < response_len; ++i) {
            if (response[i] == '\r' && response[i+1] == '\n' &&
                    response[i+2] == '\r' && response[i+3] == '\n') {//the end of header
                header_end = i+4;
                result->parse_status = 4;
                result->header_len = header_end;
                result->body = response + header_end;
                break;
            }
        }
        if (3 == result->parse_status) {//Still didn't receive the full header    
            result->header_bak = sys_malloc(HTTPS_HEADER_BAK_LEN + 1);
            if (result->header_bak) {
                memset(result->header_bak, 0, HTTPS_HEADER_BAK_LEN + 1);
                memcpy(result->header_bak, response + response_len - HTTPS_HEADER_BAK_LEN, HTTPS_HEADER_BAK_LEN);
            }
        }
    }

    //Get Content-Length
    if(1 == result->parse_status){//didn't get the content length
        const char *content_length_buf1 = "CONTENT-LENGTH";
        const char *content_length_buf2 = "Content-Length";
        const uint32_t content_length_buf_len = strlen((const char*)content_length_buf1);
        p = q = 0;

        for (i = 0; i < response_len; ++i) {
            if (response[i] == '\r' && response[i+1] == '\n') {
                q = i;//the end of the line
                if (!memcmp(response+p, content_length_buf1, content_length_buf_len) ||
                        !memcmp(response+p, content_length_buf2, content_length_buf_len)) {//get the content length
                    unsigned int j1 = p+content_length_buf_len, j2 = q-1;
                    while ( j1 < q && (*(response+j1) == ':' || *(response+j1) == ' ') ) ++j1;
                    while ( j2 > j1 && *(response+j2) == ' ') --j2;
                    uint8_t len_buf[12] = {0};
                    memcpy(len_buf, response+j1, j2-j1+1);
                    result->body_len = atoi((char const *)len_buf);
                    result->parse_status = 2;
                }
                p = i+2;
            }
            if (response[i] == '\r' && response[i+1] == '\n' &&
                    response[i+2] == '\r' && response[i+3] == '\n') {//Get the end of header
                header_end = i+4;//p is the start of the body
                if(result->parse_status == 2){//get the full header and the content length
                    result->parse_status = 4;
                    result->header_len = header_end;
                    result->body = response + header_end;
                }
                else {//there are no content length in header    
                    SYS_LOG_ERROR("No Content-Length in header");
                    return -1;
                }
                break;
            }    
        }

        if (1 == result->parse_status) {//didn't get the content length and the full header
            result->header_bak = sys_malloc(HTTPS_HEADER_BAK_LEN + 1);
            if (result->header_bak) {
                memset(result->header_bak, 0, HTTPS_HEADER_BAK_LEN + 1);
                memcpy(result->header_bak, response + response_len - HTTPS_HEADER_BAK_LEN, HTTPS_HEADER_BAK_LEN);
            }
        }
        else if (2 == result->parse_status) {//didn't get the full header but get the content length
            result->parse_status = 3;
            result->header_bak = sys_malloc(HTTPS_HEADER_BAK_LEN + 1);
            if (result->header_bak) {
                memset(result->header_bak, 0, HTTPS_HEADER_BAK_LEN + 1);
                memcpy(result->header_bak, response + response_len - HTTPS_HEADER_BAK_LEN, HTTPS_HEADER_BAK_LEN);
            }
        }
    }

    return result->parse_status;
}

static const char* https_get_ssl_error_string(int error_code)
{
    switch(error_code) {
        case -0x7780: return "Fatal alert received from server";
        case -0x7200: return "Connection was reset by peer";
        case -0x7180: return "SSL connection was closed by peer";
        case -0x6980: return "Connection timeout";
        case -0x6900: return "SSL read timeout";
        case -0x2700: return "Certificate verification failed";
        case -0x7900: return "Invalid record/message";
        case -0x7080: return "SSL connection closed cleanly";
        case -0x6800: return "SSL want read";
        case -0x6780: return "SSL want write";
        default: return "Unknown SSL error";
    }
}

static int https_read_socket(mbedtls_ssl_context *ssl, uint8_t *receive_buf, int buf_len)
{
    int bytes_rcvd = -1; 
    int retry_count = 0;
    const int max_retries = 3;

    memset(receive_buf, 0, buf_len);  

    do {
        bytes_rcvd = mbedtls_ssl_read(ssl, receive_buf, buf_len);
        
        if(bytes_rcvd > 0) {
            return bytes_rcvd; // Success
        }
        
        // Handle specific SSL errors
        if(bytes_rcvd == MBEDTLS_ERR_SSL_WANT_READ || bytes_rcvd == MBEDTLS_ERR_SSL_WANT_WRITE) {
            // Non-blocking operation would block, wait and retry
            sys_delay_ms(10); // Wait 10ms
            retry_count++;
            continue;
        }
        
        if(bytes_rcvd == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
            // Peer closed connection cleanly
            SYS_LOG_INFO("SSL connection closed cleanly by peer");
            return 0; // End of data
        }
        
        if(bytes_rcvd == MBEDTLS_ERR_SSL_CONN_EOF) {
            // Connection closed by peer
            SYS_LOG_INFO("SSL connection closed by peer");
            return 0; // End of data
        }
        
        // For other errors, retry a few times
        retry_count++;
        if(retry_count < max_retries) {
            SYS_LOG_ERROR("SSL read failed [%d]: %s, retrying (%d/%d)", 
                   bytes_rcvd, https_get_ssl_error_string(bytes_rcvd), retry_count, max_retries);
            sys_delay_ms(100); // Wait 100ms before retry
        }
        
    } while(retry_count < max_retries);

    SYS_LOG_ERROR("SSL read failed after %d retries [%d]: %s",  
           max_retries, bytes_rcvd, https_get_ssl_error_string(bytes_rcvd));
    return -2;
}

int https_download(char *url, const char *save_path)
{
    int ret = -1;
    char host[HTTPS_MAX_HOST_LEN] = {0};
    char resource[HTTPS_MAX_RESOURCE_LEN] = {0};
    uint16_t port = 443;

    unsigned char *alloc = NULL;
    unsigned char *request = NULL;
    int alloc_buf_size = HTTPS_DOWNLOAD_BUF_SIZE;
    int read_bytes = 0;
    uint32_t writelen = 0;
    https_response_result_t rsp_result = {0};
    uint32_t idx = 0;

    mbedtls_net_context server_fd;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    char *port_str = NULL;

    sys_file_t save_file = {0};
    uint32_t nwrites = 0;
    uint32_t total_written = 0;

    SYS_LOG_INFO("[HTTPS] Starting download from: %s", url);

    if(https_parse_url(url, host, &port, resource) != 0) {
        SYS_LOG_ERROR("[HTTPS] Failed to parse URL");
        goto https_download_exit;
    }

    alloc = (unsigned char *)sys_malloc(alloc_buf_size);
    if(!alloc){
        SYS_LOG_ERROR("[HTTPS] Alloc buffer failed");
        goto https_download_exit;
    }

    // Initialize mbedTLS structures
    mbedtls_net_init(&server_fd);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    // Seed the random number generator
    const char *pers = "https_download";
    if((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                   (const unsigned char *) pers, strlen(pers))) != 0) {
        SYS_LOG_ERROR("[HTTPS] mbedtls_ctr_drbg_seed failed: -0x%x", -ret);
        goto https_download_exit;
    }

    port_str = https_itoa(port);
    if((ret = mbedtls_net_connect(&server_fd, host, port_str, MBEDTLS_NET_PROTO_TCP)) != 0) {
        SYS_LOG_ERROR("[HTTPS] mbedtls_net_connect ret(%d)", ret);
        goto https_download_exit;
    }

    mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);
    if((ret = mbedtls_ssl_config_defaults(&conf,
                    MBEDTLS_SSL_IS_CLIENT,
                    MBEDTLS_SSL_TRANSPORT_STREAM,
                    MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {

        SYS_LOG_ERROR("[HTTPS] mbedtls_ssl_config_defaults ret(%d)", ret);
        goto https_download_exit;
    }

    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
    
    // Force TLS 1.2 only (MAJOR_VERSION_3 + MINOR_VERSION_3 = TLS 1.2)
    mbedtls_ssl_conf_min_version(&conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
    mbedtls_ssl_conf_max_version(&conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
    
    // Set read timeout to handle slow connections
    mbedtls_ssl_conf_read_timeout(&conf, 30000); // 30 seconds timeout
    
    // Enable more cipher suites for better compatibility
    static const int ciphersuites[] = {
        MBEDTLS_TLS_RSA_WITH_AES_256_CBC_SHA256,
        MBEDTLS_TLS_RSA_WITH_AES_128_CBC_SHA256,
        MBEDTLS_TLS_RSA_WITH_AES_256_CBC_SHA,
        MBEDTLS_TLS_RSA_WITH_AES_128_CBC_SHA,
        MBEDTLS_TLS_RSA_WITH_3DES_EDE_CBC_SHA,
        0
    };
    mbedtls_ssl_conf_ciphersuites(&conf, ciphersuites);

    if((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0) {
        SYS_LOG_ERROR("[HTTPS] mbedtls_ssl_setup ret(%d)", ret);
        goto https_download_exit;
    }

    // Set hostname for SNI (Server Name Indication)
    if((ret = mbedtls_ssl_set_hostname(&ssl, host)) != 0) {
        SYS_LOG_ERROR("[HTTPS] mbedtls_ssl_set_hostname ret(%d)", ret);
        goto https_download_exit;
    }

    // SSL handshake with retry mechanism
    int handshake_retry = 0;
    const int max_handshake_retries = 3;
    
    do {
        ret = mbedtls_ssl_handshake(&ssl);
        if(ret == 0) {
            break; // Success
        }
        
        handshake_retry++;
        SYS_LOG_ERROR("[HTTPS] mbedtls_ssl_handshake attempt %d failed ret(-0x%x): %s", handshake_retry, -ret, https_get_ssl_error_string(ret));
        
        // Provide more detailed error information
        if(ret == -0x7780) {
            SYS_LOG_ERROR("[HTTPS] Possible causes: cipher suite mismatch, certificate issues, or SNI problems");
        }
        
        if(handshake_retry < max_handshake_retries) {
            SYS_LOG_INFO("[HTTPS] Retrying SSL handshake in 1 second...");
            sys_delay_ms(1000); // Wait 1 second before retry
            
            // Reset SSL context for retry
            mbedtls_ssl_session_reset(&ssl);
        }
    } while(handshake_retry < max_handshake_retries);
    
    if(ret != 0) {
        SYS_LOG_ERROR("[HTTPS] SSL handshake failed after %d attempts", max_handshake_retries);
        goto https_download_exit;
    }

    SYS_LOG_INFO("[HTTPS] SSL ciphersuite %s", mbedtls_ssl_get_ciphersuite(&ssl));

    // send https request
    idx = 0;
    request = (unsigned char *) sys_malloc(strlen("GET /") + strlen(resource) + strlen(" HTTP/1.1\r\nHost: ") 
            + strlen(host) + strlen("\r\n\r\n") + 1);
    if (!request) {
        SYS_LOG_ERROR("[HTTPS] Failed to allocate request buffer");
        goto https_download_exit;
    }
    sprintf((char*)request, "GET /%s HTTP/1.1\r\nHost: %s\r\n\r\n", resource, host);        
    ret = mbedtls_ssl_write(&ssl, request, strlen((char*)request));
    if(ret < 0){
        SYS_LOG_ERROR("[HTTPS] Send HTTPS request failed");
        goto https_download_exit;
    }

    // parse https response
    while (3 >= rsp_result.parse_status){//still read header
        if(0 == rsp_result.parse_status){//didn't get the http response
            memset(alloc, 0, alloc_buf_size);
            read_bytes = mbedtls_ssl_read(&ssl, alloc, alloc_buf_size);
            if(read_bytes <= 0){
                SYS_LOG_ERROR("[HTTPS] Read socket failed");
                goto https_download_exit;
            }
            idx = read_bytes;
            memset(&rsp_result, 0, sizeof(rsp_result));
            if(https_parse_response(alloc, idx, &rsp_result) == -1){
                goto https_download_exit;
            }
        } else if ((1 == rsp_result.parse_status) || (3 == rsp_result.parse_status)){//just get the status code
            memset(alloc, 0, alloc_buf_size);
            if (rsp_result.header_bak) {
                memcpy(alloc, rsp_result.header_bak, HTTPS_HEADER_BAK_LEN);
                sys_free(rsp_result.header_bak);
                rsp_result.header_bak = NULL;
            }
            read_bytes = mbedtls_ssl_read(&ssl, alloc + HTTPS_HEADER_BAK_LEN, (alloc_buf_size - HTTPS_HEADER_BAK_LEN));
            if(read_bytes <= 0){
                SYS_LOG_ERROR("[HTTPS] Read socket failed");
                goto https_download_exit;
            }
            idx = read_bytes + HTTPS_HEADER_BAK_LEN;
            if (https_parse_response(alloc, read_bytes + HTTPS_HEADER_BAK_LEN, &rsp_result) == -1){
                goto https_download_exit;
            }
        }
    }

    if (0 == rsp_result.body_len) {
        SYS_LOG_ERROR("[HTTPS] File size = 0 !");
        goto https_download_exit;
    } else {
        SYS_LOG_INFO("[HTTPS] Download file begin, total size : %d", rsp_result.body_len);
    }    

    // open save file
    if (sys_file_open(&save_file, save_path, SYS_FILE_CREATE_ALWAYS | SYS_FILE_WRITE) != SYS_FILE_OK) {
        SYS_LOG_ERROR("[HTTPS] Cannot create file: %s", save_path);
        goto https_download_exit;
    }

    writelen = idx - rsp_result.header_len;
    // remove https header_len from alloc
    memmove(alloc, alloc + rsp_result.header_len, writelen);
    memset(alloc + writelen, 0, rsp_result.header_len);

    // write received data
    if(writelen > 0) {
        if (sys_file_write(&save_file, alloc, writelen, &nwrites) != SYS_FILE_OK || nwrites != writelen) {
            SYS_LOG_ERROR("[HTTPS] Write file failed");
            goto https_download_exit;
        }
        total_written += nwrites;
    }

    // continue download remaining data
    int consecutive_failures = 0;
    const int max_consecutive_failures = 5;
    
    while(total_written < rsp_result.body_len) {
        read_bytes = https_read_socket(&ssl, alloc, HTTPS_DOWNLOAD_BUF_SIZE);
        
        if(read_bytes < 0) {
            consecutive_failures++;
            SYS_LOG_ERROR("[HTTPS] Read data failed (attempt %d/%d)", consecutive_failures, max_consecutive_failures);
            
            if(consecutive_failures >= max_consecutive_failures) {
                SYS_LOG_ERROR("[HTTPS] Too many consecutive read failures, aborting download");
                break;
            }
            
            // Wait before next attempt
            sys_delay_ms(500);
            continue;
        }
        
        if(read_bytes == 0) {
            // Connection closed by peer, check if we got all data
            if(total_written >= rsp_result.body_len) {
                SYS_LOG_INFO("[HTTPS] Download completed, connection closed by peer");
                break;
            } else {
                SYS_LOG_ERROR("[HTTPS] Unexpected connection close: %d/%d bytes received", 
                             total_written, rsp_result.body_len);
                break;
            }
        }
        
        // Reset failure counter on successful read
        consecutive_failures = 0;

        // ensure not exceed file size
        if(total_written + (uint32_t)read_bytes > rsp_result.body_len) {
            read_bytes = (int)(rsp_result.body_len - total_written);
        }

        if (sys_file_write(&save_file, alloc, (uint32_t)read_bytes, &nwrites) != SYS_FILE_OK || nwrites != (uint32_t)read_bytes) {
            SYS_LOG_ERROR("[HTTPS] Write file failed: wrote %u/%d bytes", nwrites, read_bytes);
            break;
        }
        total_written += nwrites;

        // show progress more frequently for better user feedback
        if(total_written % (HTTPS_DOWNLOAD_BUF_SIZE * 5) == 0 || total_written == rsp_result.body_len) {
            SYS_LOG_INFO("[HTTPS] Downloaded: %d/%d (%d%%)", 
                    total_written, rsp_result.body_len, 
                    (total_written * 100) / rsp_result.body_len);
        }
    }

    if(total_written == rsp_result.body_len) {
        SYS_LOG_INFO("[HTTPS] Download completed successfully: %d bytes", total_written);
        ret = 0;
    } else {
        SYS_LOG_ERROR("[HTTPS] Download incomplete: %d/%d bytes", total_written, rsp_result.body_len);
    }

https_download_exit:
    if(alloc)
        sys_free(alloc);
    if(request)
        sys_free(request);
    if(port_str)
        sys_free(port_str);
    if(rsp_result.header_bak)
        sys_free(rsp_result.header_bak);

    sys_file_close(&save_file);

    mbedtls_net_free(&server_fd);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_entropy_free(&entropy);
    mbedtls_ctr_drbg_free(&ctr_drbg);

    return ret;
}
