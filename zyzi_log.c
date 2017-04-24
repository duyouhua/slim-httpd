#include "zyzi_log.h"

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static char *log_file;

static char *log_level[] = {
    "DEBUG" , /* ZYZI_LOG_DEBUG */
    "WARN"  , /* ZYZI_LOG_WARN */
    "INFO"  , /* ZYZI_LOG_INFO */
    "ERRO"  , /* ZYZI_LOG_ERRO */
    NULL
};

#define time_now() ({\
    time_t _tick = time(NULL);\
    char *s = ctime(&_tick);\
    s[strlen(s) - 1] = '\0';\
    s;\
})

void zyzi_create_log(const char *filename)
{
    if (daemon_proc) {
        log_file = (void *) filename;
    }
}

void zyzi_log(int level, const char *fmt , ...)
{
	va_list		ap;
	va_start(ap, fmt);
    
    FILE *fp = stdout;
    
    if (daemon_proc) {
        fp = fopen(log_file , "a+");
    }

    if (level == ZYZI_LOG_DEBUG) {
#ifndef NDEBUG
        fprintf(fp , "[DEBUG] [%s] " , time_now());
#else
        return;
#endif // NDEBUG
    } else {
        fprintf(fp , "[%s] [%s] " , log_level[level] , time_now());
    }

    vfprintf(fp , fmt , ap);
    fputc('\n' , fp);

    if (daemon_proc) {
        fclose(fp);
    }
    
    va_end(ap);
}

void zyzi_logx(int level, const char *fmt , ...)
{
	va_list		ap;
	va_start(ap, fmt);

    FILE *fp = stdout;
    
    if (daemon_proc) {
        fp = fopen(log_file , "a+");
    }
    
    if (level == ZYZI_LOG_DEBUG) {
#ifndef NDEBUG
        fprintf(fp , "[DEBUG] [%s] " , time_now());
#else
        return;
#endif // NDEBUG
    } else {
        fprintf(fp , "[%s] [%s] " , log_level[level] , time_now());
    }

    vfprintf(fp , fmt , ap);

    fprintf(fp , ": %s\n" , strerror(errno));
    
    if (daemon_proc) {
        fclose(fp);
    }
    
    va_end(ap);
}

void zyzi_close_log()
{
    return;
}
