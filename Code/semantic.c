#include"semantic.h"
#define IR_TOKEN_LEN 16
#define ERROR_RETURN_VOID has_error=1;return;
#define ERROR_RETURN_INT has_error=1;return 1;

extern symbol_tree *symbol_root;
extern int has_error;

match_list *stk_top = NULL;
int stk_size = 0;

int func_ret_type;

int ir_var_id = 0;
char ir_buf0[IR_TOKEN_LEN], ir_buf1[IR_TOKEN_LEN];
ir_list *ir_head = NULL, *ir_tail = NULL;

void branch_without_else(syntax_tree *, syntax_tree *);
void branch_with_else(syntax_tree *, syntax_tree *);
void loop_proc(syntax_tree *, syntax_tree *);
int func_def_dec(syntax_tree *, int);
symbol_tree *bi_search(char *);
void array_dec(syntax_tree *, symbol_tree *, int, int);
void ir_insert(int, char *, char *, char *, char *);
void local_var(syntax_tree *, int);
void assign_proc(match_list *, match_list *);
int array_addr(match_list *);
int exp_proc(syntax_tree *, int, int);
int exp_assign(syntax_tree *);
int exp_binary(syntax_tree *, int, int);
int exp_and(syntax_tree *, int, int);
int exp_or(syntax_tree *, int, int);
int exp_relop(syntax_tree *, int, int);
int exp_minus(syntax_tree *);
int exp_not(syntax_tree *, int, int);
int exp_call(syntax_tree *);
void arg_param_mismatch(syntax_tree *, symbol_tree *, int);
void exp_read(syntax_tree *, symbol_tree *);
void exp_write(syntax_tree *, symbol_tree *);
void exp_func(syntax_tree *, int, symbol_tree *);
int exp_array(syntax_tree *);
int exp_basic(syntax_tree *);
void undef_func(symbol_tree *);
void label_cut();

// 遍历语法树, 生成中间代码
void syntax_tree_to_ir(syntax_tree *cur) {
	if(cur == NULL)
		return;
	// ExtDefList --> ExtDef ExtDefList
	if(cur->type == _ExtDef) {
		// 函数定义或声明
		if(cur->c->b->type == _FunDec) {
			// 函数定义, 形参列表后面跟了一个语句块
			// ExtDef --> Specifier FunDec CompSt
			if(cur->c->b->b->type == _CompSt) {
				// 检查返回值, 函数名, 形参列表; 即使这些部分出错, 还要继续检查
				// cur->c是Specifier
				if(func_def_dec(cur->c, 1)) {
					// 检查函数体
					syntax_tree_to_ir(cur->c->b->b);
					// 检查相邻的ExtDefList
					syntax_tree_to_ir(cur->b);
					return;
				}
			}
			// 函数声明, 形参列表后面直接是分号
			// ExtDef --> Specifier FunDec SEMI
			else if(cur->c->b->b->type == _SEMI) {
				if(func_def_dec(cur->c, 0)) {
					// 检查相邻的ExtDefList
					syntax_tree_to_ir(cur->b);
					return;
				}
			}
		}
		// 结构体定义, 暂时不支持
		// ExtDef --> Specifier SEMI
		else if(cur->c->b->type == _SEMI) {
			printf("Line %d: Code contains variables or parameters of structure type (currently unsupported).\n", cur->c->b->lineno);
			ERROR_RETURN_VOID
		}
		// 全局变量定义, 暂时不支持
		// ExtDef --> Specifier ExtDecList SEMI
		else if(cur->c->b->type == _ExtDecList) {
			printf("Line %d: Code contains global variables (currently unsupported).\n", cur->c->b->lineno);
			ERROR_RETURN_VOID
		}
	}
	// 局部变量定义
	// DefList --> Def DefList
	// Def --> Specifier DecList SEMI
	else if(cur->type == _Def) {
		if(cur->c == NULL)
			return;
		// 结构体, 暂时不支持
		// Specifier --> StructSpecifier
		if(cur->c->c->type == _StructSpecifier) {
			printf("Line %d: Code contains variables or parameters of structure type (currently unsupported).\n", cur->c->c->lineno);
			ERROR_RETURN_VOID
		}
		// 非结构体
		// DecList --> Dec
		// DecList --> Dec COMMA DecList
		// cur->c->b->c指向Dec
		// cur->c->c->val_type是Specifier的类型
		else
			local_var(cur->c->b->c, cur->c->c->val_type);
	}
	else if(cur->type == _Stmt) {
		// Stmt --> RETURN Exp SEMI
		if(cur->c->type == _RETURN) {
			if(exp_proc(cur->c->b->c, -1, -1))
				return;
			if(stk_top->type != func_ret_type) {
				printf("Line %d: Type mismatched for return.\n", cur->c->b->c->lineno);
				ERROR_RETURN_VOID
			}
			ir_insert(IR_RET, stk_top->ir_name, NULL, NULL, NULL);
		}
		// Stmt --> Exp SEMI
		else if(cur->c->type == _Exp) {
			if(exp_proc(cur->c->c, -1, -1)) {
				ERROR_RETURN_VOID
			}
		}
		// Stmt --> CompSt
		else if(cur->c->type == _CompSt)
			return syntax_tree_to_ir(cur->c);
		else {
			syntax_tree *p = cur->c->b->b;
			// Stmt --> IF LP Exp RP Stmt
			if(cur->c->type == _IF && p->b->b->b == NULL)
				branch_without_else(cur, p);
			// Stmt --> IF LP Exp RP Stmt ELSE Stmt
			else if(cur->c->type == _IF)
				branch_with_else(cur, p);
			// Stmt --> WHILE LP Exp RP Stmt
			else if(cur->c->type == _WHILE)
				loop_proc(cur, p);
			return;
		}
		if(cur->b == NULL || cur->b->type == _ELSE)
			return;
	}
	else if(cur->type == _Specifier || cur->type == _FunDec || cur->type < _Program)
		return syntax_tree_to_ir(cur->b);
	syntax_tree_to_ir(cur->c);
	syntax_tree_to_ir(cur->b);
}

