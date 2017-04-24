 #include <sys/epoll.h>

typedef struct zyziApiState {
    int epfd;
    struct epoll_event *events;
} zyziApiState;

static int zyziApiCreate(zyziEventLoop *eventLoop)
{
    zyziApiState *state = zyzi_malloc(sizeof(zyziApiState));

    if (state) {

        state->events = zyzi_malloc(sizeof(struct epoll_event) * eventLoop->size);

        if (state->events) {
            state->epfd = epoll_create(1024);
            eventLoop->apiData = state;

            return ZYZI_OK;
        }
    }

err:
    zyzi_free(state);
    return ZYZI_ERR;
}

static int zyziApiResize(zyziEventLoop *eventLoop , int nsize)
{
    zyziApiState *state = eventLoop->apiData;

    state->events = zyzi_realloc(state->events , sizeof(struct epoll_event) * nsize);

    return ZYZI_OK;
}

static void zyziApiFree(zyziEventLoop *eventLoop)
{
    zyziApiState *state = eventLoop->apiData;

    zyzi_free(state->events);
    zyzi_free(eventLoop->apiData);
}

static int zyziApiAddEvent(zyziEventLoop *eventLoop , int fd , int mask)
{
    zyziApiState *state = eventLoop->apiData;

    struct epoll_event ev[1];

    ev->events = 0;

    if (mask & ZYZI_READABLE) ev->events |= EPOLLIN;
    if (mask & ZYZI_WRITABLE) ev->events |= EPOLLOUT;

    ev->data.u64 = 0;
    ev->data.fd = fd;

    if (epoll_ctl(state->epfd , EPOLL_CTL_ADD , fd , ev) == -1) return ZYZI_ERR;

    return ZYZI_OK;
}

static void zyziApiDelEvent(zyziEventLoop *eventLoop , int fd , int mask)
{
    zyziApiState *state = eventLoop->apiData;

    struct epoll_event ev[1];

    ev->events = 0;

    if (mask & ZYZI_READABLE) ev->events |= EPOLLIN;
    if (mask & ZYZI_WRITABLE) ev->events |= EPOLLOUT;

    ev->data.u64 = 0;
    ev->data.fd = fd;

    if (mask != ZYZI_NONE) {
        epoll_ctl(state->epfd , EPOLL_CTL_MOD , fd , ev);
    } else {
        epoll_ctl(state->epfd , EPOLL_CTL_DEL , fd , ev);
    }
}

static int zyziApiPoll(zyziEventLoop *eventLoop ,struct timeval *tval)
{
    int nready , i;
    zyziApiState *state = eventLoop->apiData;

    nready = epoll_wait(state->epfd , state->events , eventLoop->size , -1);

    if (nready > 0) {
        for (i = 0 ; i < nready ; i++) {
            int mask = ZYZI_NONE;
            struct epoll_event *ev = &state->events[i];

            zyziFileEvent *fe = &eventLoop->events[i];

            if (ev->events & EPOLLIN)  mask |= ZYZI_READABLE;
            if (ev->events & EPOLLOUT) mask |= ZYZI_WRITABLE;
            if (ev->events & EPOLLERR) mask |= ZYZI_WRITABLE;
            if (ev->events & EPOLLHUP) mask |= ZYZI_WRITABLE;

            eventLoop->fired[i].fd = ev->data.fd;
            eventLoop->fired[i].mask = mask;
        }
    }

    return nready;
}
