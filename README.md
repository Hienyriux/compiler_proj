# compiler_proj
Project of Nanjing University's Principles and Techniques of Compilers course in 2019 autumn
## 将C--，C的简化版转化为MIPS汇编语言
### 支持:
+ 函数调用(包括递归)
+ 高维数组
### 不支持:
+ 结构体
+ 动态分配内存
### 过程:
+ 词法分析
+ 语法分析
+ 语义分析
+ 中间代码生成
+ 目标代码生成
### 使用的开源组件:
+ 词法分析: GNU Flex
+ 语法分析: GNU Bison
### TODO:
+ 中间代码优化(公共子表达式, 死代码消除, 常量传播)
+ 寄存器分配优化(活跃变量分析 + 图染色算法)
### 运行方式:
+ make
+ ./parser arg1 arg2 arg3
    + arg1: 分析的源代码
    + arg2: 中间代码输出
    + arg3: 目标代码输出
+ 使用SPIM运行目标代码