void branch_without_else(syntax_tree *cur, syntax_tree *p) {
	int label1 = ir_var_id, label2 = label1 + 1;
	ir_var_id += 2;
	if(exp_proc(p->c, label1, label2)) {
		ERROR_RETURN_VOID
	}
	
	// 条件运算的结果为整数
	if(stk_top->type != SYM_INT) {
		printf("Line %d: Type mismatched for operands.\n", p->lineno);
		ERROR_RETURN_VOID
	}
	
	sprintf(ir_buf0, "t%d", label1);
	ir_insert(IR_LABEL, ir_buf0, NULL, NULL, NULL);
	
	syntax_tree_to_ir(p->b->b);
	
	sprintf(ir_buf0, "t%d", label2);
	ir_insert(IR_LABEL, ir_buf0, NULL, NULL, NULL);
	
	syntax_tree_to_ir(cur->b);
}

void branch_with_else(syntax_tree *cur, syntax_tree *p) {
	int label1 = ir_var_id, label2 = ir_var_id + 1, label3 = ir_var_id + 2;
	ir_var_id += 3;
	if(exp_proc(p->c, label1, label2)) {
		ERROR_RETURN_VOID
	}
	
	// 条件运算的结果为整数
	if(stk_top->type != SYM_INT) {
		printf("Line %d: Type mismatched for operands.\n", p->lineno);
		ERROR_RETURN_VOID
	}

	sprintf(ir_buf0, "t%d", label1);
	ir_insert(IR_LABEL, ir_buf0, NULL, NULL, NULL);
	
	syntax_tree_to_ir(p->b->b);
	
	sprintf(ir_buf0, "t%d", label3);
	ir_insert(IR_GOTO, ir_buf0, NULL, NULL, NULL);
	
	sprintf(ir_buf0, "t%d", label2);
	ir_insert(IR_LABEL, ir_buf0, NULL, NULL, NULL);
	
	syntax_tree_to_ir(p->b->b->b->b);
	
	sprintf(ir_buf0, "t%d", label3);
	ir_insert(IR_LABEL, ir_buf0, NULL, NULL, NULL);
	
	syntax_tree_to_ir(cur->b);
}

void loop_proc(syntax_tree *cur, syntax_tree *p) {
	int label1 = ir_var_id, label2 = ir_var_id + 1, label3 = ir_var_id + 2;
	ir_var_id += 3;
	
	sprintf(ir_buf0, "t%d", label1);
	ir_insert(IR_LABEL, ir_buf0, NULL, NULL, NULL);
	
	if(exp_proc(p->c, label2, label3)) {
		ERROR_RETURN_VOID
	}

	sprintf(ir_buf0, "t%d", label2);
	ir_insert(IR_LABEL, ir_buf0, NULL, NULL, NULL);
	
	syntax_tree_to_ir(p->b->b);
	
	sprintf(ir_buf0, "t%d", label1);
	ir_insert(IR_GOTO, ir_buf0, NULL, NULL, NULL);
	
	sprintf(ir_buf0, "t%d", label3);
	ir_insert(IR_LABEL, ir_buf0, NULL, NULL, NULL);
	
	syntax_tree_to_ir(cur->b);
}

// 检查函数(定义或声明)的返回值, 函数名, 形参列表, 配置符号表结点记录的函数信息, 生成准备形参的中间代码
int func_def_dec(syntax_tree *cur, int mode) {
	// 暂时不支持返回结构体类型
	if(cur->c->type != _TYPE) {
		printf("Line %d: Code contains variables or parameters of structure type (currently unsupported).\n", cur->c->lineno);
		ERROR_RETURN_INT
	}
	int ret_type = cur->c->val_type;
	
	// 检查函数是否重名
	char *name = cur->b->c->val_id;
	symbol_tree *p = bi_search(name);
	// 已作为变量声明
	if(p->type != SYM_UNDEF && p->ret_type == SYM_UNDEF) {
		printf("Line %d: The function's name was previously used as a variable's name.\n", cur->b->c->lineno);
		ERROR_RETURN_INT
	}
	
	// 已作为函数声明, 但返回类型不一致, 抛弃前一个声明, 永远以最新的声明为准
	if(p->ret_type != SYM_UNDEF && p->def_dec_status == 0 && ret_type != p->ret_type)
		printf("Line %d: Inconsistent declaration of function \"%s\".\n", cur->b->c->lineno, name);

	// 已有定义, 又重复定义, 抛弃前一个定义
	if(p->ret_type != SYM_UNDEF && p->def_dec_status == 1 && mode == 1)
		printf("Line %d: Redefined function \"%s\".\n", cur->b->c->lineno, name);

	param_list *parameter_list = NULL;
	// ExtDef --> Specifier FunDec CompSt
	// ExtDef --> Specifier FunDec SEMI
	// FunDec --> ID LP VarList RP
	// FunDec --> ID LP RP
	// 有形参
	if(cur->b->c->b->b->type == _VarList) {
		// VarList --> ParamDec COMMA VarList
		// ParamDec --> Specifier VarDec
		// q指向ParamDec
		syntax_tree *q = cur->b->c->b->b->c;
		while(q != NULL) {
			if(q->type == _ParamDec) {
				// r指向Specifier
				syntax_tree *r = q->c;
				if(r->c->type != _TYPE) {
					printf("Line %d: Code contains variables or parameters of structure type (currently unsupported).\n", r->c->lineno);
					ERROR_RETURN_INT
				}

				// VarDec --> ID
				// VarDec --> VarDec LB INT RB
				// 找到VarDec中ID的位置
				int dim = 0;
				// s指向VarDec的子结点, 可以是ID, 也可以是VarDec
				syntax_tree *s = r->b->c;
				// s每指向一次VarDec, 则数组维数多一维
				while(s->type != _ID) {
					s = s->c;
					dim++;
				}

				param_list *t = (param_list *)malloc(sizeof(param_list));
				t->name = s->val_id;
				t->type = r->c->val_type;
				t->dim = dim;
				if(dim > 0)
					t->array_node = r->b->c;
				t->next = parameter_list;
				parameter_list = t;
			}
			if(q->type == _VarList)
				q = q->c;
			else
				q = q->b;
		}
	}

	// 如果函数之前声明或定义过, 检查形参列表匹配情况
	if(p->ret_type != SYM_UNDEF) {
		param_list *q = parameter_list;
		symbol_list *r = p->arg_list;
		for(; q != NULL && r != NULL && q->type == r->data->type && q->dim == r->data->dim; q = q->next, r = r->next);
		if(q || r)
			printf("Line %d: Inconsistent declaration or definition of function \"%s\".\n", cur->b->c->lineno, name);
		// 重置之前声明或定义的形参变量的状态, 清空之前声明或定义的形参列表
		while(p->arg_list) {
			symbol_list *s = p->arg_list;
			p->arg_list = p->arg_list->next;
			s->data->type = SYM_UNDEF;
			free(s);
		}
	}

	// 检查形参是否重名
	for(param_list *q = parameter_list;  q != NULL; q = q->next) {
		symbol_tree *r = bi_search(q->name);
		if(r->type != SYM_UNDEF) {
			printf("Line %d: Redefined variable \"%s\".\n", p->lineno, q->name);
			ERROR_RETURN_INT
		}
		q->symbol_node = r;
	}
	
	// 设置函数的符号表结点
	p->ret_type = ret_type;
	for(param_list *q = parameter_list;  q != NULL; q = q->next) {
		// 对本次定义的形参生成关联的中间代码变量名
		sprintf(ir_buf0, "t%d", ir_var_id);
		ir_var_id++;
		q->symbol_node->ir_name = (char *)malloc((strlen(ir_buf0) + 1) * sizeof(char));
		strcpy(q->symbol_node->ir_name, ir_buf0);
		
		q->symbol_node->dim = q->dim;
		q->symbol_node->type = q->type;
		// 配置数组形参
		if(q->dim > 0)
			array_dec(q->array_node, q->symbol_node, q->dim, 1);
	}

	// 分配中间代码中的函数名
	if(strcmp(name, "main") == 0)
		strcpy(ir_buf0, "main");
	else {
		sprintf(ir_buf0, "t%d", ir_var_id);
		ir_var_id++;
	}
	p->ir_name = (char *)malloc((strlen(ir_buf0) + 1) * sizeof(char));
	strcpy(p->ir_name, ir_buf0);
	ir_insert(IR_FUNC, ir_buf0, NULL, NULL, NULL);
	
	// 在中间代码中添加参数准备语句
	// 形参链表反转
	param_list *q = NULL, *r = parameter_list, *s = NULL;
	while(r != NULL) {
		s = r->next;
		r->next = q;
		q = r;
		r = s;
	}
	parameter_list = q;
	for(; q != NULL; q = q->next) {
		// 设置新的形参列表
		symbol_list *t = (symbol_list *)malloc(sizeof(symbol_list));
		t->data = q->symbol_node;
		t->next = p->arg_list;
		p->arg_list = t;
		ir_insert(IR_PARAM, q->symbol_node->ir_name, NULL, NULL, NULL);
	}
	// 形参链表清除
	while(parameter_list) {
		q = parameter_list;
		parameter_list = parameter_list->next;
		free(q);
	}

	// 函数定义, 设置返回类型, 方便检查return语句
	if(mode == 1) {
		p->def_dec_status = 1;
		func_ret_type = ret_type;
	}
	// 函数声明
	else {
		p->def_dec_status = 0;
		p->lineno = cur->b->c->lineno;
	}
	return 0;
}

