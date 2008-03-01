#include "list.h"

#include <assert.h>

void list_add(node_t** top, node_t* node) {
	if(*top)
		node->next = *top;
	*top = node;
}

void list_remove(node_t** top, node_t* node) {
	assert(*top);
	assert(node);

	if(*top == node)  {
		*top = node->next;
	}
	else {
		node_t* p;
		for(p = *top; p && p != node; p = p->next );

		if(p)
			p = node->next;
	}
}
