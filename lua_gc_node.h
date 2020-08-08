#ifndef _XLUA_SNAPSHOT_LUA_GC_NODE_H_
#define _XLUA_SNAPSHOT_LUA_GC_NODE_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include "uthash.h"
#define LUA_GC_NODE_NAME_SIZE 32
#define LUA_GC_NODE_DESC_SIZE 64
#define LUA_GC_NODE_LINK_SIZE 32

enum lua_gc_node_type {
	LUA_STRING_TYPE = 4,
	LUA_TTABLE_TYPE = 5,
	LUA_TFUNCTION_TYPE = 6,
	LUA_TUSERDATA_TYPE = 7,
	LUA_TTHREAD_TYPE = 8,
};

// 128字节
struct lua_gc_node {
	char name[LUA_GC_NODE_NAME_SIZE];		//节点的名称如 function:0x125200380, table:0x11d3530f0
	int type;  								//节点的类型如 string、table、function、userdata、thread
	unsigned int refs;			 			//引用次数
	char desc[LUA_GC_NODE_DESC_SIZE];		//节点描述
	char link[LUA_GC_NODE_LINK_SIZE]; 		//节点连接名称 如 _G, REGISTRY, 

	struct lua_gc_node* next_sibling; 		//兄弟节点
	struct lua_gc_node* first_child; 		//第一个子节点
	//unsigned long size;			 		// 该节点所占用内存量，叶子节点是自己节点的内存，非叶子节点是所有子节点的size之和
	const void* lua_obj_ptr; 				//指向lua对象的指针，唯一标识lua对象
	UT_hash_handle hh;
};

typedef struct lua_gc_node* (*lua_gc_node_alloc_fn)();
typedef void (*lua_gc_node_free_fn)(struct lua_gc_node*);

// 分配新节点
struct lua_gc_node* lua_gc_node_new(int type, const char* typestr, const void* pointer);
// 释放节点内存，同时释放自己点的内存
void lua_gc_node_free(struct lua_gc_node* node);
// 释放所有空闲节点的内存
void lua_gc_node_free_all();
// 统计所有节点的数量
unsigned int lua_gc_node_count(struct lua_gc_node* node);
// 设置描述
int lua_gc_node_set_desc(struct lua_gc_node* node, const char* desc);
// 设置link
int lua_gc_node_set_link(struct lua_gc_node* node, const char* link);
// 添加child节点
void lua_gc_node_add_child(struct lua_gc_node* father, struct lua_gc_node* son);
// 设置内存分配、释放函数
void lua_gc_node_set_mem_funcs(lua_gc_node_alloc_fn alloc, lua_gc_node_free_fn free);
// 设置默认的内存分配、释放函数
void lua_gc_node_set_default_mem_funcs();
// 转换成json字符串, 需要手动使用free来释放
char* lua_gc_node_to_jsonstr(struct lua_gc_node* node);
// 转换成json格式化的字符串，需要手动使用free来释放内存
char* lua_gc_node_to_jsonstrfmt(struct lua_gc_node* node);
// 复制单一node节点,其子节点和兄弟节点将被置NULL
struct lua_gc_node* lua_gc_node_copy(struct lua_gc_node* node);
// 复制node节点及其所有子节点
struct lua_gc_node* lua_gc_node_copyall(struct lua_gc_node* node);
// 求node1到node2的差别
// incr: 指向增加的对象的指针, 为null时不进行增量计算
// decr: 指向减少的对象的指针, 为null时不进行减量计算
void lua_gc_node_diff(struct lua_gc_node* node1, struct lua_gc_node* node2, struct lua_gc_node** incr, struct lua_gc_node** decr);

#ifdef __cplusplus
}
#endif

#endif /* _XLUA_SNAPSHOT_LUA_GC_NODE_ */