// 在符号表中二分查找目标符号
symbol_tree *bi_search(char *name) {
    symbol_tree *res = symbol_root;
    while(res != NULL) {
        if (strcmp(name, res->name) < 0)
            res = res->left;
        else if(strcmp(name, res->name) > 0)
            res = res->right;
        else
            break;
    }
    return res;
}

// 数组声明
void array_dec(syntax_tree *p, symbol_tree *q, int dim, int mode) {
	// 记录数组每一维的大小
	q->size = (int *)malloc(dim * sizeof(int));
	int pos = dim - 1;
	// 先访问的是最后一维
	while(p->type != _ID) {
		q->size[pos] = p->b->b->val_int;
		p = p->c;
		pos--;
	}

	// 计算每一维包含的元素数: 如果要频繁访问数组, 最好一开始将每一维的元素数算出来, 避免每次访问时都再计算一遍
	// int a[3][4][5], size = {3, 4, 5}, ele_num = {4 * 5 * 1, 5 * 1, 1, 3 * 4 * 5 * 1}, 其中, ele_num的最后一个元素是整个数组的元素数
	q->ele_num = (int *)malloc((dim + 1) * sizeof(int));
	q->ele_num[dim - 1] = 1;
	for(int i = dim - 2; i > -1; i--)
		q->ele_num[i] = q->size[i + 1] * q->ele_num[i + 1];
	q->ele_num[dim] = q->size[0] * q->ele_num[0];
	
	// mode: 0表示局部变量, 1表示函数形参
	// 如果不是函数形参, 输出分配数组空间的中间代码, 并且取好首地址
	if(mode == 0) {
		sprintf(ir_buf0, "%d", 4 * q->ele_num[dim]);
		ir_insert(IR_DEC, q->ir_name, ir_buf0, NULL, NULL);
		sprintf(ir_buf0, "t%d", ir_var_id);
		sprintf(ir_buf1, "&%s", q->ir_name);
		ir_insert(IR_ASSIGN, ir_buf0, ir_buf1, NULL, NULL);
		sprintf(q->ir_name, "t%d", ir_var_id);
		ir_var_id++;
	}
}

