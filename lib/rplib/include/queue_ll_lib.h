#ifndef JQR_DSA_QUEUE_LL_LIB_H
#define JQR_DSA_QUEUE_LL_LIB_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

    struct queue_ll_node
    {
        void                 *p_data;
        struct queue_ll_node *p_next;
    };

    struct queue_ll_queue
    {
        struct queue_ll_node *p_front;
        struct queue_ll_node *p_rear;
        size_t                size;
    };

    void queue_ll_traverse_print(struct queue_ll_queue *p_queue,
                                 void (*p_print)(void *));
    struct queue_ll_node  *queue_ll_create_node(void *p_data);
    struct queue_ll_queue *queue_ll_create_queue();
    void                  *queue_ll_get_index(struct queue_ll_queue *p_queue,
                                              size_t                 target_index);
    void                   queue_ll_destroy(struct queue_ll_queue **pp_queue);
    int   queue_ll_enqueue(struct queue_ll_queue *p_queue, void *p_data);
    void *queue_ll_dequeue(struct queue_ll_queue *p_queue);
    int   queue_ll_remove(struct queue_ll_queue *p_queue,
                          size_t                 target_index,
                          void (*p_callback)(void *));

#ifdef __cplusplus
}
#endif

#endif // JQR_DSA_QUEUE_LL_LIB_H
