/** @file rplib_linkedlist.c
 *
 * @brief Implements Queue data structure using a Linked List
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#include "rplib_ll_queue.h"

#include <assert.h>
#include <limits.h>
#include <string.h>

#include "rplib_common.h"

rplib_ll_queue_t *
rplib_ll_queue_create(void)
{
    rplib_ll_queue_t *p_res = NULL;
    // allocate
    p_res = malloc(sizeof(rplib_ll_queue_t));
    if (NULL == p_res)
    {
        RPLIB_DEBUG_PRINTF("error: rplib_ll_queue, %s", "LIST MALLOC");
        goto leave;
    }
    // set fields
    p_res->size    = 0;
    p_res->p_front = NULL;
    p_res->p_rear  = NULL;
leave:
    return p_res;
}

rplib_ll_queue_node_t *
rplib_ll_queue_enqueue(rplib_ll_queue_t *p_queue,
                       void             *p_data,
                       size_t            data_size)
{
    rplib_ll_queue_node_t *p_node = NULL; // node object
    void                  *p_buf  = NULL; // to contain data
    // asserts
    assert(data_size > 0);
    assert(NULL != p_data);
    assert(NULL != p_queue);
    // allocations
    p_node = malloc(sizeof(rplib_ll_queue_node_t));
    p_buf  = malloc(data_size);
    if (NULL == p_node || NULL == p_buf)
    {
        RPLIB_DEBUG_PRINTF("error: rplib_ll_queue, %s", "NODE MALLOC");
        goto cleanup;
    }
    // copy data
    if (!memcpy(p_buf, p_data, data_size))
    {
        goto cleanup;
    }
    // set fields on node
    p_node->p_data    = p_buf;
    p_node->data_size = data_size;
    // update list
    // safely change size
    if (p_queue->size + 1 < INT_MAX)
    {
        p_queue->size = p_queue->size + 1;
    }
    // if size too big, abort
    else
    {
        goto cleanup;
    }
    // if no existing nodes, add orphan
    if (!p_queue->p_front)
    {
        p_queue->p_front = p_node;
        p_queue->p_rear  = p_node;
    }
    // if existing nodes, append to end
    else
    {
        p_queue->p_rear->p_next_node = p_node;
        p_queue->p_rear              = p_node;
    }
    goto leave;
cleanup:
    free(p_node);
    free(p_buf);
    p_node = NULL;
    return NULL;
leave:
    return p_node;
}

int
rplib_ll_remove_node(rplib_ll_queue_t      *p_queue,
                     rplib_ll_queue_node_t *p_tgt_node)
{
    int                    res         = RPLIB_UNSUCCESS;
    rplib_ll_queue_node_t *p_prev_node = NULL;

    // only node
    if (1 == p_queue->size)
    {
        p_queue->p_front = NULL;
        p_queue->p_rear  = NULL;
        goto update;
    }
    // not only node, but is front node
    else if (p_tgt_node == p_queue->p_front)
    {
        p_queue->p_front = p_tgt_node->p_next_node;
        goto update;
    }
    // >1 node, not front node, so must be in between or at end
    else
    {
        // get previous node
        p_prev_node = p_queue->p_front;
        while (p_prev_node->p_next_node != p_tgt_node)
        {
            p_prev_node = p_prev_node->p_next_node;
        }
        // update previous node's next node pointer
        p_prev_node->p_next_node = p_tgt_node->p_next_node;
        // if at end, set new rear
        if (p_tgt_node == p_queue->p_rear)
        {
            p_queue->p_rear = p_prev_node;
        }
    }

update:
    // update size
    p_queue->size--;
    // free data
    free(p_tgt_node->p_data);
    p_tgt_node->p_data = NULL;
    // free node
    free(p_tgt_node);
    p_tgt_node = NULL;

    res = RPLIB_SUCCESS;
    return res;
}

int
rplib_ll_queue_dequeue(rplib_ll_queue_t *p_queue)
{
    int                    res = RPLIB_UNSUCCESS;
    rplib_ll_queue_node_t *p_tgt_node;
    // asserts
    assert(p_queue->p_front);         // must have at least one node
    assert(p_queue->size > 0);
    assert(p_queue->p_front->p_data); // node must have data
    // get target node
    p_tgt_node = p_queue->p_front;
    res        = rplib_ll_remove_node(p_queue, p_tgt_node);
    return res;
}

rplib_ll_queue_node_t *
rplib_ll_queue_peek(rplib_ll_queue_t *p_queue)
{
    void *p_res = NULL;
    // asserts
    assert(p_queue->p_front);
    // get first node
    p_res = p_queue->p_front;
    return p_res;
}

int
rplib_ll_queue_destroy(rplib_ll_queue_t *p_queue)
{
    int    res             = RPLIB_UNSUCCESS;
    size_t free_node_index = 0;
    assert(p_queue);
    // dequeue all children
    for (free_node_index = 0; free_node_index < p_queue->size;
         free_node_index++)
    {
        assert(p_queue->p_front);
        rplib_ll_queue_dequeue(p_queue);
    }
    // destroy parent
    free(p_queue);
    p_queue = NULL;
    // leave
    return res;
}
