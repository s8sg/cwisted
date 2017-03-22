#include "queuelib.h"

queue* create_queue() {
	queue *queue = NULL;
	queue = (queue*)malloc(NODE_HEAD_SIZE);
	if (queue == NULL) {
	   twisted_error("Memory allocation failed");
	   return NULL;
	}
	queue->start = NULL;
	queue->end = NULL;
	queue->nos = 0;

	return queue;
}
	
int enqueue(queue *queue, node_head *node) {
	if (queue == NULL) {
	   twisted_error("Invalid Args: Queue is NULL");
	   return -EQUEUE_INVALARG;
	}
	if (node == NULL) {
	   twisted_error("Invalid Argd: Node is NULL");
	   return -EQUEUE_INVALARG;
	}

	if (QUEUE_EMPTY(queue)) {
	   queue->start = node;
	   queue->end = node;
	   queue->nos = 1;
	} else {
	   NODE_NEXT(queue->end) = node;
	   NODE_PREV(node) = queue->end;
	   queue->end = node;
	   queue->nos += 1;
	}
	return 0;
}

int rmqueue(queue *queue, node_head *node) {
	if (queue == NULL) {
	   twisted_error("Invalid Args: Queue is NULL");
	   return -EQUEUE_INVALARG;
	}
	if (node == NULL) {
	   twisted_error("Invalid Args: Node is NULL");
	   return -EQUEUE_INVALARG;
	}
	if (QUEUE_EMPTY(queue)) { 
	   twisted_error("Invalid Args: Empty queue");
	   return -EQUEUE_INVALARG;
	}

	if (NODE_PREV(node) != NULL) {
		NODE_NEXT(NODE_PREV(node)) = NODE_NEXT(node);
		if (NODE_NEXT(node) != NULL) {
		    NODE_PREV(NODE_NEXT(node)) = NODE_PREV(node);
		}
	} else {
		if (NODE_NEXT(node) != NULL) {
		    NODE_PREV(NODE_NEXT(node)) = NULL;
		}
		queue->start = NODE_NEXT(node);
	}

	if (NODE_NEXT(node) != NULL) {
		NODE_PREV(NODE_NEXT(node)) = NODE_PREV(node);
		if (NODE_PREV(node) != NULL) {
		    NODE_NEXT(NODE_PREV(node)) = NODE_NEXT(node);
		}
	} else {
		if (NODE_PREV(node) != NULL) {
		   NODE_PREV(NODE_NEXT(node)) = NULL;
		}
		queue->end = NODE_PREV(node);
	}

	queue->nos -= 1;

	return 0;
}


node_head* dqueue(queue *queue) {

	node_head *node = NULL;

	if (queue == NULL) {
	   twisted_error("Invalid Args: Queue is NULL");
	   return NULL;
	}

	if (QUEUE_EMPTY(queue)) {
	   return NULL;
	}

	node = queue->start;

	if (NODE_NEXT(node) != NULL) {
	   NODE_PREV(NODE_NEXT(node)) = NULL;
	   queue->start = NODE_NEXT(node);
	   queue->nos -= 1;
	} else {
	   queue->start = NULL;
	   queue->end = NULL;
	   queue->nos = 0;
	}
	return node;
}
