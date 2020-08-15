use libc::*;
use std::collections::HashMap;
use std::rc::Rc;
use crate::cJSON;

static mut STR_BUFFER: String = String::new();

#[derive(Copy, PartialEq)]
pub enum LuaGcNodeType {
    LuaStringType = 4,
    LuaTableType = 5,
    LuaFunctionType = 6,
    LuaUserDataType = 7,
    LuaThreadType = 8,

    Unknown,
}

impl From<c_int> for LuaGcNodeType {
    fn from(i: c_int) -> Self {
        match i {
            4 => LuaGcNodeType::LuaStringType,
            5 => LuaGcNodeType::LuaTableType,
            6 => LuaGcNodeType::LuaFunctionType,
            7 => LuaGcNodeType::LuaUserDataType,
            8 => LuaGcNodeType::LuaThreadType,
            _ => LuaGcNodeType::Unknown,
        }
    }
}

impl Clone for LuaGcNodeType {
    fn clone(&self) -> Self {
        return *self;
    }
}

#[derive(Copy, PartialEq)]
pub enum LuaGcNodeIncrOrDecr {
    Incr,
    Decr,
    No,
}

impl Clone for LuaGcNodeIncrOrDecr {
    fn clone(&self) -> Self {
        return *self;
    }
}

#[derive(Clone)]
pub struct LuaGcNode {
    pub name: String,
    pub node_type: LuaGcNodeType,
    pub incr_or_decr: LuaGcNodeIncrOrDecr,
    pub refs: u32,
    pub desc: String,
    pub link: String,

    pub next_sibling: Option<Box<LuaGcNode>>,
    pub first_child: Option<Box<LuaGcNode>>,
    pub lua_obj_ptr: *const c_void,
}

impl LuaGcNode {
    pub fn new(node_type: LuaGcNodeType, type_str: &str, pointer: *const c_void) -> Self {
        return Self {
            name: { format!("{}:{:p}", type_str, pointer) },
            node_type: node_type,
            incr_or_decr: LuaGcNodeIncrOrDecr::No,
            refs: 0,
            desc: String::new(),
            link: String::new(),

            next_sibling: None,
            first_child: None,
            lua_obj_ptr: pointer,
        };
    }

    pub fn count(&self) -> u32 {
        if self.first_child.is_none() {
            return 1;
        }
        let mut res = 1;
        let mut node = self.first_child.as_ref();
        while let Some(n) = node {
            res += n.count();
            node = n.next_sibling.as_ref();
        }
        return res;
    }

    pub fn set_desc(&mut self, desc: &str) {
        self.desc = String::from(desc);
    }

    pub fn set_link(&mut self, link: &str) {
        self.link = String::from(link);
    }

    pub fn copyall(&self) -> Self {
        let mut ret = Self {
            name: String::from(&self.name),
            node_type: self.node_type,
            incr_or_decr: self.incr_or_decr,
            refs: self.refs,

            desc: String::from(&self.desc),
            link: String::from(&self.link),

            next_sibling: None,
            first_child: None,
            lua_obj_ptr: self.lua_obj_ptr,
        };
        let mut child = self.first_child.as_ref();
        let mut new_child: Option<Box<LuaGcNode>> = None;
        let mut new_child_tail: &mut Option<Box<LuaGcNode>> = &mut new_child;
        while let Some(n) = child {
            let new_node = n.copyall();
            child = n.next_sibling.as_ref();
            *new_child_tail = Some(Box::new(new_node));
            new_child_tail = &mut (*new_child_tail).as_mut().unwrap().next_sibling;
        }
        if !new_child.is_none() {
            ret.first_child = new_child;
        }
        return ret;
    }

    pub fn copy(&self) -> Self {
        let mut ret = self.clone();
        ret.next_sibling = None;
        ret.first_child = None;
        return ret;
    }

    pub fn add_child(&mut self, mut node: LuaGcNode) {
        if self.first_child.is_none() {
            self.first_child = Some(Box::new(node));
        } else {
            let childs = self.first_child.take();
            node.next_sibling = childs;
            self.first_child = Some(Box::new(node));
        }
    }

    pub fn add_child_box(&mut self, mut node: Box<LuaGcNode>) {
        if self.first_child.is_none() {
            self.first_child = Some(node);
        } else {
            let childs = self.first_child.take();
            node.as_mut().next_sibling = childs;
            self.first_child = Some(node);
        }
    }