// 在中间代码列表中追加中间代码
// 中间代码格式(根据参数数量排序):
// 0	IR_LABEL	1	{ x }				LABEL x 
// 1	IR_FUNC		1	{ f }				FUNCTION f
// 2	IR_GOTO		1	{ x }				GOTO x
// 3	IR_RET		1	{ x }				RETURN x
// 4	IR_ARG		1	{ x }				ARG x
// 5	IR_PARAM	1	{ x }				PARAM x
// 6	IR_READ		1	{ x }				READ x
// 7	IR_WRITE	1	{ x }				WRITE x
// 8	IR_ASSIGN	2	{ x, y }			x := y
// 9	IR_DEC		2	{ x, size }			DEC x size
// 10	IR_CALL		2	{ x, f }			x := CALL f
// 11	IR_ARITH	4	{ x, y, op, z }		x := y {+, -, *, /} z
// 12	IR_IF		4	{ x, relop, y, z }	IF x relop y GOTO z
void ir_insert(int type, char *arg0, char *arg1, char *arg2, char *arg3) {
	ir_list *p = (ir_list *)malloc(sizeof(ir_list));
	p->type = type;
	if(type <= IR_WRITE) {
		p->args[0] = (char *)malloc(IR_TOKEN_LEN * sizeof(char));
		strcpy(p->args[0], arg0);
	}
	else if(type <= IR_CALL) {
		p->args[0] = (char *)malloc(IR_TOKEN_LEN * sizeof(char));
		p->args[1] = (char *)malloc(IR_TOKEN_LEN * sizeof(char));
		strcpy(p->args[0], arg0);
		strcpy(p->args[1], arg1);
	}
	else {
		p->args[0] = (char *)malloc(IR_TOKEN_LEN * sizeof(char));
		p->args[1] = (char *)malloc(IR_TOKEN_LEN * sizeof(char));
		p->args[2] = (char *)malloc(IR_TOKEN_LEN * sizeof(char));
		p->args[3] = (char *)malloc(IR_TOKEN_LEN * sizeof(char));
		strcpy(p->args[0], arg0);
		strcpy(p->args[1], arg1);
		strcpy(p->args[2], arg2);
		strcpy(p->args[3], arg3);
	}
	if(ir_head == NULL)
		ir_head = p;
	else
		ir_tail->next = p;
	ir_tail = p;
}

void local_var(syntax_tree *p, int type) {
	while(p != NULL) {
		if(p->type == _Dec) {
			// 找到VarDec中ID的位置, 并确定数组变量的维数
			int dim = 0;
			syntax_tree *r = p->c->c;
			while(r->type != _ID) {
				r = r->c;
				dim++;
			}
			
			symbol_tree *s = bi_search(r->val_id);
			// 检查重名
			if(s->type != SYM_UNDEF) {
				printf("Line %d: Redefined variable \"%s\".\n", r->lineno, r->val_id);
				ERROR_RETURN_VOID
			}

			// 生成关联的中间结果名
			sprintf(ir_buf0, "t%d", ir_var_id);
			ir_var_id++;
			s->ir_name = (char *)malloc((strlen(ir_buf0) + 1) * sizeof(char));
			strcpy(s->ir_name, ir_buf0);
			
			// 因为下面的初始化赋值要用到符号表结点信息, 不得不把相关信息的写入提前到这里, 如果下面的类型检查出错, 则这里会写入无效信息
			s->type = type;
			s->dim = dim;
			if(dim > 0)
				array_dec(p->c->c, s, dim, 0);
			
			// 处理初始化赋值
			// Dec --> VarDec
			// Dec --> VarDec ASSIGNOP Exp
			if(p->c->b && p->c->b->type == _ASSIGNOP) {
				if(exp_proc(p->c->b->b->c, -1, -1))
					return;
				if(stk_top->type != type || stk_top->dim != dim) {
					printf("Line %d: Type mismatched for assignment.\n", p->c->b->lineno);
					ERROR_RETURN_VOID
				}
				match_list lhs;
				lhs.symbol_node = s;
				lhs.dim = dim;
				lhs.dim_trav = 0;
				lhs.ir_name = s->ir_name;
				assign_proc(&lhs, stk_top);
			}
			p = p->b;
		}
		else if(p->type == _DecList)
			p = p->c;
		else
			p = p->b;
	}
}

void assign_proc(match_list *lhs, match_list *rhs) {
	// 对于等号两边是等维数组的情况, 从两边的起始位置挨个复制, 直到左边数组满了
	// 例如, 现有数组int a[3][4][5], int b[2][3][4], 令a[0] = b[1], a[0]有4 * 5 = 20个空位, b[1]有3 * 4 = 12个空位
	// 等号左边的为数组
	if(lhs->dim > 0) {
		int left_addr = array_addr(lhs), right_addr = array_addr(rhs);
		// 左边空位数
		int lsize = 0;
		// 最高维
		if(lhs->dim_trav == 0)
			lsize = lhs->symbol_node->ele_num[lhs->dim] - 1;
		// 访问到中间, 例如a[3][4][5]
		//          dim_trav 1  2  3
		//       ele_num_idx 0  1  2  3
		//           ele_num 20 5  1  60
		else
			lsize = lhs->symbol_node->ele_num[lhs->dim_trav - 1] - 1;
		// 挨个赋值
		for(int i = 0; i < lsize - 1; i++) {
			sprintf(ir_buf0, "*t%d", left_addr);
			sprintf(ir_buf1, "*t%d", right_addr);
			ir_insert(IR_ASSIGN, ir_buf0, ir_buf1, NULL, NULL);
			// 向前移动一个元素
			ir_insert(IR_ARITH, ir_buf0 + 1, ir_buf0 + 1, "+", "#4");
			ir_insert(IR_ARITH, ir_buf1 + 1, ir_buf1 + 1, "+", "#4");
		}
		// 最后一次赋值
		sprintf(ir_buf0, "*t%d", left_addr);
		sprintf(ir_buf1, "*t%d", right_addr);
		ir_insert(IR_ASSIGN, ir_buf0, ir_buf1, NULL, NULL);
	}
	// 等号左边的为变量, 由于经过类型检查, 右边也保证是变量
	else
		ir_insert(IR_ASSIGN, lhs->ir_name, rhs->ir_name, NULL, NULL);
	
	// 退掉一个元素
	match_list *tmp = stk_top;
	stk_top = stk_top->next;
	if(tmp->dim > 0) {
		for(int i = 0; i < tmp->dim_trav; i++)
			free(tmp->pos[i]);
		free(tmp->pos);
	}
	free(tmp->ir_name);
	free(tmp);
	stk_size--;
}

int array_addr(match_list *stk_node) {
	// 取数组的首地址
	int start_addr = ir_var_id;
	sprintf(ir_buf0, "t%d", ir_var_id);
	ir_var_id++;
	ir_insert(IR_ASSIGN, ir_buf0, stk_node->ir_name, NULL, NULL);	
	// 计算偏移量
	for(int i = 0; i < stk_node->dim_trav; i++) {
		// ir_buf1: 当前维元素个数
		sprintf(ir_buf0, "t%d", ir_var_id);
		sprintf(ir_buf1, "#%d", stk_node->symbol_node->ele_num[i] * 4);
		ir_var_id++;
		
		// stk_node->pos[i]: 当前维位置
		// start_addr += 当前维位置 * 当前维元素个数
		ir_insert(IR_ARITH, ir_buf0, stk_node->pos[i], "*", ir_buf1);
		sprintf(ir_buf1, "t%d", start_addr);
		ir_insert(IR_ARITH, ir_buf1, ir_buf1, "+", ir_buf0);
	}
	return start_addr;
}

