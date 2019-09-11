#include <malloc.h>
#include "queue.h"

struct _Queue {
    int size;
    void **tab;
    int next_to_write;
    int next_to_read;
    int *ready;
} ;


/**
 * 初始化队列
 */
Queue *queue_init(int size, queue_fill_func fill_func) {
    Queue *queue = (Queue *) malloc(sizeof(Queue));
    queue->size = size;
    queue->next_to_read = 0;
    queue->next_to_write = 0;

    queue->tab = malloc(sizeof(*queue->tab) * size);
    int i;
    for (i = 0; i < size; i++) {
        queue->tab[i] = fill_func();
    }
    return queue;
}

/**
 * 销毁队列
 */
void queue_free(Queue *queue, queue_free_func free_func) {
    int i;
    for (i = 0; i < queue->size; i++) {
        free_func((void *) queue->tab[i]);
    }
    free(queue->tab);
    free(queue);
}

/**
 * 获取下一个索引位置
 */
int queue_get_next(Queue *queue, int current) {
    // 双向循环链表
    return (current + 1) % queue->size;
}

/**
 * 队列压入元素
 */
void *queue_push(Queue *queue, pthread_mutex_t *mutex, pthread_cond_t *cond) {
    int current = queue->next_to_write;
    int next_to_write;

    for (;;) {
        next_to_write = queue_get_next(queue, current);
        if (next_to_write != queue->next_to_read) {
            break;
        }
        pthread_cond_wait(cond, mutex);
    }
    LOGI("queue_push queue:%#x, %d",queue,current);
    queue->next_to_write = next_to_write;
    pthread_cond_broadcast(cond);
    return queue->tab[current];
}

/**
 * 弹出元素
 */
void *queue_pop(Queue *queue, pthread_mutex_t *mutex, pthread_cond_t *cond) {
    int current = queue->next_to_read;
    for (;;) {
        if (queue->next_to_read != queue->next_to_write) {
            break;
        }
        pthread_cond_wait(cond, mutex);
    }
    LOGI("queue_pop queue:%#x, %d",queue,current);
    queue->next_to_read = queue_get_next(queue, current);
    pthread_cond_broadcast(cond);
    return queue->tab[current];

}
