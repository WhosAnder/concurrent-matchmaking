#ifndef COMMON_H
#define COMMON_H

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_PLAYERS 40
#define MAX_ROOMS 8
#define ROOM_CAPACITY 5
#define MAX_WAITING_QUEUE 200

typedef struct {
    int id;
    int play_time_sec;
    int assigned_room;
    int completed;
} Player;

typedef struct {
    int id;
    int capacity;
    int current_players;
    int is_active;
    int total_entries;
    int total_exits;
    sem_t slots;
} Room;

typedef struct {
    int items[MAX_WAITING_QUEUE];
    int front;
    int rear;
    int count;
} WaitingQueue;

static inline void queue_init(WaitingQueue *q) {
    q->front = 0;
    q->rear = -1;
    q->count = 0;
}

static inline int queue_push(WaitingQueue *q, int id) {
    if (q->count >= MAX_WAITING_QUEUE) {
        return -1;
    }
    q->rear = (q->rear + 1) % MAX_WAITING_QUEUE;
    q->items[q->rear] = id;
    q->count++;
    return 0;
}

static inline int queue_pop(WaitingQueue *q, int *id) {
    if (q->count == 0) {
        return -1;
    }
    *id = q->items[q->front];
    q->front = (q->front + 1) % MAX_WAITING_QUEUE;
    q->count--;
    return 0;
}

static inline unsigned int rng_seed(void) {
    return (unsigned int)time(NULL) ^ (unsigned int)getpid() ^ (unsigned int)(uintptr_t)pthread_self();
}

#endif
