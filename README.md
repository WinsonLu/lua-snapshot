## 1. 工具库介绍

​	`Snapshot`是一款用于Lua内存分析的Lua程序库，可以以快照的方式记录某一时刻lua虚拟机中的内存情况，并对不同时刻的内存快照进行增量/减量分析，以分析内存的变更情况，可用于检查和跟踪lua虚拟机中可能存在的内存泄漏问题。

​	`Snapshot`使用C语言编写，代码上参考了云风的[`Lua-Snapshot`内存泄漏检查工具](https://blog.codingnow.com/2012/12/lua_snapshot.html)，并在其基础上增加了对table大小变更的记录，内存引用链的跟踪、引用计数的统计、可提供更加友好的输出，并能针对特定table进行分析等功能。（至于为什么不使用Lua来实现，这是因为Lua代码的运行过程本身会影响State，即，你想对其进行观察，就可能改变它。最终使得分析的结果不太准确。）

## 2. 函数接口说明

​	`Snapshot`库提供了11个函数来支持内存分析功能，本节将介绍每一个函数的使用说明。

### 2.1 `snapshot()`函数

- 参数：`0`个或`2`个（`table`, `table`的名称)
- 返回值：`snapshot(userdata)`对象
- 作用：返回一个保存着当前时刻的`registry`表或某一table的内存快照的`snapshot`对象
- 使用样例：

![](https://github.com/WinsonLu/lua-snapshot/blob/master/assets/image-20200810025930964.png)

​	注意：

​	1）`snapshot`仅会对 **除`string`类型以外的GC对象类型**进行内存快照，包括:

​		 `table`、`function`、`thread`和`userdata`。

​	2）内存快照会忽略`snapshot`对象。

​	3）`snapshot`对象已经实现了`__gc`函数，可以被Lua虚拟机回收。

------

### 2.2 `print()`函数

- 参数：`1`个（`snapshot`对象）
- 返回值：无
- 作用：打印快照到标准输出
- 使用样例：

|                             代码                             |                           打印结果                           |
| :----------------------------------------------------------: | :----------------------------------------------------------: |
| ![image-20200810031505860](https://github.com/WinsonLu/lua-snapshot/blob/master/assets/image-20200810031505860.png) | ![image-20200810031531442](https://github.com/WinsonLu/lua-snapshot/blob/master/assets/image-20200810031531442.png) |

------

### 2.3 `print_jsonfmt()`函数

- 参数：`1`个（`snapshot`对象）
- 返回值：无
- 作用：以格式化json字符串的方式打印快照到标准输出
- 使用样例：

|                             代码                             |                           运行结果                           |
| :----------------------------------------------------------: | :----------------------------------------------------------: |
| ![image-20200810031702112](https://github.com/WinsonLu/lua-snapshot/blob/master/assets/image-20200810031702112.png) | ![image-20200810031750428](https://github.com/WinsonLu/lua-snapshot/blob/master/assets/image-20200810031750428.png) |

------

### 2.4 `print_json()`函数

- 参数：`1`个（`snapshot`对象）
- 返回值：无
- 作用：以非格式化json字符串的方式打印快照到标准输出
- 使用样例：

|     代码     | ![image-20200810031928114](https://github.com/WinsonLu/lua-snapshot/blob/master/assets/image-20200810031928114.png) |
| :----------: | :----------------------------------------------------------: |
| **运行结果** | ![image-20200810032014658](https://github.com/WinsonLu/lua-snapshot/blob/master/assets/image-20200810032014658.png) |

------

### 2.5 `to_file()`函数

- 参数：`2`个（`snapshot`对象，保存的文件路径名）
- 返回值：无
- 作用：将快照内容保存到指定文件
- 使用样例：

![image-20200810033352528](https://github.com/WinsonLu/lua-snapshot/blob/master/assets/image-20200810033352528.png)

------

### 2.6 `to_jsonfile()`函数

- 参数：`2`个（`snapshot`对象，保存的文件路径名）
- 返回值：无
- 作用：将快照内容以格式化json字符串的方式保存到指定文件
- 使用样例：参考`2.5 to_file()函数`

------

### 2.7 `to_jsonfilefmt()`函数

- 参数：`2`个（`snapshot`对象，保存的文件路径名）
- 返回值：无
- 作用：将快照内容以非格式化json字符串的方式保存到指定文件
- 使用样例：参考`2.5 to_file()函数`

------

### 2.8 `copy()`函数

- 参数：1个（`snapshot`对象）
- 返回值：`snapshot(userdata)`对象
- 作用：复制并返回一个`snapshot(userdata)`对象
- 使用样例：

|                             代码                             |                           运行结果                           |
| :----------------------------------------------------------: | :----------------------------------------------------------: |
| ![image-20200810033705571](https://github.com/WinsonLu/lua-snapshot/blob/master/assets/image-20200810033705571.png) | ![image-20200810033841872](https://github.com/WinsonLu/lua-snapshot/blob/master/assets/image-20200810033841872.png) |

------

### 2.9 `incr()`函数

- 参数：`2`个（`snapshot`对象1， `snapshot`对象2）
- 返回值：`snapshot(userdata)`对象
- 作用：求出snapshot1 到 snapshot2 的增量，并返回一个`snapshot(userdata)`对象
- 使用样例：

|                             代码                             |                           运行结果                           |
| :----------------------------------------------------------: | :----------------------------------------------------------: |
| ![image-20200810034259049](https://github.com/WinsonLu/lua-snapshot/blob/master/assets/image-20200810034259049.png) | ![image-20200810041452817](https://github.com/WinsonLu/lua-snapshot/blob/master/assets/image-20200810041452817.png) |

------

### 2.10 `decr()`函数

- 参数：`2`个（`snapshot`对象1， `snapshot`对象2）
- 返回值：`snapshot(userdata)`对象
- 作用：求出snapshot1 到 snapshot2 的减量，并返回一个`snapshot(userdata)`对象
- 使用样例：

|                             代码                             |                           运行结果                           |
| :----------------------------------------------------------: | :----------------------------------------------------------: |
| ![image-20200810040505277](https://github.com/WinsonLu/lua-snapshot/blob/master/assets/image-20200810040505277.png) | ![image-20200810040524724](https://github.com/WinsonLu/lua-snapshot/blob/master/assets/image-20200810040524724.png) |

------

### 2.11 `free()`函数

- 参数：`1`个（`snapshot`对象）

- 返回值：无

- 作用：手动释放snapshot(userdata)对象占用的内存。

  ​			虽然`snapshot`对象已经实现了`__gc`函数，但是由于`snapshot`对象在创建时只会向lua虚拟机中申请**`一个指针大小的内存`**，因此在大量创建snapshot对象的场景，往往无法触及lua虚拟机的GC内存阈值，从而导致在实际内存占用较高时，这些snapshot对象仍然不会被回收的情况发生。因此最好的使用方式是按时手动进行`collectgarbage`调用（这里可以后续考虑在C语言代码中自动调用，而不是在lua层手动调用）或每次都使用`free()`函数手动释放不再使用的snapshot对象所占用的内存。

- 使用样例：

|                             代码                             |                           运行结果                           |
| :----------------------------------------------------------: | :----------------------------------------------------------: |
| ![image-20200810042202987](https://github.com/WinsonLu/lua-snapshot/blob/master/assets/image-20200810042202987.png) | ![image-20200810042251643](https://github.com/WinsonLu/lua-snapshot/blob/master/assets/image-20200810042251643.png) |


