#include <stdio.h>
#include "log_disk.h"

static void str_replace(const char* find, const char* replace, char* buffer)
{
    size_t find_len    = strlen(find);
    size_t replace_len = strlen(replace);
    size_t str_len     = strlen(buffer);
    char *search_pos   = buffer;
    char *found_pos;

    while ((found_pos = strstr(search_pos, find)) != NULL) {
        if (replace_len != find_len) {
            memmove(found_pos + replace_len, found_pos + find_len, strlen(found_pos + find_len) + 1);
            str_len += replace_len - find_len;
        }
        memcpy(found_pos, replace, replace_len);
        search_pos = found_pos + replace_len;
    }
    buffer[str_len] = '\0';
}

static void str_replace_num(const char* find, const int replace, const int width, char* buffer) {
    char repl[10], repl_template[10];
    sprintf(repl_template, "%%0%dd", width);
    sprintf(repl, repl_template, replace);
    str_replace(find, repl, buffer);
}

static void get_date(char* buffer) {
    char date_format[] = "Y-m-d_H-i-s_v";
    strcpy(buffer, date_format);

    SYSTEMTIME st;
    GetSystemTime(&st);

    str_replace_num("Y", st.wYear,         4, buffer);
    str_replace_num("m", st.wMonth,        2, buffer);
    str_replace_num("d", st.wDay,          2, buffer);
    str_replace_num("H", st.wHour,         2, buffer);
    str_replace_num("i", st.wMinute,       2, buffer);
    str_replace_num("s", st.wSecond,       2, buffer);
    str_replace_num("v", st.wMilliseconds, 3, buffer);
}

void LogSimple(const wchar_t *filename, const char *text)
{
    if (filename == NULL) return;
    
    FILE *file = _wfopen(filename, L"a");
    if (file == NULL) {
        wprintf(L"Error open log file \"%s\"\n", filename);
        return;
    }
    char date_str[32];
    get_date(date_str);
    fprintf(file, "%s,msg,%s\n", date_str, text);
    fclose(file);
}

void LogAAFBufferStat(const wchar_t *filename, const AAF_buffer_t *AAFb)
{
    if (filename == NULL) return;
    
    FILE *file = _wfopen(filename, L"a");
    if (file == NULL) {
        wprintf(L"Error open log file \"%s\"\n", filename);
        return;
    }
    char date_str[32];
    get_date(date_str);
    fprintf(file,
        "%s,rqs,%I64d,%I64d,%I64d,%I64d,%I64d,%I64d,%I64d,%I64d,%I64d,%I64d,%I64d,%I64d,%I64d\n",
        date_str,
        AAFb->request.PID, AAFb->request.hFile, AAFb->request.blockSize, AAFb->request.alignSize,
        AAFb->result.success, AAFb->result.statusCode,
        AAFb->stats.fileLength, AAFb->stats.fileFragments, AAFb->stats.allocOffset, AAFb->stats.allocTime,
        AAFb->stats.searchSkip, AAFb->stats.searchTotal, AAFb->stats.moveAttempts
    );
    fclose(file);
}

void LogAAFBufferHeaders(const wchar_t *filename)
{
    if (filename == NULL) return;

    FILE *file = _wfopen(filename, L"a");
    if (file == NULL) {
        wprintf(L"Error open log file \"%s\"\n", filename);
        return;
    }
    char date_str[32];
    get_date(date_str);
    fprintf(file,
        "%s,hdr,request.PID,request.hFile,request.blockSize,request.alignSize,result.success,result.statusCode,stats.fileLength,stats.fileFragments,stats.allocOffset,stats.allocTime,stats.searchSkip,stats.searchTotal,stats.moveAttempts\n",
        date_str
    );
    fclose(file);
}
