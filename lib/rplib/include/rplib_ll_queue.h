/** @file rp_common.h
 *
 * @brief Implements Queue data structure using a Linked List
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#ifndef RPLIB_LL_QUEUE_H
#define RPLIB_LL_QUEUE_H

#include "stdlib.h"

typedef struct rplib_ll_queue_node
{
    void                 *p_data;      // pointer to data
    size_t                data_size;   // size of data in bytes
    struct rplib_ll_queue_node *p_next_node; // next node
} rplib_ll_queue_node_t;

typedef struct rplib_ll_queue
{
    size_t                 size;    // size of queue
    rplib_ll_queue_node_t *p_front; // first node
    rplib_ll_queue_node_t *p_rear;   // last node
} rplib_ll_queue_t;

/**
 * Create a Queue Linked List
 * @return Pointer to created Queue Linked List; NULL on failure
 */
rplib_ll_queue_t *rplib_ll_queue_create(void);

/**
 * Add a new node to the Queue containing data
 * Note: Creates heap allocations for node object and data that must be freed in
 * `rplib_ll_queue_dequeue` or `rplib_ll_queue_destroy`
 * @param p_queue Pointer to target Queue
 * @param p_data Pointer to data to contain within this node
 * @param data_size Non-zero size of data pointed to
 * @return Created node on success; NULL otherwise
 */
rplib_ll_queue_node_t*  rplib_ll_queue_enqueue(rplib_ll_queue_t *p_queue,
                           void             *p_data,
                           size_t            data_size);


/**
 * Remove a specific node from the Queue
 * @param p_queue Pointer to target Queue
 * @param p_target_node Pointer to node to delete
 * @return RPLIB_SUCCESS on removal, RPLIB_UNSUCCESS on failure
 */
int rplib_ll_remove_node(rplib_ll_queue_t *p_queue, rplib_ll_queue_node_t *p_target_node);

/**
 * Removes the first node from the Queue
 * @param p_queue Target Queue
 * @return RPLIB_SUCCESS if no issues; RPLIB_UNSUCCESS otherwise
 */
int rplib_ll_queue_dequeue(rplib_ll_queue_t *p_queue);

/**
 * Get the first node of the Queue WITHOUT removing it
 * @param p_queue Target Queue
 * @return First node from Queue; NULL on failure
 */
rplib_ll_queue_node_t *rplib_ll_queue_peek(rplib_ll_queue_t *p_queue);

/**
 * Destroy a Queue in memory, freeing all children
 * @param p_queue Pointer to LL Queue to destroy
 * @return RPLB
 */
int rplib_ll_queue_destroy(rplib_ll_queue_t *p_queue);

#endif /* RPLIB_LL_QUEUE_H */

/*** end of file ***/
