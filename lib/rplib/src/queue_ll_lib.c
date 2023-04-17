#include "queue_ll_lib.h"

#include <assert.h>
#include <string.h>

#include "rp_common.h"

typedef struct queue_ll_node  node_t;
typedef struct queue_ll_queue queue_t;

static int queue_ll_is_empty(queue_t *p_queue);

/**
 * Check if queue linked list is empty
 * @param p_queue pointer to queue linked list to evaluate
 * @return RPLIB_UNSUCCESS if passed queue is empty, 0 if it isn't, -1 on error
 */
static int
queue_ll_is_empty(queue_t *p_queue)
{
    // validate param
    assert(p_queue);
    // if size is 0, empty
    if (0 == p_queue->size)
    {
        return RPLIB_UNSUCCESS;
    }
    return RPLIB_SUCCESS;
}

/**
 * Get the data of the node at a given index
 * @param p_queue Queue Linked List to get index from
 * @param index
 * @return
 */
void *
queue_ll_get_index(queue_t *p_queue, size_t target_index)
{
    size_t                i              = 0;
    void                 *res            = NULL;
    struct queue_ll_node *p_current_node = NULL;
    if (target_index >= p_queue->size || queue_ll_is_empty(p_queue))
    {
        return NULL;
    }
    // starting point
    p_current_node = p_queue->p_front;
    for (i = 0; i < p_queue->size; i++)
    {
        if (i == target_index)
        {
            if(p_current_node)
            {

            }
            res = p_current_node->p_data;
            break;
        }
        p_current_node = p_current_node->p_next;
    }

    return res;
}

/**
 * Traverse queue linked list and print values
 * @param p_queue pointer to queue linked list to evaluate
 * @param p_print pointer to function to use to print each entry
 */
void
queue_ll_traversal_print(queue_t *p_queue, void (*p_print)(void *))
{
    node_t *next_node;
    // validate param
    if (!p_queue)
    {
#ifdef DEBUG
        fprintf(stderr, "Error: Cannot traverse invalid queue linked list.\n");
#endif
    }
    // iterate over and pass to printer
    next_node = p_queue->p_front;
    while (next_node->p_next != NULL)
    {
        p_print(next_node->p_data);
        next_node = next_node->p_next;
    }
}

/**
 * Helper function for queue_ll_traversal_reverse_print to recurse through a
 * given queue linked list.
 * @param node pointer to first node to evaluate
 * @param p_print function pointer to a function to evaluate each entry
 */
void
queue_ll_traversal_recurse(node_t *node, void (*p_print)(void *))
{
    // base
    if (!node)
    {
        return;
    }
    // ascending
    queue_ll_traversal_recurse(node->p_next, p_print);
    // descending
    p_print(node->p_data);
}

/**
 * Traverse queue linked list and print values in reverse
 * @param p_queue pointer to queue linked list to evaluate
 * @param p_print pointer to function to use to print each entry
 */
void
queue_ll_traversal_reverse_print(queue_t *p_queue, void (*p_print)(void *))
{
    node_t *next_node;
    // validate param
    if (!p_queue)
    {
#ifdef DEBUG
        fprintf(stderr, "Error: Cannot traverse invalid queue linked list.\n");
#endif
    }
    // recurse
    queue_ll_traversal_recurse(p_queue->p_front, p_print);
}

/**
 * Create a new node in the queue linked list\n
 * Allocates memory for new node on heap\n
 * Warning: Allocated node must be freed with queue_ll_destroy,
 * queue_ll_dequeue, or manually
 * @param p_data pointer to data to place in linked list
 * @return pointer to new node on the heap, NULL on failure
 */
node_t *
queue_ll_create_node(void *p_data)
{
    node_t *node_pointer = NULL;
    if (!p_data)
    {
#ifdef DEBUG
        fprintf(stderr,
                "Error: Cannot insert invalid value into queue linked list.\n");
#endif
        return NULL;
    }
    // allocate
    errno        = 0;
    node_pointer = malloc(sizeof(struct queue_ll_node));
    if (!node_pointer || errno)
    {
#ifdef DEBUG
        perror("Linked List Node Allocation Error");
#endif
        return NULL;
    }
    // allocation successful, set data
    node_pointer->p_data = p_data;
    node_pointer->p_next = NULL;
    // return
    return node_pointer;
}

/**
 * Create queue linked list data structure\n
 * Allocates memory for new queue linked list on heap\n
 * Warning: Allocated queue must be freed with queue_ll_destroy or manually
 * @return pointer to new queue on the heap, NULL on failure
 */
queue_t *
queue_ll_create_queue(void)
{
    queue_t *struct_pointer = NULL;
    // allocate
    errno          = 0;
    struct_pointer = malloc(sizeof(struct queue_ll_queue));
    if (!struct_pointer || errno)
    {
#ifdef DEBUG
        perror("Linked List Struct Allocation Error");
#endif
        return NULL;
    }
    // allocation successful, init values
    // top and next are NULL since no entries yet
    struct_pointer->p_front = NULL;
    struct_pointer->p_rear  = NULL;
    struct_pointer->size    = 0;

    // return pointer
    return struct_pointer;
}

/**
 * Destroy a given queue linked list
 * @param pp_queue pointer to queue linked list to destroy
 */
