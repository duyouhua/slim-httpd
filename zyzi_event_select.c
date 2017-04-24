typedef struct zyziApiState {
    fd_set  rset ,  wset ;
    fd_set _rset , _wset ;
} zyziApiState;

static int zyziApiCreate(zyziEventLoop *eventLoop)
{
    zyziApiState *state = zyzi_malloc(sizeof(zyziApiState));

    if (state) {
        FD_ZERO(&state->rset);
        FD_ZERO(&state->wset);

        eventLoop->apiData = state;

        return ZYZI_OK;
    } else {
        zyzi_free(state);

        return ZYZI_ERR;
    }
}

static int zyziApiResize(zyziEventLoop *eventLoop , int nsize)
{
    (void) eventLoop;
    (void) nsize;

    return ZYZI_OK;
}

static void zyziApiFree(zyziEventLoop *eventLoop)
{
    zyzi_free(eventLoop->apiData);
}

static int zyziApiAddEvent(zyziEventLoop *eventLoop , int fd , int mask)
{
    zyziApiState *state = eventLoop->apiData;

    if (mask & ZYZI_READABLE) FD_SET(fd , &state->rset);
    if (mask & ZYZI_WRITABLE) FD_SET(fd , &state->wset);

    return ZYZI_OK;
}

static void zyziApiDelEvent(zyziEventLoop *eventLoop , int fd , int mask)
{
    zyziApiState *state = eventLoop->apiData;

    if (mask & ZYZI_READABLE) FD_CLR(fd , &state->rset);
    if (mask & ZYZI_WRITABLE) FD_CLR(fd , &state->wset);
}

static int zyziApiPoll(zyziEventLoop *eventLoop ,struct timeval *tval)
{
    int nready = 0, val , i;
    zyziApiState *state = eventLoop->apiData;

    memcpy(&state->_rset , &state->rset , sizeof(state->rset));
    memcpy(&state->_wset , &state->wset , sizeof(state->wset));

    val = select(eventLoop->maxfd + 1 , &state->_rset , &state->_wset , NULL , tval);

    if (val > 0) {
        for (i = 0 ; i <= eventLoop->maxfd ; i++) {
            int mask = ZYZI_NONE;
            zyziFileEvent *fe = &eventLoop->events[i];

            if (fe->mask == ZYZI_NONE) continue;

            if (fe->mask & ZYZI_READABLE && FD_ISSET(i , &state->_rset)) mask |= ZYZI_READABLE;
            if (fe->mask & ZYZI_WRITABLE && FD_ISSET(i , &state->_wset)) mask |= ZYZI_WRITABLE;

            if (mask != ZYZI_NONE) {
                eventLoop->fired[nready].fd   = i;
                eventLoop->fired[nready].mask = mask;
                nready++;
            }
        }
    }

    return nready;
}