// 正确返回0, 错误返回1
// stk_top规定的序列可以看作一个栈, stk_top指向栈顶, 从栈顶到栈底, 存放着最近到最开始归约的exp
// 一旦出现错误, 出现错误结点的父结点全部标记为出错
// 二元运算: Exp --> Exp ASSIGNOP/AND/OR/RELOP/PLUS/MINUS/STAR/DIV Exp
// 一元运算: Exp --> MINUS/NOT Exp
// 括号: Exp --> LP Exp RP
// 函数调用(含参): Exp --> ID LP Args RP
// 函数调用(不含参): Exp --> ID LP RP
// 数组访问: Exp --> Exp LB Exp RB
// 结构体访问: Exp --> Exp DOT ID
// 基本表达式: Exp --> ID/INT/FLOAT
int exp_proc(syntax_tree *p, int label1, int label2) {
	int res = 0;
	// 非基本表达式
	if(p->b != NULL) {
		if(p->b->type == _ASSIGNOP)
			res = exp_assign(p);
		else if(p->b->type >= _RELOP && p->b->type <= _OR)
			res = exp_binary(p, label1, label2);
		else if(p->type == _LP)
			res = exp_proc(p->b->c, -1, -1);
		else if(p->type == _MINUS)
			res = exp_minus(p);
		else if(p->type == _NOT)
			res = exp_not(p, label1, label2);
		else if(p->b->type == _LP)
			res = exp_call(p);
		else if(p->b->type == _LB)
			res = exp_array(p);
	}
	else if(p->type == _ID || p->type == _INT || p->type == _FLOAT)
		res = exp_basic(p);
	if(label1 != -1) {
		sprintf(ir_buf0, "t%d", label1);
		ir_insert(IR_IF, stk_top->ir_name, "!=", "#0", ir_buf0);
		sprintf(ir_buf0, "t%d", label2);
		ir_insert(IR_GOTO, ir_buf0, NULL, NULL, NULL);
	}
	if(res)
		has_error = 1;
	return res;
}

int exp_assign(syntax_tree *p) {
	if(exp_proc(p->c, -1, -1) + exp_proc(p->b->b->c, -1, -1))
		return 1;
	match_list *lhs = stk_top->next;
	match_list *rhs = stk_top;
	// 检查等号左边的元素是否可以左值
	if(lhs->symbol_node == NULL) {
		printf("Line %d: The left-hand side of an assignment must be a variable.\n", p->b->lineno);
		return 1;
	}
	// 检查两个操作数类型是否相同, 维数是否相同
	if(rhs->type != lhs->type || rhs->dim != lhs->dim) {
		printf("Line %d: Type mismatched for assignment.\n", p->b->lineno);
		return 1;
	}
	assign_proc(lhs, rhs);
	// 除了连等之外, 赋值操作的结果不能作为表达式, 赋值操作的优先级总是最低的
	// 在连等操作中, 由于总是最右端的赋值先返回, 所以不存在等号左侧是右值的情况
	return 0;
}

int exp_binary(syntax_tree *p, int label1, int label2) {
	int res = 0;
	// 以下操作都要求算符两边无错误, 因为在错误检查完成之前就输出了部分中间代码
	// 一旦检查出了错误, 已经输出的中间代码很有可能是无效的, 甚至可能导致程序逻辑混乱
	// 关系运算
	if(p->b->type == _AND)
		res = exp_and(p, label1, label2);
	else if(p->b->type == _OR)
		res = exp_or(p, label1, label2);
	else if(p->b->type == _RELOP)
		res = exp_relop(p, label1, label2);
	// 算术运算
	else
		res = exp_proc(p->c, -1, -1) + exp_proc(p->b->b->c, -1, -1);
	if(res)
	   return 1;
	
	match_list *lhs = stk_top->next;
	match_list *rhs = stk_top;
	// 没有取到规定维数的数组都不能参与任何运算
	if(rhs->dim > 0 || lhs->dim > 0) {
		printf("Line %d: Type mismatched for operands.\n", p->b->lineno);
		return 1;
	}
	// 检查双目运算符两边表达式类型是否相同
	if(rhs->type != lhs->type) {
		printf("Line %d: Type mismatched for operands.\n", p->b->lineno);
		return 1;
	}
	// float不能参与逻辑运算
	if((rhs->type == SYM_FLOAT) && (p->b->type == _AND || p->b->type == _OR)) {
		printf("Line %d: Type mismatched for operands.\n", p->b->lineno);
		return 1;
	}
	
	// 是否采用常数折叠
	int is_cf = 0;
	// 算术运算
	if(p->b->type >= _PLUS && p->b->type <= _DIV) {
		// 获取算符类型
		char op = '+';
		switch(p->b->type) {
			case _MINUS: op = '-'; break;
			case _STAR: op = '*'; break;
			case _DIV: op = '/';
		}
		// 如果两边都是立即数
		if(lhs->ir_name[0] == '#' && rhs->ir_name[0] == '#') {
			if(lhs->type == SYM_INT) {
				int left_int = atoi(lhs->ir_name + 1);
				int right_int = atoi(rhs->ir_name + 1);
				switch(op) {
					case '+': left_int += right_int; break;
					case '-': left_int -= right_int; break;
					case '*': left_int *= right_int; break;
					case '/': left_int /= right_int;
				}
				sprintf(lhs->ir_name + 1, "%d", left_int);
			}
			else {
				double left_double = atof(lhs->ir_name + 1);
				int right_double = atof(rhs->ir_name + 1);
				switch(op) {
					case '+': left_double += right_double; break;
					case '-': left_double -= right_double; break;
					case '*': left_double *= right_double; break;
					case '/': left_double /= right_double;
				}
				sprintf(lhs->ir_name + 1, "%.6lf", left_double);
			}
			is_cf = 1;
		}
		else {
			sprintf(ir_buf0, "t%d", ir_var_id);
			ir_var_id++;
			// 算符左边和右边都只能是非数组元素
			ir_buf1[0] = op;
			ir_buf1[1] = '\0';
			ir_insert(IR_ARITH, ir_buf0, lhs->ir_name, ir_buf1, rhs->ir_name);
		}
	}
	
	// 退掉一个元素, 退掉的元素不可能是数组变量
	match_list *tmp = stk_top;
	stk_top = stk_top->next;
	free(tmp->ir_name);
	free(tmp);
	stk_size--;

	// 对于算术运算，修改新栈顶
	if(p->b->type >= _PLUS && p->b->type <= _DIV && !is_cf) {
		// 新的栈顶不可能是数组变量
		// 表达式变右值
		stk_top->symbol_node = NULL;
		// 释放分配的空间, 更新中间结果名
		free(stk_top->ir_name); 
		stk_top->ir_name = (char *)malloc((strlen(ir_buf0) + 1) * sizeof(char));
		strcpy(stk_top->ir_name, ir_buf0);
	}
	return 0;
}

