#include <sys/time.h>
#include <sys/event.h>
#include <sys/event.h>

typedef struct zyziApiState {
    int kq;
    struct kevent *events;
} zyziApiState;

static int zyziApiCreate(zyziEventLoop *eventLoop)
{
    zyziApiState *state = zyzi_malloc(sizeof(zyziApiState));
    state->kq = kqueue();

    if (state) {

        state->events = zyzi_malloc(sizeof(struct kevent) * eventLoop->size);

        if (state->events) {
            if (state->kq == -1) {
                zyzi_free(state->events);

                goto err;
            }

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

    state->events = zyzi_realloc(state->events , sizeof(struct kevent) * nsize);

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

    struct kevent ev[1];

    if (mask & ZYZI_READABLE) {
        EV_SET(ev , fd , EVFILT_READ , EV_ADD , 0 , 0 , NULL);
        if (kevent(state->kq , ev , 1 , NULL , 0 , NULL) == -1) return ZYZI_ERR;
    }

    if (mask & ZYZI_WRITABLE) {
        EV_SET(ev , fd , EVFILT_WRITE , EV_ADD , 0 , 0 , NULL);
        if (kevent(state->kq , ev , 1 , NULL , 0 , NULL) == -1) return ZYZI_ERR;
    }

    return ZYZI_OK;
}

static void zyziApiDelEvent(zyziEventLoop *eventLoop , int fd , int mask)
{
    zyziApiState *state = eventLoop->apiData;

    struct kevent ev[1];

    if (mask & ZYZI_READABLE) {
        EV_SET(ev , fd , EVFILT_READ , EV_DELETE , 0 , 0 , NULL);
        kevent(state->kq , ev , 1 , NULL , 0 , NULL);
    }

    if (mask & ZYZI_WRITABLE) {
        EV_SET(ev , fd , EVFILT_WRITE , EV_DELETE , 0 , 0 , NULL);
        kevent(state->kq , ev , 1 , NULL , 0 , NULL);
    }
}

static int zyziApiPoll(zyziEventLoop *eventLoop ,struct timeval *tval)
{
    int nready , i;
    zyziApiState *state = eventLoop->apiData;

    nready = kevent(state->kq , NULL , 0 , state->events , eventLoop->size , NULL);

    if (nready > 0) {
        for (i = 0 ; i < nready ; i++) {
            int mask = ZYZI_NONE;
            struct kevent *ev = &state->events[i];

            zyziFileEvent *fe = &eventLoop->events[i];

            if (ev->filter & EVFILT_READ)  mask |= ZYZI_READABLE;
            if (ev->filter & EVFILT_WRITE) mask |= ZYZI_WRITABLE;

            eventLoop->fired[i].fd   = ev->ident;
            eventLoop->fired[i].mask = mask;
        }
    }

    return nready;
}
