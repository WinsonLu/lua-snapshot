#ifdef __cplusplus
extern "C" {
#endif
#include "lua_gc_node.h"
#include "cJSON.h"
#define DEFAULT_ALLOC_SIZE (sizeof(struct lua_gc_node)*32)


static struct lua_gc_node* default_alloc();
static void default_free(struct lua_gc_node*);

static struct lua_gc_node* free_list = NULL;
struct lua_gc_node_mem_fn {
	lua_gc_node_alloc_fn alloc;
	lua_gc_node_free_fn free;
};
static struct lua_gc_node_mem_fn  mem_func = {
	default_alloc,
	default_free
};

// 默认内存分配函数
static struct lua_gc_node* default_alloc() {
	if (free_list == NULL) {
		struct lua_gc_node* first = (struct lua_gc_node*)malloc(DEFAULT_ALLOC_SIZE);
		struct lua_gc_node* last = (struct lua_gc_node*)((char*)first + DEFAULT_ALLOC_SIZE);
		free_list = first;
		while (first != last - 1) {
			first->next_sibling = first + 1;
			first = first + 1;
		}
		first->next_sibling = NULL;
	}
	struct lua_gc_node* ret = free_list;
	free_list = free_list->next_sibling;
	return ret;
}

// 默认内存释放函数
static void default_free(struct lua_gc_node* node) {
	if (node == NULL) return;
	node->next_sibling = free_list;
	free_list = node;
}

// 分配新节点
struct lua_gc_node* lua_gc_node_new(int type, const char* typestr, const void* pointer) {
	struct lua_gc_node* ret = (struct lua_gc_node*)mem_func.alloc();
	memset(ret, 0, sizeof(*ret));
	ret->type = type;
	snprintf(ret->name, LUA_GC_NODE_NAME_SIZE, "%s:%p", typestr, pointer);
	ret->lua_obj_ptr = pointer;
	return ret;
}

// 释放节点内存，同时释放自己点的内存
void lua_gc_node_free(struct lua_gc_node* node) {
	if (node == NULL)
		return;
	struct lua_gc_node* child = node->first_child;
	struct lua_gc_node* next_child = NULL;
	while (child != NULL) {
		next_child = child->next_sibling;
		lua_gc_node_free(child);
		child = next_child;
	}
	mem_func.free(node);
}

// 释放所有空闲节点的内存
void lua_gc_node_free_all() {
	struct lua_gc_node* node = free_list;
	struct lua_gc_node* next = NULL;
	while (node != NULL) {
		next = node->next_sibling;
		free(node);
		node = next;
	}
	free_list = NULL;
}

// 统计所有节点的数量（包括空闲和非空闲的节点）
unsigned int lua_gc_node_count(struct lua_gc_node* node) {
	if (node == NULL)
		return 0;
	struct lua_gc_node* child = node->first_child;
	unsigned int total = 1;
	while (child != NULL) {
		total += lua_gc_node_count(child);
		child = child->next_sibling;
	}
	return total;
}

// 设置描述
int lua_gc_node_set_desc(struct lua_gc_node* node, const char* desc) {
	if (node == NULL)
		return -1;
	strncpy(node->desc, desc, LUA_GC_NODE_DESC_SIZE - 1);
	return 0;
}

// 添加child节点
void lua_gc_node_add_child(struct lua_gc_node* father, struct lua_gc_node* son) {
	if (father == NULL || son == NULL)
		return;
	if (father->first_child) {
		son->next_sibling = father->first_child;
		father->first_child = son;
	} else {
		father->first_child = son;
	}
}

// 设置内存分配，释放函数
void lua_gc_node_set_mem_funcs(lua_gc_node_alloc_fn alloc, lua_gc_node_free_fn free) {
	mem_func.alloc = alloc;
	mem_func.free = free;
}

// 设置默认的内存分配、释放函数
void lua_gc_node_set_default_mem_funcs() {
	mem_func.alloc = default_alloc;
	mem_func.free = default_free;
}

static cJSON* lua_gc_node_to_jsonobject(struct lua_gc_node* node) {
	if (node == NULL) return NULL;
	cJSON* ret = cJSON_CreateObject();
	cJSON_AddStringToObject(ret, "name", node->name);
	cJSON_AddNumberToObject(ret, "type", node->type);
	cJSON_AddNumberToObject(ret, "refs", node->refs);
	cJSON_AddStringToObject(ret, "desc", node->desc);
	cJSON* child_array = cJSON_CreateArray();
	struct lua_gc_node* child = node->first_child;
	while (child != NULL) {
		cJSON_AddItemToArray(child_array, lua_gc_node_to_jsonobject(child));
		child = child->next_sibling;
	}
	cJSON_AddItemToObject(ret, "childs", child_array);
	return ret;
}

// 转换成json字符串
char* lua_gc_node_to_jsonstr(struct lua_gc_node* node) {
	if (node == NULL)
		return NULL;
	cJSON* jsonobj = lua_gc_node_to_jsonobject(node);
	char* ret = cJSON_PrintUnformatted(jsonobj);
	cJSON_free(jsonobj);
	return ret;
}

// 转换成json格式化的字符串
char* lua_gc_node_to_jsonstrfmt(struct lua_gc_node* node) {
	if (node == NULL)
		return NULL;
	cJSON* jsonobj = lua_gc_node_to_jsonobject(node);
	char* ret = cJSON_Print(jsonobj);
	cJSON_free(jsonobj);
	return ret;
}

// 复制单一node节点,其子节点和兄弟节点将被置NULL
struct lua_gc_node* lua_gc_node_copy(struct lua_gc_node* node) {
	if (node == NULL)
		return NULL;
	struct lua_gc_node* ret = mem_func.alloc();
	memcpy((void*)ret, (void*)node, sizeof(*node));
	ret->next_sibling = NULL;
	ret->first_child = NULL;
	return ret;
}

// 复制node节点及其所有子节点
struct lua_gc_node* lua_gc_node_copyall(struct lua_gc_node* node) {
	if (node == NULL)
		return NULL;
	struct lua_gc_node* ret = lua_gc_node_copy(node);
	struct lua_gc_node** child_ptr = &ret->first_child;
	struct lua_gc_node* child = node->first_child;
	while (child != NULL) {
		*child_ptr = lua_gc_node_copyall(child);
		child_ptr = &(*child_ptr)->next_sibling;
		child = child->next_sibling;
	}
	return ret;
}

// 将node节点以及其所有的子节点都添加到哈希表中
static void add_to_hashtable_by_ptr(struct lua_gc_node** htable, struct lua_gc_node* node) {
	if (htable == NULL || node == NULL)
		return;
	struct lua_gc_node* find_node = NULL;
	HASH_FIND(hh, *htable, &node->lua_obj_ptr, sizeof(node->lua_obj_ptr), find_node);
	if (find_node == NULL)
		HASH_ADD(hh, (*htable), lua_obj_ptr, sizeof(node->lua_obj_ptr), node);
	struct lua_gc_node* child = node->first_child;
	while (child != NULL) {
		add_to_hashtable_by_ptr(htable, child);
		child = child->next_sibling;
	}
}

// 根据哈希集求出node2的增节点
static struct lua_gc_node* lua_gc_node_incr_by_htable(struct lua_gc_node* htable, struct lua_gc_node* node) {
	if (htable == NULL || node == NULL)
		return NULL;
	struct lua_gc_node* find_node = NULL;
	HASH_FIND(hh, htable, &node->lua_obj_ptr, sizeof(node->lua_obj_ptr), find_node);
	struct lua_gc_node* ret = NULL;
	// 如果本节点不在哈希集中，则直接复制本节点及其所有子节点作为返回值
	if (find_node == NULL)
		ret = lua_gc_node_copyall(node);
	
	struct lua_gc_node* child = node->first_child;
	struct lua_gc_node* new_child = NULL;
	struct lua_gc_node** new_child_ptr = &new_child;
	while (child != NULL) {
		*new_child_ptr = lua_gc_node_incr_by_htable(htable, child);
		if (*new_child_ptr != NULL)
			new_child_ptr = &(*new_child_ptr)->next_sibling;
		child = child->next_sibling;
	}

	// 如果子节点存在增节点，则构建本节点
	if (new_child != NULL) {
		if (ret == NULL)
			ret = lua_gc_node_copy(node);
		ret->first_child = new_child;
	}
	return ret;
}

static struct lua_gc_node* lua_gc_node_incr(struct lua_gc_node* node1, struct lua_gc_node* node2) {
	if (node1 == NULL || node2 == NULL)
		return NULL;
	struct lua_gc_node* hash_table = NULL;
	// 先将node1的所有节点添加到哈希集
	add_to_hashtable_by_ptr(&hash_table, node1);
	// 然后根据对node2的每一个节点，都在哈希集中查找对应的节点是否存在，不存在则为增节点
	struct lua_gc_node* ret = lua_gc_node_incr_by_htable(hash_table, node2);
	return ret;
}

static struct lua_gc_node* lua_gc_node_decreased(struct lua_gc_node* node1, struct lua_gc_node* node2) {
	if (node1 == NULL || node2 == NULL)
		return NULL;
	
	struct lua_gc_node* hash_table = NULL;
	// 先将node2的所有节点都添加到哈希表
	add_to_hashtable_by_ptr(&hash_table, node2);
	// 然后根据对node1的每一个节点，都在哈希集中查找对应的节点是否存在，不存在则为减节点
	struct lua_gc_node* ret = lua_gc_node_incr_by_htable(hash_table, node1);
	return ret;
}

// 求node1到node2的差别
void lua_gc_node_diff(struct lua_gc_node* node1, struct lua_gc_node* node2, struct lua_gc_node** incr, struct lua_gc_node** decr) {
	if (node1 == NULL || node2 == NULL) {
		return;
	}
	if (incr != NULL) {
		*incr = lua_gc_node_incr(node1, node2);
	}
	if (decr != NULL) {
		*decr = lua_gc_node_decreased(node1, node2);
	}
}


#ifdef __cplusplus
}
#endif