void
queue_ll_destroy(queue_t **pp_queue)
{
    node_t *delete_node;
    node_t *next_node;
    // validate param
    if (!pp_queue || !*pp_queue)
    {
#ifdef DEBUG
        fprintf(stderr, "Error: Cannot delete invalid queue linked list.\n");
#endif
        return;
    }
    // if not empty, iterate over queue linked list and free nodes
    next_node = ((queue_t *)*pp_queue)->p_front;
    while (next_node != NULL)
    {
        delete_node = next_node;
        next_node   = next_node->p_next;
        free(delete_node);
    }
    // free control struct
    free(*pp_queue);
    *pp_queue = NULL;
}

/**
 * Remove item at specified index from the queue
 * @param p_queue Object to operate on
 * @param target_index Index to remove at
 * @param p_callback Pointer to callback destructor to run on p_data
 * @return RPLIB_SUCCESS on success, 1 on failure
 */
int
queue_ll_remove(queue_t *p_queue,
                size_t   target_index,
                void (*p_callback)(void *))
{
    size_t                i               = 0;
    int res=1;
    struct queue_ll_node *p_previous_node = NULL;
    struct queue_ll_node *p_current_node  = NULL;
    if (target_index >= p_queue->size || queue_ll_is_empty(p_queue))
    {
        return RPLIB_UNSUCCESS;
    }
    // starting point
    p_previous_node = p_queue->p_front;
    p_current_node  = p_queue->p_front;
    for (i = 0; i < p_queue->size; i++)
    {
        // keep searching
        if (i != target_index)
        {
            p_previous_node = p_current_node;
            p_current_node  = p_current_node->p_next;
            continue;
        }
        /*
         * Found
         * If in front, update front
         */
        if (p_current_node == p_queue->p_front)
        {
            p_previous_node=NULL;
            p_queue->p_front = p_current_node->p_next;
        }
        if (p_current_node == p_queue->p_rear)
        {
            p_queue->p_rear = p_previous_node;
            if(p_previous_node)
            {
                p_previous_node->p_next=NULL;
            }
        }
        // if between 2 nodes, update links
        if (p_previous_node != NULL && p_current_node->p_next != NULL)
        {
            p_previous_node->p_next = p_current_node->p_next;
        }
        // run callback on data
        if (p_callback)
            p_callback(p_current_node->p_data);

        // decrement
        p_queue->size--;

        // free allocation
        free(p_current_node);
        p_current_node = NULL;
        res=0;
        break;
    }
    return res;
}

/**
 * enqueue a new value to the queue linked list
 * @param p_queue pointer to queue linked list to append value to
 * @param p_data pointer to value to append
 * @return RPLIB_SUCCESS on success, -1 on failure
 */
int
queue_ll_enqueue(queue_t *p_queue, void *p_data)
{
    node_t *new_node = NULL;
    // validate param
    if (!p_queue)
    {
#ifdef DEBUG
        fprintf(stderr,
                "Error: Cannot enqueue value to invalid queue linked list.\n");
#endif
        return -1;
    }
    // validate data
    if (!p_data)
    {
#ifdef DEBUG
        fprintf(stderr,
                "Error: Cannot enqueue invalid value to queue linked list.\n");
#endif
        return -1;
    }
    // if p_rear p_next is null need to expand list
    if (!p_queue->p_rear || !p_queue->p_rear->p_next)
    {
        // add a node to contain value
        new_node = queue_ll_create_node(p_data);
    }
    else
    {
        new_node = p_queue->p_rear;
    }
    if (!new_node)
    {
#ifdef DEBUG
        fprintf(stderr,
                "Error: Could not allocate new node for enqueue operation. See "
                "output for details.\n");
#endif
        return -1;
    }
    // add node to linked list
    // if empty queue linked list, just set front
    if (queue_ll_is_empty(p_queue))
    {
        p_queue->p_front = new_node;
        p_queue->p_rear  = new_node;
    }
    else
    {
        // if not empty, just get rear and update p_next
        p_queue->p_rear->p_next = new_node;
        // new node is new rear (since queue)
        p_queue->p_rear = new_node;
    }
    // update size
    p_queue->size++;
    return RPLIB_SUCCESS;
}

/**
 * Retrieve top entry of the queue linked list while removing it from the
 * queue\n
 * @param p_queue pointer to queue linked list to evaluate
 * @return pointer to value that was on top of the queue data structure; NULL if
 * operation unsuccessful
 */
void *
queue_ll_dequeue(queue_t *p_queue)
{
    void   *value_pointer;
    node_t *old_node;
    // validate param
    if (!p_queue || !p_queue->p_rear)
    {
        fprintf(
            stderr,
            "Error: Cannot dequeue value from invalid queue linked list.\n");
        return NULL;
    }
    if (queue_ll_is_empty(p_queue))
    {
#ifdef DEBUG
        fprintf(stderr,
                "Error: Cannot dequeue value from empty queue linked list.\n");
#endif
        return NULL;
    }
    // store front value
    old_node      = p_queue->p_front;
    value_pointer = old_node->p_data;

    // if 1+ items, front becomes next element
    // if 1 item, front is NULL
    if (p_queue->size > 1)
    {
        p_queue->p_front = old_node->p_next;
    }
    else
    {
        // if size is 1, set everything to null
        p_queue->p_front = NULL;
        p_queue->p_rear  = NULL;
    }
    p_queue->size--;
    // free old node
    free(old_node);
    old_node = NULL;
    // return retrieved value pointer
    return value_pointer;
}
