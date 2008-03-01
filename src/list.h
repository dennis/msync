#ifndef __list_h__
#define __list_h__

typedef struct node_s {
	struct node_s* next;
} node_t;
 
void list_add(node_t**, node_t*);
void list_remove(node_t**, node_t*);

#endif
