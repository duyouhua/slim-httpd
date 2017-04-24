#ifndef _ZYZI_HTTP_H_
#define _ZYZI_HTTP_H_

#include <sys/types.h>

typedef struct zyzi_http_head {
    char   *method;
    char   *url;
    char   *proto;
    char   *host;
    char   *user_agent;
    char   *conn;

    char   *head_text;
    size_t head_length;

    char   *content_text;
    size_t content_length;

    void   *allocated[6];

    char   *msg;
    int     code;
    int    clnt_fd;
} zyzi_http_head;

void sendHttpErrorPage(zyzi_http_head *head);

void zyziHttpSend(zyzi_http_head *head);

zyzi_http_head *zyziProcessHttpRequest(char *head_txt , size_t head_len ,int clnt_fd);

void zyziHttpHeadDestroy(zyzi_http_head *head);

#endif // _KUMA_HTTP_H_
