#ifdef __cplusplus
extern "C" {
#endif
#include "lua_gc_node.h"
#include "cJSON.h"
#include <stdbool.h>
#define DEFAULT_ALLOC_SIZE (sizeof(struct lua_gc_node)*32)
#define DEFAULT_BUFF_SIZE 512

static __thread char buff[DEFAULT_BUFF_SIZE];

static struct lua_gc_node* default_alloc();
static void default_free(struct lua_gc_node*);


// 内存分配链表
struct mem_buff_header {
	struct mem_buff_header* next;
	void* start;
	void* curr;
	void* end;
};

static __thread struct mem_buff_header* mem_buff_list = NULL;
static __thread struct mem_buff_header* mem_buff_list_tail = NULL;

static __thread struct lua_gc_node* free_list = NULL;

// 存放str内存
static __thread char* strbuff = NULL;
static __thread long strbuff_len = 0;

struct lua_gc_node_mem_fn {
	lua_gc_node_alloc_fn alloc;
	lua_gc_node_free_fn free;
};
static struct lua_gc_node_mem_fn  mem_func = {
	default_alloc,
	default_free
};

// 内存池扩展
static inline void realloc_mem_buff() {
	// 如果没有分配过内存，则分配
	struct mem_buff_header* header = (struct mem_buff_header*)malloc(sizeof(struct mem_buff_header) + DEFAULT_ALLOC_SIZE);
	header->start = (void*)(header + 1);
	header->curr = header->start;
	header->end = (char*)header + sizeof(struct mem_buff_header) + DEFAULT_ALLOC_SIZE;
	header->next = NULL;
	if (mem_buff_list_tail == NULL) {
		header->next = NULL;
		mem_buff_list = mem_buff_list_tail = header;
	}
	else {
		mem_buff_list_tail->next = header;
		mem_buff_list_tail = header;
	}
}

// 查询内存池是否能分配size大小内存
static inline bool mem_buff_has_buffer(size_t size) {
	if (mem_buff_list_tail == NULL)
		return false;
	
	return ((char*)mem_buff_list_tail->end - (char*)mem_buff_list_tail->curr >= size);
}

// 向内存池中申请size大小的内存
static inline void* mem_buff_alloc_size(size_t size) {
	if (!mem_buff_has_buffer(size)) {
		realloc_mem_buff();
		mem_buff_list_tail->curr = (void*)((char*)mem_buff_list_tail->curr + size);
		return mem_buff_list_tail->start;
	} else {
		void *ret = mem_buff_list_tail->curr;
		mem_buff_list_tail->curr = (void*)((char*)mem_buff_list_tail->curr + size);
		return ret;
	}
}


// 默认内存分配函数
static struct lua_gc_node* default_alloc() {
	if (free_list == NULL) {
		struct lua_gc_node* ret = (struct lua_gc_node*)mem_buff_alloc_size(sizeof(struct lua_gc_node));
		return ret;
	} else {
		struct lua_gc_node* ret = free_list;
		free_list = free_list->next_sibling;
		return ret;
	}
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

// 释放所有空闲节点和str内存
void lua_gc_node_free_all() {
	struct mem_buff_header* header = mem_buff_list;
	struct mem_buff_header* next_header = NULL;
	while (header != NULL) {
		next_header = header->next;
		free(header);
		header = next_header;
	}
	mem_buff_list = NULL;
	mem_buff_list_tail = NULL;
	if (strbuff != NULL)
		free(strbuff);
	strbuff = NULL;
	strbuff_len = 0;
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

// 设置link
int lua_gc_node_set_link(struct lua_gc_node* node, const char* link) {
	if (node == NULL)
		return -1;
	strncpy(node->link, link, LUA_GC_NODE_LINK_SIZE - 1);
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
	cJSON_AddStringToObject(ret, "link", node->link);
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
	cJSON_Delete(jsonobj);
	return ret;
}

// 转换成json格式化的字符串
char* lua_gc_node_to_jsonstrfmt(struct lua_gc_node* node) {
	if (node == NULL)
		return NULL;
	cJSON* jsonobj = lua_gc_node_to_jsonobject(node);
	char* ret = cJSON_Print(jsonobj);
	cJSON_Delete(jsonobj);
	return ret;
}

// 为strbuff分配内存，
// need_copy是否将旧的缓冲区的内容复制到新的缓冲区
static bool realloc_strbuff(long size, bool need_copy) {
	if (size <= strbuff_len)
		return true;

	char* newstrbuff = (char*)malloc(sizeof(char)*size);
	if (!need_copy && strbuff != NULL)
		free(strbuff);
	else if (need_copy && strbuff != NULL) {
		memcpy(newstrbuff, strbuff, strbuff_len);
		free(strbuff);
	}
	strbuff = newstrbuff;
	strbuff_len = size;
	return true;
}

// 将单一节点打印到strbuff中，如果strbuff长度不足，会进行扩容
static inline void lua_gc_node_to_str_single(struct lua_gc_node* node, const char* full_link, long* total_str_len) {
	if (node == NULL)
		return;
	snprintf(buff, sizeof(buff), "%26s\t%6d\t%18s\t%s\n", node->name, node->refs, node->desc, full_link);
	if (strbuff_len - *total_str_len < 512) {
		realloc_strbuff(strbuff_len*2, true);
	}
	strncat(strbuff, buff, strbuff_len - 1);
	*total_str_len += strlen(buff);
}

// 将节点和其所有子节点都转化成str格式化字符串，保存在strbuff中
static void lua_gc_node_to_str_recursively(struct lua_gc_node* node, char* node_link_buff, long node_link_buff_len, long* total_str_len, bool is_normal_node) {
	if (node == NULL)
		return;
	// 修改full link
	strncat(node_link_buff, node->link, 256 - 1);
	// 如果是叶子节点,只打印本节点
	if (node->first_child == NULL) {
		lua_gc_node_to_str_single(node, node_link_buff, total_str_len);
	}
	// 如果不是
	else {
		// 如果是normal节点，打印本节点
		if (is_normal_node)
			lua_gc_node_to_str_single(node, node_link_buff, total_str_len);
		// 如果是非normal节点，根据其是否是增/减节点，选择性打印
		else if (node->is_incr_or_decr != 0) {
			lua_gc_node_to_str_single(node, node_link_buff, total_str_len);
		}
		long link_len = strlen(node->link);
		strncat(node_link_buff, ".", 256 - 1);
		link_len++;
		struct lua_gc_node* child = node->first_child;
		while (child != NULL) {
			lua_gc_node_to_str_recursively(child, node_link_buff, node_link_buff_len + link_len, total_str_len, is_normal_node);
			child = child->next_sibling;
		}
	}
	// 恢复 full link
	node_link_buff[node_link_buff_len] = 0;
}

// 判断一个节点对应的snapshot是普通snapshot还是增量/减量返回的snapshot
// true: normal
bool is_normal_or_delta_node(struct lua_gc_node* node) {
	if (node == NULL)
		return true;
	if (node->is_incr_or_decr != 0)
		return false;
	struct lua_gc_node* child = node->first_child;
	while (child != NULL) {
		if (!is_normal_or_delta_node(child))
			return false;
		child = child->next_sibling;
	}

	return true;
}

char* lua_gc_node_to_str(struct lua_gc_node* node) {
	if (strbuff == NULL)
		realloc_strbuff(4096, false);
	char node_link_buff[256] = "";
	snprintf(strbuff, strbuff_len, "%26s\t%6s\t%18s\t%s\n", "name", "refs", "desc", "link");
	long total_str_len = strlen(strbuff);
	bool is_normal_node = is_normal_or_delta_node(node);
	lua_gc_node_to_str_recursively(node, node_link_buff, 0, &total_str_len, is_normal_node);
	strbuff[total_str_len] = 0;
	return strbuff;
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

// 从tbl的desc中取出size
// 样例desc: (size: 10) -> 10
static int get_table_size_from_desc(const char* desc) {
	if (desc == NULL)
		return 0;
	while (*desc != 0 && !(*desc >= '0' && *desc <= '9'))
		desc++;
	if (*desc == 0)
		return 0;
	char* ptr = buff;
	while (*desc != 0 && (*desc >= '0' && *desc <= '9')) {
		*ptr++ = *desc++;
	}
	*ptr = '\0';
	return atoi(buff);
}

// 根据哈希集求出node2的增节点/或减节点
static struct lua_gc_node* lua_gc_node_incr_or_decr_by_htable(struct lua_gc_node* htable, struct lua_gc_node* node, bool is_incr) {
	if (htable == NULL || node == NULL)
		return NULL;
	struct lua_gc_node* find_node = NULL;
	HASH_FIND(hh, htable, &node->lua_obj_ptr, sizeof(node->lua_obj_ptr), find_node);
	struct lua_gc_node* ret = NULL;
	// 如果本节点不在哈希集中，则复制本节点
	if (find_node == NULL) {
		ret = lua_gc_node_copy(node);
		if (is_incr) {
			// 标识为增节点
			ret->is_incr_or_decr = 1;
			strncat(ret->desc, "(+)", LUA_GC_NODE_DESC_SIZE);
		}
		else {
			// 标识为减节点
			ret->is_incr_or_decr = -1;
			strncat(ret->desc, "(-)", LUA_GC_NODE_DESC_SIZE);
		}
	}
	
	struct lua_gc_node* child = node->first_child;
	struct lua_gc_node* new_child = NULL;
	struct lua_gc_node** new_child_ptr = &new_child;
	while (child != NULL) {
		*new_child_ptr = lua_gc_node_incr_or_decr_by_htable(htable, child, is_incr);
		if (*new_child_ptr != NULL)
			new_child_ptr = &(*new_child_ptr)->next_sibling;
		child = child->next_sibling;
	}

	// 如果子节点存在增节点/减节点
	if (new_child != NULL) {
		if (ret == NULL && is_incr)
			ret = lua_gc_node_copy(node);
		else if (ret == NULL && !is_incr)
			ret = lua_gc_node_copy(find_node);
		ret->first_child = new_child;
	} 
	// 如果类型是table，判断其size是否增加
	if (node->type == LUA_TTABLE && find_node != NULL) {
		int tbl1_size = get_table_size_from_desc(find_node->desc);
		int tbl2_size = get_table_size_from_desc(node->desc);
		if (tbl2_size > tbl1_size) {
			if (ret == NULL && is_incr)
				ret = lua_gc_node_copy(node);
			else if (ret == NULL && !is_incr)
				ret = lua_gc_node_copy(find_node);

			if (is_incr) {
				snprintf(buff, sizeof(buff), "(+%d)", tbl2_size - tbl1_size);
				// 标识为增节点
				ret->is_incr_or_decr = 1;
			}
			else {
				snprintf(buff, sizeof(buff), "(-%d)", tbl2_size - tbl1_size);
				// 标识为减节点
				ret->is_incr_or_decr = -1;
			}
			strncat(ret->desc, buff, LUA_GC_NODE_DESC_SIZE);
		}
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
	struct lua_gc_node* ret = lua_gc_node_incr_or_decr_by_htable(hash_table, node2, true);
	HASH_CLEAR(hh, hash_table);
	return ret;
}

static struct lua_gc_node* lua_gc_node_decr(struct lua_gc_node* node1, struct lua_gc_node* node2) {
	if (node1 == NULL || node2 == NULL)
		return NULL;
	
	struct lua_gc_node* hash_table = NULL;
	// 先将node2的所有节点都添加到哈希表
	add_to_hashtable_by_ptr(&hash_table, node2);
	// 然后根据对node1的每一个节点，都在哈希集中查找对应的节点是否存在，不存在则为减节点
	struct lua_gc_node* ret = lua_gc_node_incr_or_decr_by_htable(hash_table, node1, false);
	HASH_CLEAR(hh, hash_table);
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
		*decr = lua_gc_node_decr(node1, node2);
	}
}

#ifdef __cplusplus
}
#endif
