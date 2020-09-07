#include<stdio.h>
#include<stdlib.h>
#include<string.h>

// _INT ... _WHILE 共27个
enum { _INT, _FLOAT, _ID, _SEMI, _COMMA, _ASSIGNOP, _RELOP, _PLUS, _MINUS, _STAR,
_DIV, _AND, _OR, _DOT, _NOT, _TYPE, _LP, _RP, _LB, _RB,
_LC, _RC, _STRUCT, _RETURN, _IF, _ELSE, _WHILE, _Program, _ExtDefList, _ExtDef,
_ExtDecList, _Specifier, _StructSpecifier, _OptTag, _Tag, _VarDec, _FunDec, _VarList, _ParamDec, _CompSt,
_StmtList, _Stmt, _DefList, _Def, _DecList, _Dec, _Exp, _Args };

enum { SYM_INT, SYM_FLOAT, SYM_UNDEF };

enum { IR_LABEL, IR_FUNC, IR_GOTO, IR_RET, IR_ARG, IR_PARAM, IR_READ, IR_WRITE, IR_ASSIGN, IR_DEC, IR_CALL, IR_ARITH, IR_IF };

// 语法树结点
typedef struct syntax_tree {
	int type;
	int lineno;
	union {
		int val_int;
		double val_double;
		int val_type;
		char *val_id;
		char val_relop[3];
	};
	// 子结点
	struct syntax_tree *c;
	// 兄弟节点
	struct syntax_tree *b;
}syntax_tree;

typedef struct symbol_list {
	struct symbol_tree *data;
	struct symbol_list *next;
}symbol_list;

// 符号表AVL树结点
typedef struct symbol_tree {
	// AVL树结构相关变量
	int bf;
	struct symbol_tree *left;
	struct symbol_tree *right;
	
	// 符号表相关变量
	char *name;
	int type;
	// 整型或浮点型变量的值
	union {
		int val_int;
		double val_double;
	};
	
	// 函数返回值类型
	int ret_type;
	// 函数形参列表
	struct symbol_list *arg_list;
	// 函数声明/定义状态, 0已声明未定义, 1已定义
	int def_dec_status;
	// 函数所在行号
	int lineno;

	// 数组维数
	int dim;
	// 数组每一维的大小
	int *size;
	// 数组每一维包含的元素数
	int *ele_num;
	// 关联一个中间代码变量名
	char *ir_name;
}symbol_tree;

typedef struct param_list {
	char *name;
	int type;
	int dim;
	syntax_tree *array_node;
	symbol_tree *symbol_node;
	struct param_list *next;
}param_list;

typedef struct ir_list {
	int type;
	char *args[4];
	struct ir_list *next;
}ir_list;

typedef struct match_list {
	struct symbol_tree *symbol_node;
	int type;
	// 数组剩下的维数
	int dim;
	// 已访问的维数
	int dim_trav;
	int lineno;
	// 多维数组访问每一维的位置值, 只记录中间结果名, 立即数也用字符串记录
	char **pos;
	// 中间结果名
	char *ir_name;
	struct match_list *next;
}match_list;

typedef struct array_info {
	int var_idx;
	int size;
	struct array_info *next;
}array_info;

typedef struct func_info {
	int ir_idx;
	char *name;
	int param_start;
	struct func_info *next;
}func_info;
