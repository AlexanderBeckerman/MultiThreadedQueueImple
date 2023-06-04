#include "queue.h"

static Queue * queue;
static ThreadQueue * threadQueue;
static mtx_t qlock;
static mtx_t threadlock;
static cnd_t notEmpty;
static size_t times_visited = 0;

void initQueue(void) {
    queue = (Queue *)malloc(sizeof(Queue));
    threadQueue = (ThreadQueue *)malloc(sizeof(ThreadQueue));
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    threadQueue->head = NULL;
    threadQueue->tail = NULL;
    threadQueue->size = 0;

}

void destroyQueue(void) {

}

void enqueue(void *) {

}

void dequeue(void) {

}

bool tryDequeue(void **) {

}

size_t size(void) {

}

size_t waiting(void) {

}

size_t visited(void) {

}
