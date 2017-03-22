#include "listlib.h"

struct queue {
	node_head* start;  /* Start of the ready Queue */
	node_head* end;    /* End Of the Ready Queue */
	int nos;           /* No Of Element in the ready Queue */
};

typedef struct queue queue;

#define QUEUE_SIZE(queue) queue->nos
#define QUEUE_EMPTY(queue) (queue->nos == 0)
#define EQUEUE_INVALARG -1
#define QUEUE_PEEK(queue) queue->start

queue* create_queue();
int enqueue(queue*, node_head*);
int rmqueue(queue*, node_head*);
node_head* dqueue(queue*);
