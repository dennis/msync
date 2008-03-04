/*
The MIT License

Copyright (c) 2008 Dennis Møllegaard Pedersen <dennis@moellegaard.dk>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
#include "list.h"

#include <assert.h>
#include <stdlib.h>

/*
static void list_add_pre(node_t** top, node_t* node) {
	node->next = *top;
	*top = node;
}
*/

static void list_add_post(node_t** top, node_t* node) {
	if(*top) {
		node_t* end;
		for(end = *top; end && end->next; end = end->next);
		end->next = node;
		node->next = NULL;
	}
	else {
		*top = node;
		node->next = NULL;
	}
}

void list_add(node_t** top, node_t* node) {
	list_add_post(top, node);
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
