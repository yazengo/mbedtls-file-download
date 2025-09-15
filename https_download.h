#ifndef HTTPS_DOWNLOAD_H
#define HTTPS_DOWNLOAD_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Download a file from an HTTPS URL
 * 
 * @param url The HTTPS URL to download from
 * @param save_path The local path where the file should be saved
 * @return 0 on success, negative value on error
 */
int https_download(char *url, const char *save_path);

#ifdef __cplusplus
}
#endif

#endif // HTTPS_DOWNLOAD_H
