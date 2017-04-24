#include "zyzi_log.h"
#include "zyzi_http.h"
#include "zyzi_malloc.h"

#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "slim-httpd.h"

#define time_now() ({\
    time_t _tick = time(NULL);\
    char *s = ctime(&_tick);\
    s[strlen(s) - 1] = '\0';\
    s;\
})

static void *zyzi_http_split(zyzi_http_head *head , int left , int right)
{
    char *dst;

	if (right == head->head_length) return NULL;

    if (!(dst = zyzi_malloc(right - left + 1))) return NULL;

    memcpy(dst  , head->head_text + left , right - left);

    dst[right - left] = '\0';

	return dst;
}

static  char *zyzi_parse_field(zyzi_http_head *head , char *key)
{
    int left , right;
    char *pos = strstr(head->head_text , key);

	if (pos) {

        left = (size_t) (pos - head->head_text) + strlen(key);

        for (right = left ; right < head->head_length && head->head_text[right] != '\r' &&
                            head->head_text[right] != '\n' ; right++);
        return zyzi_http_split(head , left , right);
    }

    return NULL;
}

static int zyzi_parse_http_request(zyzi_http_head *head)
{
    int left = 0, right = 0 , i = 0;
    char *text = head->head_text;

    #define zyzi_http_split_helper(_field) do { \
        void *t = zyzi_http_split(head , left , right);\
        if (!t) return -1;\
        head->_field = t;\
        head->allocated[i++] = head->_field;\
        right++;\
    } while(0)

    for (left = 0 ; right < head->head_length && text[right] != ' ' ; right++);
    zyzi_http_split_helper(method);

    for (left = right ; right < head->head_length && text[right] != ' ' ; right++);
    zyzi_http_split_helper(url);

    for (left = right ; right < head->head_length &&text[right] != '\r' &&
           text[right] != '\n' ; right++);
    zyzi_http_split_helper(proto);

    #undef zyzi_http_split_helper

    #define zyzi_parse_field_helper(_field , _key) do { \
        void *t = zyzi_parse_field(head , _key);\
        if (!t) return -1;\
        head->_field = t;\
        head->allocated[i++] = head->_field;\
        right++;\
    } while(0)

    zyzi_parse_field_helper(host , "Host: ");
    zyzi_parse_field_helper(user_agent , "User-Agent: ");
    zyzi_parse_field_helper(conn , "Connection: ");
    #undef parse_field_helper

#if 0
    printf("method: %s\n" , head->method);
    printf("url: %s\n" , head->url);
    printf("proto: %s\n" , head->proto);
    printf("Host: %s\n" , head->host);
    printf("User-Agent: %s\n" , head->user_agent);
    printf("User-Connection: %s\n" , head->conn);
#endif

    return 0;
}

zyzi_http_head *zyziProcessHttpRequest(char *head_txt , size_t head_len ,int clnt_fd)
{
    zyzi_http_head *head = zyzi_malloc(sizeof(zyzi_http_head));

    if (!head) return NULL;

    bzero(head , sizeof(zyzi_http_head));

    head->clnt_fd     = clnt_fd;
    head->head_text   = head_txt;
    head->head_length = head_len;

    if (zyzi_parse_http_request(head) == -1) {

        head->code = 400;
        head->msg  = "Bad request";

        zyzi_log(ZYZI_LOG_ERRO , "bad request from %s" , head->host);

        sendHttpErrorPage(head);

        zyziHttpHeadDestroy(head);

        return NULL;
    }

    return head;
}

static size_t http_response_head_bulider(zyzi_http_head *head , char *res_head , size_t res_len)
{
    char *head_format = "HTTP/1.1 %d %s\r\n"
                        "Server: %s\r\n"
                        "Connection: Keep-Alive\r\n"
                        "Content-Length: %d\r\n"
                        "Content-Type: text/html\r\n\r\n";

    return snprintf(res_head , res_len , head_format , head->code , head->msg ,
                                          SERV_NAME , head->content_length);
}

void http_send_base(zyzi_http_head *head)
{
    char res_head[1024];

    size_t n = http_response_head_bulider(head , res_head , 1024);

    struct iovec iov[2];

    iov[0].iov_base = res_head;
    iov[0].iov_len  = n;

    iov[1].iov_base = head->content_text;
    iov[1].iov_len  = head->content_length;

    if (writev(head->clnt_fd , iov , 2) == -1) {
        if (errno != EWOULDBLOCK) {
            zyzi_log(ZYZI_LOG_ERRO , "response to '%s' failed" , head->host);

            return;
        }
    }

    zyzi_log(ZYZI_LOG_INFO , "response to '%s'" , head->host);
}

void sendHttpErrorPage(zyzi_http_head *head)
{
    char html[1024];

    char *fmt = "<html><head><title>%d %s</title></head><body><h1>%s</h1>%d - %s<hr>%s - %s</body></html>";

    head->content_length = snprintf(html , 1024 , fmt , head->code , head->msg , SERV_NAME , head->code ,
                                                        head->msg, SERV_NAME , time_now());
    head->content_text = html;

    http_send_base(head);
}

void zyziHttpSend(zyzi_http_head *head)
{
    if (head->url[0] == '/' && (head->url[1] == '\0' || head->url[1] == '?')) {
        head->url = "index.html";
    } else {
        char *pos = strstr(head->url , "?");

        if (pos) *pos = '\0';

        head->url = &head->url[1];
    }

    struct stat t;

    if (stat(head->url , &t) == -1) {

        head->code = 404;
        head->msg  = "Not found";

        zyzi_log(ZYZI_LOG_ERRO , "'%s' is not fond" , head->url);

        sendHttpErrorPage(head);

        return;
    }

    char *content_text = zyzi_malloc(t.st_size + 1);

    int fd = open(head->url , O_RDONLY);

    size_t n = read(fd , content_text , t.st_size);
    content_text[n] = '\0';

    head->code = 200;
    head->msg  = "OK";

    head->content_text   = content_text;
    head->content_length = n;

    zyzi_log(ZYZI_LOG_INFO , "request '/%s' from '%s' [%s]" , head->url , head->host , head->user_agent);

    http_send_base(head);
}

void zyziHttpHeadDestroy(zyzi_http_head *head)
{
    int i;
    for (i = 0 ; i < 6 ; i++) {
        zyzi_free(head->allocated[i]);
    }
}
