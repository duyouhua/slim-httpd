#ifndef _ZYZI_LOG_H_
#define _ZYZI_LOG_H_

#define ZYZI_LOG_DEBUG 0
#define ZYZI_LOG_WARN  1
#define ZYZI_LOG_INFO  2
#define ZYZI_LOG_ERRO  3

static int daemon_proc = 0;

void zyzi_create_log(const char *filename);
void zyzi_log (int level, const char *s , ...);
void zyzi_logx(int level, const char *s , ...);
void zyzi_close_log();

#endif // _ZYZI_LOG_H_
