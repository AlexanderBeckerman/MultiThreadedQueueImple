#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <threads.h>
#include <stdatomic.h>

void freeQueue(void); // Free the queue memory
void *removeItemFromQueue(int index); // Removes an item from the queue at a specific index.
// Useful for when we have enough items at the queue, but we want to save the {index} amount of items for sleeping threads

typedef struct qNode { // Node for main queue
    struct qNode *next;
    void *value;
} qNode;

typedef struct Queue { // Struct representing our main queue
    struct qNode *head;
    struct qNode *tail;
    atomic_int size;
    int empty; // A flag to keep track when to wake up a new thread
} Queue;

typedef struct tNode { // Node for the thread queue
    struct tNode *next;
    thrd_t tid;
    cnd_t waitItem; // Condition variable for the thread. Uses it when it arrives at an empty queue
} tNode;

typedef struct ThreadQueue { // Struct representing the thread queue
    struct tNode *head;
    struct tNode *tail;
    atomic_int size;
} ThreadQueue;


static Queue *queue; // Our main queue
static ThreadQueue *threadQueue; // Secondary queue to track the sleeping threads
static mtx_t qlock; // Lock for the main queue
static mtx_t tlock; // Lock for the thread queue
static atomic_int times_visited; // Counter to keep track for the visited() function. It is updated after every completed dequeue


void initQueue(void) {
    queue = (Queue *) malloc(sizeof(Queue));
    threadQueue = (ThreadQueue *) malloc(sizeof(ThreadQueue));
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    queue->empty = 1;
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
    freeQueue(); // Frees both queues and destroys the condition variables inside the queue

}

void enqueue(void *item) {
    mtx_lock(&qlock);
    queue->size++; // First we insert the new item
    qNode *newNode = (qNode *) malloc(sizeof(qNode));
    newNode->value = item;
    newNode->next = NULL;
    if (queue->head == NULL) {
        queue->head = newNode;
        queue->tail = newNode;

    } else {
        queue->tail->next = newNode;
        queue->tail = newNode;
    }
    if (threadQueue->size > 0 &&
        queue->empty == 1) { // If there are waiting threads, wake up the first in line to process the new item.
        mtx_lock(&tlock);
        queue->empty = 0;
        cnd_signal(&(threadQueue->head->waitItem)); // Wake up the thread that went to sleep first
        mtx_unlock(&tlock);
    }
    mtx_unlock(&qlock);
}

void *dequeue(void) {
    mtx_lock(&qlock);
    void *item;
    if (queue->head == NULL ||
        queue->size <= threadQueue->size) { // If the queue is empty we need to send the thread to sleep
        mtx_lock(&tlock);
        tNode *newNode = (tNode *) malloc(sizeof(tNode)); // Add the current thread to the waiting threads queue
        newNode->tid = thrd_current();
        newNode->next = NULL;
        cnd_init(&(newNode->waitItem));
        threadQueue->size++;
        if (threadQueue->head == NULL) {
            threadQueue->head = newNode;
            threadQueue->tail = newNode;
        } else {
            threadQueue->tail->next = newNode;
            threadQueue->tail = newNode;
        }
        mtx_unlock(&tlock);
        cnd_wait(&(newNode->waitItem),
                 &qlock); // Wait for a new item to be added

        // Remove the item from the main queue with the current thread (the thread woke up, so it means an item is available)
        qNode *qfirst = queue->head;
        item = qfirst->value;
        queue->head = qfirst->next;
        if (queue->head == NULL) {
            queue->tail = NULL;
        }
        queue->size--;
        if (queue->size == 0) {
            queue->empty = 1; // If the queue is empty, we want to wake up a new thread next enqueue
        }
        free(qfirst);

        mtx_lock(&tlock);
        tNode *first = threadQueue->head; // Remove the current thread from the thread queue after we removed an item
        threadQueue->head = first->next;
        if (threadQueue->head == NULL) {
            threadQueue->tail = NULL;
        }
        threadQueue->size--;
        free(first);

        if (queue->size > 0 && threadQueue->size > 0) { // Wake up the next thread in line to handle the available item
            cnd_signal(&(threadQueue->head->waitItem));
        }
        times_visited++;
        mtx_unlock(&tlock);
        mtx_unlock(&qlock);
        return item;
    }
    // Else queue is not empty, so we remove the item that doesn't belong to any other (sleeping or not) thread
    int index = threadQueue->size;
    item = removeItemFromQueue(index);
    if (queue->head == NULL) {
        queue->empty = 1;
        queue->tail = NULL;
    }
    times_visited++;
    mtx_unlock(&qlock);
    return item;
}

bool tryDequeue(void **arg) {
    mtx_lock(&qlock);
    if (queue->size == 0 || threadQueue->size >= queue->size) { // Queue is empty
        mtx_unlock(&qlock);
        return false; // Don't go to sleep and wait for an item, immediately return.
    }
    int index = threadQueue->size;
    *arg = removeItemFromQueue(index); // Get the returned item inside arg
    if (queue->head == NULL) {
        queue->empty = 1;
        queue->tail = NULL;
    }
    times_visited++;

    mtx_unlock(&qlock);

    return true;
}

size_t size(void) {
    return queue->size;
}

size_t waiting(void) {
    mtx_lock(&qlock);
    size_t waiting = threadQueue->size; // Our threadQueue size is the number of waiting threads
    mtx_unlock(&qlock);
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

void *removeItemFromQueue(int index) { // Removes and returns an item at the given index from the queue
    qNode *current = queue->head;
    qNode *previous = NULL;
    int i = 0;
    void *ret = NULL;
    while (current != NULL) {
        if (i == index) {
            if (previous == NULL) {
                queue->head = current->next;
            } else {
                previous->next = current->next;
            }
            if (current == queue->tail) {
                queue->tail = previous;
            }
            ret = current->value;
            free(current);
            break;
        }
        previous = current;
        current = current->next;
        i++;
    }
    queue->size--;
    return ret;

}
