#define NODE_NEXT(node) (node->next)
#define NODE_PREV(node) (node->prev)
#define NODE_DATA(node) (node->data)
#define NOT_LAST(ptr) ptr->next != NULL

struct node_head {
    struct node_head *data;
    struct node_head *next;
    struct node_head *prev;
};

typedef struct node_head node_head;

#define NODE_HEAD_SIZE sizeof(node_head)

node_head* create_node(void*);
int delete_node(node_head*, void**);
int add_node(node_head *c_node, node_head *n_node, int position);
