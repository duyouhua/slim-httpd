#ifndef _ZYZI_EV_H_
#define _ZYZI_EV_H_

#define ZYZI_ERR -1
#define ZYZI_OK   0

#define ZYZI_NONE 0
#define ZYZI_READABLE 1
#define ZYZI_WRITABLE 2

struct zyziEventLoop;

typedef void fileProc(struct zyziEventLoop *eventLoop , int fd , int mask , void *clntData);

typedef struct zyziFileEvent {
    int mask;

    fileProc *wproc;
    fileProc *rproc;

    void *clntData;
} zyziFileEvent;

typedef struct zyziFireEvent {
    int fd;
    int mask;
} zyziFireEvent;

typedef struct zyziEventLoop {
    int maxfd;
    int size;
    int stop;

    zyziFireEvent *fired;
    zyziFileEvent *events;

    void *apiData;
} zyziEventLoop;

zyziEventLoop *zyziCreateEventLoop(int size);
void zyziEventStop(zyziEventLoop *eventLoop);
int zyziResizeEventLoop(zyziEventLoop *eventLoop , int nsize);
int zyziEventLoopSize(zyziEventLoop *eventLoop);
void zyziDeleteEventLoop(zyziEventLoop *eventLoop);
int zyziCreateEventFile(zyziEventLoop *eventLoop , int fd , int mask ,
                        fileProc *proc , void *clntData);
void zyziDeleteEventFile(zyziEventLoop *eventLoop , int fd , int mask);
void zyziMain(zyziEventLoop *eventLoop);

#endif // _ZYZI_EV_H_