int exp_and(syntax_tree *p, int label1, int label2) {
	int res = 0;
	if(label1 != -1) {
		int cur_label1 = ir_var_id;
		ir_var_id++;
		res = exp_proc(p->c, cur_label1, label2);
		sprintf(ir_buf0, "t%d", cur_label1);
		ir_insert(IR_LABEL, ir_buf0, NULL, NULL, NULL);
		res += exp_proc(p->b->b->c, label1, label2);
		label1 = -1;
	}
	else {
		int cur_name = ir_var_id, cur_label1 = ir_var_id + 1, cur_label2 = ir_var_id + 2;
		sprintf(ir_buf0, "t%d", cur_name);
		ir_insert(IR_ASSIGN, ir_buf0, "#0", NULL, NULL);
		ir_var_id += 3;
		
		int cur_label3 = ir_var_id;
		ir_var_id++;
		res = exp_proc(p->c, cur_label3, cur_label2);
		sprintf(ir_buf0, "t%d", cur_label3);
		ir_insert(IR_LABEL, ir_buf0, NULL, NULL, NULL);
		res += exp_proc(p->b->b->c, cur_label1, cur_label2);
		
		sprintf(ir_buf0, "t%d", cur_label1);
		ir_insert(IR_LABEL, ir_buf0, NULL, NULL, NULL);
		sprintf(ir_buf0, "t%d", cur_name);
		ir_insert(IR_ASSIGN, ir_buf0, "#1", NULL, NULL);
		sprintf(ir_buf0, "t%d", cur_label2);
		ir_insert(IR_LABEL, ir_buf0, NULL, NULL, NULL);
	}
	return res;
}

int exp_or(syntax_tree *p, int label1, int label2) {
	int res = 0;
	if(label1 != -1) {
		int cur_label1 = ir_var_id;
		ir_var_id++;
		res = exp_proc(p->c, label1, cur_label1);
		sprintf(ir_buf0, "t%d", cur_label1);
		ir_insert(IR_LABEL, ir_buf0, NULL, NULL, NULL);
		res += exp_proc(p->b->b->c, label1, label2);
		label1 = -1;
	}
	else {
		int cur_name = ir_var_id, cur_label1 = ir_var_id + 1, cur_label2 = ir_var_id + 2;
		sprintf(ir_buf0, "t%d", cur_name);
		ir_insert(IR_ASSIGN, ir_buf0, "#0", NULL, NULL);
		ir_var_id += 3;
		
		int cur_label3 = ir_var_id;
		ir_var_id++;
		res = exp_proc(p->c, cur_label1, cur_label3);
		sprintf(ir_buf0, "t%d", cur_label3);
		ir_insert(IR_LABEL, ir_buf0, NULL, NULL, NULL);
		res += exp_proc(p->b->b->c, cur_label1, cur_label2);
		
		sprintf(ir_buf0, "t%d", cur_label1);
		ir_insert(IR_LABEL, ir_buf0, NULL, NULL, NULL);
		sprintf(ir_buf0, "t%d", cur_name);
		ir_insert(IR_ASSIGN, ir_buf0, "#1", NULL, NULL);
		sprintf(ir_buf0, "t%d", cur_label2);
		ir_insert(IR_LABEL, ir_buf0, NULL, NULL, NULL);
	}
	return res;
}

int exp_relop(syntax_tree *p, int label1, int label2) {
	int res = 0;
	if(label1 != -1) {
		res = exp_proc(p->c, -1, -1) + exp_proc(p->b->b->c, -1, -1);
		sprintf(ir_buf0, "t%d", label1);
		ir_insert(IR_IF, stk_top->next->ir_name, p->b->val_relop, stk_top->ir_name, ir_buf0);
		sprintf(ir_buf0, "t%d", label2);
		ir_insert(IR_GOTO, ir_buf0, NULL, NULL, NULL);
		label1 = -1;
	}
	else {
		int cur_name = ir_var_id, cur_label1 = ir_var_id + 1, cur_label2 = ir_var_id + 2;
		sprintf(ir_buf0, "t%d", cur_name);
		ir_insert(IR_ASSIGN, ir_buf0, "#0", NULL, NULL);
		ir_var_id += 3;
		
		res = exp_proc(p->c, -1, -1) + exp_proc(p->b->b->c, -1, -1);
		sprintf(ir_buf0, "t%d", cur_label1);
		ir_insert(IR_IF, stk_top->next->ir_name, p->b->val_relop, stk_top->ir_name, ir_buf0);
		sprintf(ir_buf0, "t%d", cur_label2);
		ir_insert(IR_GOTO, ir_buf0, NULL, NULL, NULL);
		
		sprintf(ir_buf0, "t%d", cur_label1);
		ir_insert(IR_LABEL, ir_buf0, NULL, NULL, NULL);
		sprintf(ir_buf0, "t%d", cur_name);
		ir_insert(IR_ASSIGN, ir_buf0, "#1", NULL, NULL);
		sprintf(ir_buf0, "t%d", cur_label2);
		ir_insert(IR_LABEL, ir_buf0, NULL, NULL, NULL);
	}
	return res;
}

