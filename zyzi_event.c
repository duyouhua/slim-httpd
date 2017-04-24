#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "zyzi_log.h"
#include "zyzi_event.h"
#include "zyzi_malloc.h"
#include "slim-httpd.h"

#ifdef HAVE_KQUEUE
    #include "zyzi_event_kqueue.c"
#else
    #ifdef HAVE_EPOLL
        #include "zyzi_event_epoll.c"
    #else
        #include "zyzi_event_select.c"
    #endif // HAVE_EPOLL
#endif // HAVE_KQUEUE

static void set_non_blocking(int fd)
{
    int mask;
    if ((mask = fcntl(fd , F_GETFL)) == -1) {
        zyzi_logx(ZYZI_LOG_ERRO , "fcntl 'F_GETFL' failed");
    }

    mask |= O_NONBLOCK;
    if (fcntl(fd , F_SETFL , mask) == -1) {
        zyzi_logx(ZYZI_LOG_ERRO , "fcntl 'F_SETFL' failed");
    }
}

zyziEventLoop *zyziCreateEventLoop(int size)
{
    zyziEventLoop *eventLoop = zyzi_malloc(size * sizeof(zyziEventLoop));

    if (!eventLoop) goto err;

    eventLoop->events =  zyzi_malloc(size * sizeof(zyziFileEvent));
    eventLoop->fired  =  zyzi_malloc(size * sizeof(zyziFireEvent));

    if (!eventLoop->events || !eventLoop->fired) goto err;

    eventLoop->size = size;

    if (zyziApiCreate(eventLoop) == ZYZI_ERR) goto err;

    int i;
    for (i = 0 ; i <size ; i++) {
        eventLoop->events[i].mask = ZYZI_NONE;
    }

    eventLoop->stop = 0;

    return eventLoop;

err:
    if (eventLoop) {
        zyzi_free(eventLoop->events);
        zyzi_free(eventLoop->fired);
        zyzi_free(eventLoop);
    }

    return NULL;
}

void zyziEventStop(zyziEventLoop *eventLoop)
{
    eventLoop->stop = 1;
}

int zyziResizeEventLoop(zyziEventLoop *eventLoop , int nsize)
{
    if (nsize == eventLoop->size) return ZYZI_OK;
    if (nsize < eventLoop->maxfd) return ZYZI_ERR;

    if (zyziApiResize(eventLoop , nsize) == -1) return ZYZI_ERR;

    eventLoop->events = zyzi_realloc(eventLoop->events , nsize * sizeof(zyziFileEvent));
    eventLoop->fired  = zyzi_realloc(eventLoop->fired  , nsize * sizeof(zyziFireEvent));

    eventLoop->size = zyzi_malloc_size(eventLoop->events);

    int i;
    for (i = eventLoop->maxfd + 1 ; i < nsize ; i++) {
        eventLoop->events[i].mask = ZYZI_NONE;
    }

    return ZYZI_OK;
}

int zyziEventLoopSize(zyziEventLoop *eventLoop)
{
    return eventLoop->size;
}

void zyziDeleteEventLoop(zyziEventLoop *eventLoop)
{
    zyziApiFree(eventLoop);

    zyzi_free(eventLoop->events);
    zyzi_free(eventLoop->fired);
    zyzi_free(eventLoop);
}

int zyziCreateEventFile(zyziEventLoop *eventLoop , int fd , int mask ,
                        fileProc *proc , void *clntData)
{
    if (fd >= eventLoop->size) {
        errno = ERANGE;
        return ZYZI_ERR;
    }

    zyziFileEvent *fe = &eventLoop->events[fd];

    if (zyziApiAddEvent(eventLoop , fd , mask) == ZYZI_ERR) {
        return ZYZI_ERR;
    }

    fe->mask |= mask;
    if (mask & ZYZI_READABLE) fe->rproc = proc;
    if (mask & ZYZI_WRITABLE) fe->wproc = proc;

    fe->clntData = clntData;

    if (fd > eventLoop->maxfd) {
        eventLoop->maxfd = fd;
    }

    set_non_blocking(fd);

    return ZYZI_OK;
}

void zyziDeleteEventFile(zyziEventLoop *eventLoop , int fd , int mask)
{
    if (fd >= eventLoop->size) return;

    zyziFileEvent *fe = &eventLoop->events[fd];

    fe->mask &= (~mask);

    if (fd == eventLoop->maxfd && fe->mask == ZYZI_NONE) {
        int i;
        for (i = eventLoop->maxfd - 1; i >= 0 ; i--) {
            if (eventLoop->events[i].mask != ZYZI_NONE) break;
        }
        eventLoop->maxfd = i;
    }

    zyziApiDelEvent(eventLoop , fd , mask);
}

static void zyziEventHandler(zyziEventLoop *eventLoop , int nready)
{
    int i;
    for (i = 0 ; i < nready ; i++) {
        int fired = 0;
        int fd   = eventLoop->fired[i].fd;
        int mask = eventLoop->fired[i].mask;

        zyziFileEvent *fe = &eventLoop->events[fd];

        if (mask | ZYZI_READABLE) {
            fired = 1;

            fe->rproc(eventLoop , fd , mask , fe->clntData);
        }

        if (mask | ZYZI_WRITABLE) {
            if (!fired && fe->rproc != fe->wproc) {
                fe->wproc(eventLoop , fd , mask , fe->clntData);
            }
        }
    }
}

void zyziMain(zyziEventLoop *eventLoop)
{
    int nready;

    while (!eventLoop->stop) {
        nready = zyziApiPoll(eventLoop , NULL);

        if (nready > 0) {
            zyziEventHandler(eventLoop , nready);
        }
    }
}
