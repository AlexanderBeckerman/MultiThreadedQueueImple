#include "queue.h"

static Queue *queue;
static ThreadQueue *threadQueue;
static mtx_t qlock;
static mtx_t tlock;
static size_t times_visited;

void initQueue(void) {
    queue = (Queue *) malloc(sizeof(Queue));
    threadQueue = (ThreadQueue *) malloc(sizeof(ThreadQueue));
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    threadQueue->head = NULL;
    threadQueue->tail = NULL;
    threadQueue->size = 0;
    mtx_init(&qlock, mtx_plain);
    mtx_init(&tlock, mtx_plain);
    times_visited = 0;
}

void destroyQueue(void) {

    mtx_destroy(&qlock);
    mtx_destroy(&tlock);
    freeQueue();

}

void enqueue(void *item) {
    mtx_lock(&qlock);
    queue->size = queue->size + 1;
    qNode *newNode = (qNode *) malloc(sizeof(qNode));
    newNode->value = item;
    newNode->next = NULL;
    if (queue->head == NULL) {
        queue->head = newNode;
        queue->tail = newNode;
        if (threadQueue->size > 0) { // If there are waiting threads, wake up the first in line to process the new item.
            cnd_signal(&(threadQueue->head->waitItem));
        }
    } else {
        queue->tail->next = newNode;
        queue->tail = newNode;
    }

    mtx_unlock(&qlock);
}

void *dequeue(void) {
    mtx_lock(&qlock);
    void *item;
    if (queue->head == NULL) {
        mtx_lock(&tlock);
        tNode *newNode = (tNode *) malloc(sizeof(tNode)); // Add the current thread to the waiting threads queue
        newNode->tid = thrd_current();
        newNode->next = NULL;
        cnd_init(&(newNode->waitItem));
        threadQueue->size = threadQueue->size + 1;
        if (threadQueue->head == NULL) {
            threadQueue->head = newNode;
            threadQueue->tail = newNode;
        } else {
            threadQueue->tail->next = newNode;
            threadQueue->tail = newNode;
        }
        mtx_unlock(&tlock);
        cnd_wait(&(newNode->waitItem),
                 &qlock); // Wait for a new item to be added (every enqueue we signal the top of the thread queue)
        mtx_lock(&tlock);
        tNode *first = threadQueue->head; // Remove the current thread from the thread queue. If we woke up it means an item is ready
        threadQueue->head = first->next;
        if (threadQueue->head = NULL) {
            threadQueue->tail = NULL;
        }
        threadQueue->size = threadQueue->size - 1;
        free(first);
        mtx_unlock(&tlock);

        // Remove the item from the main queue with the current thread
        qNode *qfirst = queue->head;
        item = qfirst->value;
        queue->head = qfirst->next;
        if (queue->head = NULL) {
            queue->tail = NULL;
        }
        queue->size = queue->size - 1;
        free(qfirst);
        times_visited++;
        mtx_unlock(&qlock);
        return item;
    }
    // Else queue is not empty we can just remove the item
    qNode *qfirst = queue->head;
    item = qfirst->value;
    queue->head = qfirst->next;
    if (queue->head = NULL) {
        queue->tail = NULL;
    }
    queue->size = queue->size - 1;
    free(qfirst);
    times_visited++;
    mtx_unlock(&qlock);
    return item;
}

bool tryDequeue(void **arg) {
    mtx_lock(&qlock);
    if (queue->size == 0 || threadQueue->size > queue->size) {
        mtx_unlock(&qlock);
        return false;
    }
    qNode *first = queue->head;
    arg = first->value;
    queue->head = queue->head->next;
    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    queue->size = queue->size - 1;
    free(first);
    times_visited++;
    mtx_unlock(&qlock);
    return true;
}

size_t size(void) {
    return queue->size;
}

size_t waiting(void) {
    mtx_lock(&tlock);
    size_t waiting = threadQueue->size;
    mtx_unlock(&tlock);
    return waiting;
}

size_t visited(void) {
    return times_visited;
}

void freeQueue(void) {
    qNode *current = queue->head;
    while (current != NULL) {
        qNode *temp = current;
        current = current->next;
        free(temp);
    }
    free(queue);

    tNode *current2 = threadQueue->head;
    while (current2 != NULL) {
        tNode *temp2 = current2;
        cnd_destroy(&(current2->waitItem));
        current2 = current2->next;
        free(temp2);
    }
    free(threadQueue);
}