int exp_minus(syntax_tree *p) {
	if(exp_proc(p->b->c, -1, -1))
		return 1;
	// 没有取到规定维数的数组不能参与任何运算
	if(stk_top->dim > 0) {
		printf("Line %d: Type mismatched for operands.\n", p->lineno);
		return 1;
	}
	char *ir_name = stk_top->ir_name;
	// 对于立即数, 不需要额外的中间代码语句, 只要将栈中的变量值取负就可以了
	if(ir_name[0] == '#') {
		int len = strlen(ir_name);
		for(int i = len + 1; i > 1; i--)
			ir_name[i] = ir_name[i - 1];
		ir_name[1] = '-';
	}
	// 对于其他变量, 额外引入一条取负语句
	else {
		sprintf(ir_buf0, "t%d", ir_var_id);
		ir_insert(IR_ARITH, ir_buf0, "#0", "-", ir_name);
		strcpy(ir_name, ir_buf0);
		ir_var_id++;
	}
	// 表达式变右值
	stk_top->symbol_node = NULL;
	return 0;
}

int exp_not(syntax_tree *p, int label1, int label2) {
	if(exp_proc(p->b->c, label2, label1))
		return 1;
	// 除了int变量其他变量都不能参与逻辑运算, 没有取到规定维数的数组都不能参与任何运算
	if(stk_top->type != SYM_INT || stk_top->dim > 0) {
		printf("Line %d: Type mismatched for operands.\n", p->lineno);
		return 1;
	}
	stk_top->symbol_node = NULL;
	return 0;
}

int exp_call(syntax_tree *p) {
	symbol_tree *func_node = bi_search(p->val_id);
	if(func_node->ret_type == SYM_UNDEF) {
		printf("Line %d: Undefined function \"%s\".\n", p->lineno, p->val_id);
		return 1;
	}
	if(func_node->type != SYM_UNDEF) {
		printf("Line %d: \"%s\" is not a function.\n", p->lineno, p->val_id);
		return 1;
	}

	int arg_num = 0;
	// 检查实参与形参个数、类型是否匹配
	// 计算实参值
	if(p->b->b->type == _Args) {
		syntax_tree *q = p->b->b->c;
		int last_stk_size = stk_size;
		while(q != NULL) {
			if(q->type == _Exp) {
				if(exp_proc(q->c, -1, -1))
					return 1;
				q = q->b;
			}
			else if(q->type == _Args)
				q = q->c;
			else
				q = q->b;
		}
		arg_num = stk_size - last_stk_size;
	}

	match_list *q = stk_top;
	symbol_list *r = func_node->arg_list;
	int is_mismatched = 0;
	// 函数定义有形参此处却没有检测到实参, 或函数没有定义形参此处却检测到了实参
	if((arg_num == 0 && r != NULL) || (arg_num > 0 && r == NULL))
		is_mismatched = 1;
	else {
		int i = 0;
		for(; i < arg_num && r != NULL; i++, q = q->next, r = r->next) {
			if(q->type != r->data->type) {
				is_mismatched = 1;
				break;
			}
			else if(q->dim != r->data->dim) {
				is_mismatched = 1;
				break;
			}
		}
		if(i != arg_num || r != NULL)
			is_mismatched = 1;
	}
	if(is_mismatched)
		arg_param_mismatch(p, func_node, arg_num);
	if(strcmp(p->val_id, "read") == 0)
		exp_read(p, func_node);
	else if(strcmp(p->val_id, "write") == 0)
		exp_write(p, func_node);
	else
		exp_func(p, arg_num, func_node);
	return 0;
}

void arg_param_mismatch(syntax_tree *p, symbol_tree *func_node, int arg_num) {
	printf("Line %d: Function \"%s(", p->lineno, p->val_id);
	int i = 0;
	symbol_list *q;
	for(q = func_node->arg_list; q != NULL; q = q->next, i++);
	symbol_tree **arg_list = (symbol_tree **)malloc(i * sizeof(symbol_tree *));
	int j = 0;
	for(q = func_node->arg_list; q != NULL; q = q->next, j++)
		arg_list[j] = q->data;
	for(j = j - 1; j > -1; j--) {
		if(arg_list[j]->type == SYM_INT)
			printf("int");
		else
			printf("float");
		for(int k = 0; k < arg_list[j]->dim; k++)
			printf("*");
		if(j > 0)
			printf(", ");
	}
	free(arg_list);

	printf(")\" is not applicable for arguments\"(");
	i = 0;
	match_list **cur_arg_list = (match_list **)malloc(arg_num * sizeof(match_list *)); 
	match_list *r;
	for(r = stk_top; i < arg_num; r = r->next, i++)
		cur_arg_list[i] = r;
	for(i = i - 1; i > -1; i--) {
		if(cur_arg_list[i]->type == SYM_INT)
			printf("int");
		else
			printf("float");
		for(j = 0; j < cur_arg_list[i]->dim; j++)
			printf("*");
		if(i > 0)
			printf(", ");
	}
	printf(")\".\n");
}

void exp_read(syntax_tree *p, symbol_tree *func_node) {
	sprintf(ir_buf0, "t%d", ir_var_id);
	ir_var_id++;
	ir_insert(IR_READ, ir_buf0, NULL, NULL, NULL);
	match_list *q = (match_list *)malloc(sizeof(match_list));
	q->symbol_node = NULL;
	q->type = func_node->ret_type;
	q->dim = 0;
	q->lineno = p->lineno; 
	q->ir_name = (char *)malloc((strlen(ir_buf0) + 1) * sizeof(char));
	strcpy(q->ir_name, ir_buf0);
	q->next = stk_top;
	stk_top = q;
	stk_size++;
}

void exp_write(syntax_tree *p, symbol_tree *func_node) {
	ir_insert(IR_WRITE, stk_top->ir_name, NULL, NULL, NULL);
	stk_top->symbol_node = NULL;
	stk_top->type = func_node->ret_type;
	stk_top->dim = 0;
	stk_top->lineno = p->lineno;
	free(stk_top->ir_name);
	stk_top->ir_name = (char *)malloc(3 * sizeof(char));
	strcpy(stk_top->ir_name, "#0");
}