    pub fn map_childs<'a, F>(&'a self, mut f: F)
    where
        F: FnMut(&'a LuaGcNode),
    {
        let mut child = self.first_child.as_ref();
        while let Some(n) = child {
            child = n.next_sibling.as_ref();
            f(n);
        }
    }

    pub fn map<'a, F: std::clone::Clone>(&'a self, mut f: F)
    where
        F: FnMut(&'a LuaGcNode),
    {
        f(self);
        let mut child = self.first_child.as_ref();

        while let Some(n) = child {
            child = n.next_sibling.as_ref();
            n.map(f.clone());
        }
    }

    fn _map_with_height<'a, F: std::clone::Clone>(&'a self, mut f: F, h: i32)
    where
        F: FnMut(&'a LuaGcNode, i32),
    {
        f(self, h);
        let mut child = self.first_child.as_ref();

        while let Some(n) = child {
            child = n.next_sibling.as_ref();
            n._map_with_height(f.clone(), h + 1);
        }
    }

    pub fn map_with_height<'a, F: std::clone::Clone>(&'a self, f: F)
    where
        F: FnMut(&'a LuaGcNode, i32),
    {
        self._map_with_height(f, 0);
    }

    fn _to_str_single(&self, full_link: &mut String) {
        let line_str = format!(
            "{:>26}\t{:>6}\t{:>18}\t{}\n",
            &self.name, self.refs, &self.desc, &full_link
        );
        unsafe {
            STR_BUFFER.push_str(&line_str);
        }
    }

    fn _to_str_recursively(&self, full_link: &mut String, is_normal_node: bool) {
        // 记录full_link的长度
        let full_link_len = full_link.len();
        full_link.push_str(&self.link);
        // 如果是叶子节点
        if self.first_child.is_none() {
            self._to_str_single(full_link);
        } else {
            if is_normal_node {
                self._to_str_single(full_link);
            } else if self.incr_or_decr != LuaGcNodeIncrOrDecr::No {
                self._to_str_single(full_link);
            }
            full_link.push('.');
            let mut child = self.first_child.as_ref();
            while let Some(n) = child {
                n.as_ref()._to_str_recursively(full_link, is_normal_node);
                child = child.as_ref().unwrap().next_sibling.as_ref();
            }
        }
        // 回复full link
        full_link.truncate(full_link_len);
    }

    fn _is_normal_or_delta(&self) -> bool {
        if self.incr_or_decr != LuaGcNodeIncrOrDecr::No {
            return false;
        } else {
            let mut child = self.first_child.as_ref();
            while let Some(n) = child {
                if !n.as_ref()._is_normal_or_delta() {
                    return false;
                }
                child = child.as_ref().unwrap().next_sibling.as_ref();
            }
        }
        return true;
    }

    pub fn to_str(&self) -> &'static String {
        unsafe {
            STR_BUFFER.truncate(0);
            STR_BUFFER.push_str(&format!(
                "{:>26}\t{:>6}\t{:>18}\t{}\n",
                "name", "refs", "desc", "link"
            ));
        }
        let mut full_link_str = String::new();
        let is_normal_node = self._is_normal_or_delta();
        self._to_str_recursively(&mut full_link_str, is_normal_node);

        unsafe {
            return &STR_BUFFER;
        }
    }

    fn _add_to_hashtable<'a>(&'a self) -> HashMap<*const c_void, &'a LuaGcNode> {
        let mut htable = HashMap::new();

        let htable_ptr = &mut htable as *mut HashMap<*const c_void, &'a LuaGcNode>;
        // 插入数据
        self.map(|node| unsafe {
            let htable = &mut *htable_ptr;
            htable.insert(node.lua_obj_ptr, node);
        });
        return htable;
    }

    fn get_table_size_from_desc(&self) -> u32 {
        let mut start: usize = 0;
        let mut end: usize = 0;
        for (idx, c) in self.desc.as_bytes().iter().enumerate() {
            if *c >= '0' as u8 && *c <= '9' as u8 {
                start = idx;
                break;
            }
        }
        for (idx, c) in ((&self.desc)[start..]).as_bytes().iter().enumerate() {
            if *c < '0' as u8 || *c > '9' as u8 {
                end = idx;
                break;
            }
        }

        return (&self.desc[start..(start + end)]).parse().unwrap();
    }

    fn _get_incr_or_decr_by_htable(
        &self,
        htable: &HashMap<*const c_void, &LuaGcNode>,
        is_incr: bool,
    ) -> Option<Self> {
        let find_node = htable.get(&self.lua_obj_ptr);
        let mut ret: Option<Self> = None;
        if find_node.is_none() {
            ret = Some(self.copy());
            if is_incr {
                ret.as_mut().unwrap().incr_or_decr = LuaGcNodeIncrOrDecr::Incr;
                ret.as_mut().unwrap().desc.push_str("(+)");
            } else {
                ret.as_mut().unwrap().incr_or_decr = LuaGcNodeIncrOrDecr::Decr;
                ret.as_mut().unwrap().desc.push_str("(-)");
            }
        }

        let mut new_child: Option<Box<Self>> = None;
        let mut new_child_tail = &mut new_child as *mut Option<Box<Self>>;

        let mut child = self.first_child.as_ref();
        while let Some(n) = child {
            let res = n._get_incr_or_decr_by_htable(htable, is_incr);
            if !res.is_none() {
                unsafe {
                    *new_child_tail = Some(Box::new(res.unwrap()));
                    new_child_tail = &mut (*new_child_tail).as_mut().unwrap().next_sibling;
                }
            }
            child = n.next_sibling.as_ref();
        }

        // 如果子节点存在增/减节点
        if !new_child.is_none() {
            if ret.is_none() && is_incr {
                ret = Some(self.copy());
            } else if ret.is_none() && !is_incr {
                ret = Some(find_node.unwrap().copy());
            }
            ret.as_mut().unwrap().first_child = new_child;
        }

        // 如果类型是table，判断其size是否增加
        if self.node_type == LuaGcNodeType::LuaTableType && !find_node.is_none() {
            let tbl1_size = find_node.unwrap().get_table_size_from_desc();
            let tbl2_size = self.get_table_size_from_desc();
            if tbl2_size > tbl1_size {
                if ret.is_none() && is_incr {
                    ret = Some(self.copy());
                } else if ret.is_none() && !is_incr {
                    ret = Some(find_node.unwrap().copy());
                }

                if is_incr {
                    ret.as_mut()
                        .unwrap()
                        .desc
                        .push_str(&format!("(+{})", tbl2_size - tbl1_size));
                    ret.as_mut().unwrap().incr_or_decr = LuaGcNodeIncrOrDecr::Incr;
                } else {
                    ret.as_mut()
                        .unwrap()
                        .desc
                        .push_str(&format!("(-{})", tbl2_size - tbl1_size));
                    ret.as_mut().unwrap().incr_or_decr = LuaGcNodeIncrOrDecr::Decr;
                }
            }
        }
        return ret;
    }

    pub fn incr(&self, from: &Self) -> Option<Self> {
        let htable = from._add_to_hashtable();
        return self._get_incr_or_decr_by_htable(&htable, true);
    }

    pub fn decr(&self, from: &Self) -> Option<Self> {
        let htable = self._add_to_hashtable();
        return from._get_incr_or_decr_by_htable(&htable, false);
    }

    fn create_jsonobj(&self) -> *mut cJSON::cJSON {
        unsafe {
            let ret = cJSON::cJSON_CreateObject();
            cJSON::cJSON_AddStringToObject(ret, "name".as_ptr() as *const i8, self.name.as_ptr() as *const i8);
            cJSON::cJSON_AddNumberToObject(ret, "refs".as_ptr() as *const i8, self.refs.into());
            cJSON::cJSON_AddStringToObject(ret, "desc".as_ptr() as *const i8, self.desc.as_ptr() as *const i8);
            cJSON::cJSON_AddStringToObject(ret, "link".as_ptr() as *const i8, self.link.as_ptr() as *const i8);
            let array = cJSON::cJSON_CreateArray();
            let mut child = self.first_child.as_ref();
            while let Some(n) = child {
                cJSON::cJSON_AddItemToArray(array, n.create_jsonobj());
                child = n.next_sibling.as_ref();
            }
            cJSON::cJSON_AddItemToObject(ret, "childs".as_ptr() as *const i8, array);
            return ret;
        }
    }

    pub fn to_json(&self) -> *const i8 {
        unsafe {
            let json = self.create_jsonobj();
            let res = cJSON::cJSON_PrintUnformatted(json);
            cJSON::cJSON_Delete(json);
            return res;
        }
    }

    pub fn to_jsonfmt(&self) -> *const i8 {
        unsafe {
            let json = self.create_jsonobj();
            let res = cJSON::cJSON_Print(json);
            cJSON::cJSON_Delete(json);
            return res;
        }
    }
}
