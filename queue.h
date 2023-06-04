#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <threads.h>
#include <stdatomic.h>

void initQueue(void);
void destroyQueue(void);
void enqueue(void*);
void* dequeue(void);
bool tryDequeue(void**);
size_t size(void);
size_t waiting(void);
size_t visited(void);

typedef struct qNode{
    struct qNode * next;
    void * value;
} qNode;

typedef struct Queue{
    struct qNode * head;
    struct qNode * tail;
    int size;
} Queue;

typedef struct tNode{
    struct tNode * next;
    thrd_t tid;
} tNode;

typedef struct ThreadQueue{
    struct tNode * head;
    struct tNode * tail;
    int size;
} ThreadQueue;