void exp_func(syntax_tree *p, int arg_num, symbol_tree *func_node) {
	sprintf(ir_buf0, "t%d", ir_var_id);
	ir_var_id++;
	// 准备参数, 退出栈顶元素
	for(int i = 0; i < arg_num; i++) {
		ir_insert(IR_ARG, stk_top->ir_name, NULL, NULL, NULL);
		match_list *q = stk_top;
		stk_top = stk_top->next;
		if(q->dim > 0) {
			for(int j = 0; j < q->dim_trav; j++)
				free(q->pos[j]);
			free(q->pos);
		}
		free(q->ir_name);
		free(q);
		stk_size--;
	}
	ir_insert(IR_CALL, ir_buf0, func_node->ir_name, NULL, NULL);
	match_list *q = (match_list *)malloc(sizeof(match_list));
	q->symbol_node = NULL;
	q->type = func_node->ret_type;
	q->dim = 0;
	q->lineno = p->lineno; 
	q->ir_name = (char *)malloc((strlen(ir_buf0) + 1) * sizeof(char));
	strcpy(q->ir_name, ir_buf0);
	q->next = stk_top;
	stk_top = q;
	stk_size++;
}

int exp_array(syntax_tree *p) {
	if(exp_proc(p->c, -1, -1))
		return 1;
	if(stk_top->symbol_node == NULL) {
	    printf("Line %d: Element before the left bracket is not an array.\n", stk_top->lineno);
	    return 1;
	}
	if(stk_top->dim == 0) {
	    printf("Line %d: \"%s\" is not an array.\n", stk_top->lineno, stk_top->symbol_node->name);
	    return 1;
	}
	if(exp_proc(p->b->b->c, -1, -1))
		return 1;
	if(stk_top->type != SYM_INT) {
		printf("Line %d: Element between the two brackets is not an integer.\n", stk_top->lineno);
		return 1;
	}

	int dim_trav = stk_top->next->dim_trav;
	char **pos = stk_top->next->pos;
	
	char *ir_name = stk_top->ir_name;
	pos[dim_trav] = (char *)malloc((strlen(ir_name) + 1) * sizeof(char));
	strcpy(pos[dim_trav], ir_name);
	stk_top->next->dim_trav++;

	// 中括号内容退栈
	match_list *tmp = stk_top;
	stk_top = stk_top->next;
	free(tmp->ir_name);
	free(tmp);
	stk_size--;

	// 现在栈顶为数组变量
	stk_top->dim--;
	// 如果已经取到规定的维数, 立刻寻址, 退化为非数组变量
	if(stk_top->dim == 0) {
		// 取数组的首地址
		int start = array_addr(stk_top);
		for(int i = 0; i < stk_top->dim_trav; i++)
			free(stk_top->pos[i]);
		free(stk_top->pos);
		sprintf(ir_buf0, "t%d", ir_var_id);
		ir_var_id++;
		sprintf(ir_buf1, "t%d", start);
		ir_insert(IR_ASSIGN, ir_buf0, ir_buf1, NULL, NULL);
		sprintf(stk_top->ir_name, "*%s", ir_buf0);
	}
	return 0;
}

int exp_basic(syntax_tree *p) {
	match_list *q = (match_list *)malloc(sizeof(match_list));
	if(p->type == _ID) {
		symbol_tree *res = bi_search(p->val_id);
		if(res->type == SYM_UNDEF) {
			printf("Line %d: Undefined variable \"%s\".\n", p->lineno, p->val_id);
			return 1;
		}
		q->symbol_node = res;
		q->type = res->type;
		q->dim = res->dim;
		// 为了计数的方便, 增加了成员dim_trav，其值永远等于symbol_node->dim - dim
		q->dim_trav = 0;
		q->lineno = p->lineno;
		// 记录每一维的位置值, 如果后面处理中括号时发生错误, 分配的内存会被丢弃, 造成内存泄漏
		if(q->dim > 0)
			q->pos = (char **)malloc(res->dim * sizeof(char *));
		// 初始化为与ID关联的中间结果名 
		q->ir_name = (char *)malloc((strlen(res->ir_name) + 1) * sizeof(char));
		strcpy(q->ir_name, res->ir_name);
	}
	else {
		q->symbol_node = NULL;
		q->dim = 0;
		q->dim_trav = 0;
		q->lineno = p->lineno;

		if(p->type == _INT) {
			q->type = SYM_INT;
			sprintf(ir_buf0, "#%d", p->val_int);
		}
		else {
			q->type = SYM_FLOAT;
			sprintf(ir_buf0, "#%.6lf", p->val_double);
		}
		q->ir_name = (char *)malloc((strlen(ir_buf0) + 1) * sizeof(char));
		strcpy(q->ir_name, ir_buf0);
	}
	
   	q->next = stk_top;
    stk_top = q;
    stk_size++;
	return 0;
}

void undef_func(symbol_tree *cur) {
	if(cur == NULL)
		return;
	if(cur->ret_type != SYM_UNDEF && cur->def_dec_status == 0) {
		printf("Line %d: Undefined function \"%s\".\n", cur->lineno, cur->name);
		has_error = 1;
	}
	undef_func(cur->left);
	undef_func(cur->right);
}

void label_cut() {
	ir_list *p = ir_head;
	
	// 抽取所有GOTO项, 并清理掉紧跟在RETURN后面的GOTO
	int goto_cnt = 0;
	for(; p != NULL; p = p->next) {
		if(p->type == IR_GOTO)
			goto_cnt++;
		else if(p->type == IR_RET && p->next && p->next->type == IR_GOTO) {
			ir_list *q = p->next;
			p->next = q->next;
			free(q->args[0]);
			free(q);
		}
	}
	
	ir_list **goto_list = (ir_list **)malloc(goto_cnt * sizeof(ir_list *));
	goto_cnt = 0;
	for(p = ir_head; p != NULL; p = p->next) {
		if(p->type == IR_GOTO) {
			goto_list[goto_cnt] = p;
			goto_cnt++;
		}
	}

	for(p = ir_head; p != NULL; p = p->next) {
		if(p->type == IR_LABEL && p->next && p->next->type == IR_LABEL) {
			// 这里假定要删除的标签永远不可能出现在链表的开头
			ir_list *q = p->next;
			strcpy(ir_buf0, q->args[0]);
			p->next = q->next;
			free(q->args[0]);
			free(q);
			for(int i = 0; i < goto_cnt; i++) {
				if(strcmp(goto_list[i]->args[0], ir_buf0) == 0)
					strcpy(goto_list[i]->args[0], p->args[0]);
			}
		}
	}
	free(goto_list);
}
