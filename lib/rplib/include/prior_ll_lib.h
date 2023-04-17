#ifndef JQR_DSA_PRIOR_LL_LIB_H
#define JQR_DSA_PRIOR_LL_LIB_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

struct prior_ll_node
{
    void *p_data;
    void *p_priority;
    struct prior_ll_node *p_next;
};

struct prior_ll_queue
{
    struct prior_ll_node *p_front;
    struct prior_ll_node *p_rear;
    size_t size;
};

void prior_ll_traverse(struct prior_ll_queue *p_queue, void (*p_print)(void *));
struct prior_ll_node *prior_ll_create_node(void *p_data, void *p_priority);
struct prior_ll_queue *prior_ll_create_queue();
void prior_ll_destroy(struct prior_ll_queue **pp_queue);
int prior_ll_enqueue(struct prior_ll_queue *p_queue, void *p_data, int (*p_compare)(void *, void *), void *p_priority);
void *prior_ll_dequeue(struct prior_ll_queue *p_queue);

#ifdef __cplusplus
}
#endif

#endif // JQR_DSA_PRIOR_LL_LIB_H
