#include "lua_gc_node.h"
#include <stdio.h>

int main() {
	int i = 0;
	while (1) {
		struct lua_gc_node* node = lua_gc_node_new(5, "table", &main);
	//	printf("%d\n", i++);
		struct lua_gc_node* node1 = lua_gc_node_new(5, "table", &main);
		struct lua_gc_node* node2 = lua_gc_node_new(6, "userdata", &main);
		lua_gc_node_set_desc(node, "Hello");
		lua_gc_node_set_link(node, "_G");
		lua_gc_node_set_desc(node1, "123");
		lua_gc_node_set_link(node1, "A");
		lua_gc_node_add_child(node, node1);
		lua_gc_node_set_desc(node2, "Good");
		lua_gc_node_set_link(node2, "AA");
		lua_gc_node_add_child(node1, node2);
		printf("%s\n", lua_gc_node_to_str(node));
		lua_gc_node_free(node);
	}
}
