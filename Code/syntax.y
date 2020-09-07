%{
	#include"lex.yy.c"

	int has_error = 0;
	worklist *wl_head = NULL;
	FILE *fp_dc = NULL;

	char token_names[48][16] = { "INT", "FLOAT", "ID", "SEMI", "COMMA", "ASSIGNOP", "RELOP", "PLUS", "MINUS", "STAR",
	"DIV", "AND", "OR", "DOT", "NOT", "TYPE", "LP", "RP", "LB", "RB",
	"LC", "RC", "STRUCT", "RETURN", "IF", "ELSE", "WHILE", "Program", "ExtDefList", "ExtDef",
	"ExtDecList", "Specifier", "StructSpecifier", "OptTag", "Tag", "VarDec", "FunDec", "VarList", "ParamDec", "CompSt",
	"StmtList", "Stmt", "DefList", "Def", "DecList", "Dec", "Exp", "Args" };
	
	// 用于记录if和条件语句的信息, 然后匹配, 修正
	typedef struct repair {
		struct syntax_tree *data;
		int lineno;
		struct repair *next;
	}repair;
	
	void yyerror(const char *);
	void production0(int);
	void production1(int, int);
	void production2(int, int, int);
	void production3(int, int, int, int);
	void production4(int, int, int, int, int);
	void production5(int, int, int, int, int, int);
	void production7(int, int, int, int, int, int, int, int);
	void insert(int *, int, int);
	void repair_if(syntax_tree *);
	void syntax_tree_trav(syntax_tree *, int);
	void ir_proc();
	
	extern symbol_tree *avl_insert(char *);
	extern symbol_tree *bi_search(char *);
	extern void syntax_tree_to_ir(syntax_tree *);
	extern void undef_func(symbol_tree *);
	extern void label_cut();
	extern void ir_to_dc();

	extern symbol_tree *symbol_root;
	extern ir_list *ir_head;
	extern char ir_fname[128];
	extern char dc_fname[128];
%}

%define parse.error verbose

%token INT FLOAT ID
%token SEMI COMMA
%token ASSIGNOP RELOP
%token PLUS MINUS STAR DIV
%token AND OR DOT NOT
%token TYPE
%token LP RP LB RB LC RC
%token STRUCT
%token RETURN
%token IF ELSE WHILE

%nonassoc LOWER
%nonassoc ELSE
%nonassoc error
%right ASSIGNOP
%left OR
%left AND
%left RELOP
%left PLUS MINUS
%left STAR DIV
%left NOT
%nonassoc HIGHER
%left DOT
%left LB RB
%left LP RP

%%
Program:			ExtDefList {
									// 把产生式右侧符号集齐, 组装成产生式左边的符号, 即更高一级的概念
									production1(_ExtDefList, _Program);
									// 所有子结构都递归处理完毕, 语法分析的出口
									if(!has_error) {
										repair_if(wl_head->data);
										//syntax_tree_trav(wl_head->data, 0);
										ir_proc();
									}
								};

ExtDefList:			ExtDef ExtDefList { production2(_ExtDef, _ExtDefList, _ExtDefList); }
					| { production0(_ExtDefList); };

ExtDef: 			Specifier ExtDecList SEMI { /* 全局变量定义 */ production3(_Specifier, _ExtDecList, _SEMI, _ExtDef); }
					| Specifier SEMI { /* 结构体定义 */ production2(_Specifier, _SEMI, _ExtDef); }
					| Specifier FunDec SEMI { /* 函数声明 */ production3(_Specifier, _FunDec, _SEMI, _ExtDef); }
					| Specifier FunDec CompSt { /* 函数定义 */ production3(_Specifier, _FunDec, _CompSt, _ExtDef); };

ExtDecList:			VarDec { production1(_VarDec, _ExtDecList); }
					| VarDec COMMA ExtDecList { production3(_VarDec, _COMMA, _ExtDecList, _ExtDecList); };

Specifier:			TYPE { production1(_TYPE, _Specifier); }
					| StructSpecifier { production1(_StructSpecifier, _Specifier); };

StructSpecifier:	STRUCT OptTag LC DefList RC { /* 结构体定义 */ production5(_STRUCT, _OptTag, _LC, _DefList, _RC, _StructSpecifier); }
					| STRUCT Tag { production2(_STRUCT, _Tag, _StructSpecifier); };

OptTag:				ID { production1(_ID, _OptTag); }
					| { production0(_OptTag); };

Tag:				ID { production1(_ID, _Tag); };

VarDec:				ID { production1(_ID, _VarDec); }
					| VarDec LB INT RB { production4(_VarDec, _LB, _INT, _RB, _VarDec); };

FunDec:				ID LP VarList RP { production4(_ID, _LP, _VarList, _RP, _FunDec); }
					| ID LP RP { production3(_ID, _LP, _RP, _FunDec); }
					| error RP;

VarList:			ParamDec COMMA VarList { production3(_ParamDec, _COMMA, _VarList, _VarList); }
					| ParamDec { production1(_ParamDec, _VarList); };

ParamDec:			Specifier VarDec { production2(_Specifier, _VarDec, _ParamDec); };

CompSt:				LC DefList StmtList RC { production4(_LC, _DefList, _StmtList, _RC, _CompSt); };

StmtList:			Stmt StmtList { production2(_Stmt, _StmtList, _StmtList); }
					| { production0(_StmtList); };

Stmt:				Exp SEMI { production2(_Exp, _SEMI, _Stmt); }
					| CompSt { production1(_CompSt, _Stmt); }
					| RETURN Exp SEMI { production3(_RETURN, _Exp, _SEMI, _Stmt); }
					| IF LP Exp RP Stmt %prec LOWER { production5(_IF, _LP, _Exp, _RP, _Stmt, _Stmt); }
					| IF LP Exp RP Stmt ELSE Stmt { production7(_IF, _LP, _Exp, _RP, _Stmt, _ELSE, _Stmt, _Stmt); }
					| WHILE LP Exp RP Stmt { production5(_WHILE, _LP, _Exp, _RP, _Stmt, _Stmt); }
					| IF LP error RP Stmt %prec LOWER
					| IF LP error RP Stmt ELSE Stmt
					| WHILE LP error RP Stmt
					| error SEMI;

DefList:			Def DefList { production2(_Def, _DefList, _DefList); }
					| %prec LOWER { production0(_DefList); };

Def:				Specifier DecList SEMI { /* 局部变量定义 */ production3(_Specifier, _DecList, _SEMI, _Def); }
					| error SEMI;

DecList:			Dec { production1(_Dec, _DecList); }
					| Dec COMMA DecList { production3(_Dec, _COMMA, _DecList, _DecList); };

Dec:				VarDec { production1(_VarDec, _Dec); }
					| VarDec ASSIGNOP Exp { production3(_VarDec, _ASSIGNOP, _Exp, _Dec); };

Exp:				Exp ASSIGNOP Exp { production3(_Exp, _ASSIGNOP, _Exp, _Exp); }
					| Exp AND Exp { production3(_Exp, _AND, _Exp, _Exp); }
					| Exp OR Exp { production3(_Exp, _OR, _Exp, _Exp); }
					| Exp RELOP Exp { production3(_Exp, _RELOP, _Exp, _Exp); }
					| Exp PLUS Exp { production3(_Exp, _PLUS, _Exp, _Exp); }
					| Exp MINUS Exp { production3(_Exp, _MINUS, _Exp, _Exp); }
					| Exp STAR Exp { production3(_Exp, _STAR, _Exp, _Exp); }
					| Exp DIV Exp { production3(_Exp, _DIV, _Exp, _Exp); }
					| LP Exp RP { production3(_LP, _Exp, _RP, _Exp); }
					| MINUS Exp %prec HIGHER { production2(_MINUS, _Exp, _Exp); }
					| NOT Exp { production2(_NOT, _Exp, _Exp); }
					| ID LP Args RP { production4(_ID, _LP, _Args, _RP, _Exp); }
					| ID LP RP { production3(_ID, _LP, _RP, _Exp); }
					| Exp LB Exp RB { production4(_Exp, _LB, _Exp, _RB, _Exp); }
					| Exp DOT ID { production3(_Exp, _DOT, _ID, _Exp); } 
					| ID { production1(_ID, _Exp); }
					| INT { production1(_INT, _Exp); }
					| FLOAT { production1(_FLOAT, _Exp); };

Args:				Exp COMMA Args { production3(_Exp, _COMMA, _Args, _Args); }
					| Exp { production1(_Exp, _Args); };
%%

// 错误处理
void yyerror(const char *msg) {
	fprintf(stderr, "Error type B at Line %d: %s\n", yylineno, msg);
	has_error = 1;
}

// production0 ... production7, 将符号的type填入列表, 越靠后出现的组件越先处理, 填在列表前面的位置
void production0(int cur_type) {
	insert(NULL, 0, cur_type);
}

void production1(int type1, int cur_type) {
	int *type_list = (int *)malloc(sizeof(int));
	type_list[0] = type1;
	insert(type_list, 1, cur_type);
	free(type_list);
}

void production2(int type1, int type2, int cur_type) {
	int *type_list = (int *)malloc(2 * sizeof(int));
	type_list[0] = type2;
	type_list[1] = type1;
	insert(type_list, 2, cur_type);
	free(type_list);
}

void production3(int type1, int type2, int type3, int cur_type) {
	int *type_list = (int *)malloc(3 * sizeof(int));
	type_list[0] = type3;
	type_list[1] = type2;
	type_list[2] = type1;
	insert(type_list, 3, cur_type);
	free(type_list);
}

void production4(int type1, int type2, int type3, int type4, int cur_type) {
	int *type_list = (int *)malloc(4 * sizeof(int));
	type_list[0] = type4;
	type_list[1] = type3;
	type_list[2] = type2;
	type_list[3] = type1;
	insert(type_list, 4, cur_type);
	free(type_list);
}

void production5(int type1, int type2, int type3, int type4, int type5, int cur_type) {
	int *type_list = (int *)malloc(5 * sizeof(int));
	type_list[0] = type5;
	type_list[1] = type4;
	type_list[2] = type3;
	type_list[3] = type2;
	type_list[4] = type1;
	insert(type_list, 5, cur_type);
	free(type_list);
}

void production7(int type1, int type2, int type3, int type4, int type5, int type6, int type7, int cur_type) {
	int *type_list = (int *)malloc(7 * sizeof(int));
	type_list[0] = type7;
	type_list[1] = type6;
	type_list[2] = type5;
	type_list[3] = type4;
	type_list[4] = type3;
	type_list[5] = type2;
	type_list[6] = type1;
	insert(type_list, 7, cur_type);
	free(type_list);
}

void insert(int *type_list, int list_len, int cur_type) {
	if(has_error)
		return;
	// 非终止符的语法树结点
	syntax_tree *p = (syntax_tree *)malloc(sizeof(syntax_tree));
	p->type = cur_type;
	p->c = NULL;
	p->b = NULL;
	p->lineno = -1;
	
	/*
	for(worklist *r = wl_head; r != NULL; r = r->next)
		printf("%s ", token_names[r->data->type]);
	printf("\n");
	*/

	// 指向新建结点应该插入的位置, 新建结点成为pos->next
	worklist *pos = NULL;
	// 寻找产生式右侧的符号
	for(int i = 0; i < list_len; i++) {
		// 考虑首结点与所需符号匹配
		worklist *q = wl_head;
		// "匹配"指的是: 符号类型相同, 且满足下列3个条件中的任意一个:
		// 1.该符号是非终结符(存在子结构)
		// 2.该符号是列表中最后一个符号
		// 3.该符号不是列表中的最后一个符号, 且该符号的下一个符号与所需符号类型不同
		if(q->data->type == type_list[i] && (type_list[i] >= _Program || !q->next || q->next->data->type != type_list[i])) {
			p->lineno = q->data->lineno;
			// q指向的符号成为新建结点的下一个子结点
			q->data->b = p->c;
			p->c = q->data;
		
			// 设置新首结点	
			wl_head = q->next;
			// 删除原首结点			
			free(q);
			// 新建结点应在列表最前面插入
			pos = NULL;
		}
		// 首结点与所需符号不匹配, 在后面的结点中寻找匹配结点
		else {
			// 为了方便结点删除, r指向所需结点的前一个结点
			for(worklist *r = wl_head; r->next != NULL; r = r->next) {
				q = r->next;
				// 依照同样的"匹配"规则
				if(q->data->type == type_list[i] && (type_list[i] >= _Program || !q->next || q->next->data->type != type_list[i])) {
					p->lineno = q->data->lineno;
					q->data->b = p->c;
					p->c = q->data;
					
					r->next = q->next;
					free(q);
					// 新建结点应在r后面插入
					pos = r;
					break;
				}
			}		
		}
	}
	
	
	worklist *q = (worklist *)malloc(sizeof(worklist));
	q->data = p;
	// 在开头插入
	if(pos == NULL) {
		q->next = wl_head;
		wl_head = q;
	}
	// 在中间插入
	else {
		q->next = pos->next;
		pos->next = q;
	}
}

// 移花接木大法, 修正【在同一个StmtList中】错配的if和条件语句
// 搜索组件可能会出错, 因为Stmt返回时, 它的组件有可能还没出现在链表(栈)中, 这是bison的锅
// 所以必须在全部组装结束后, 再从全局的视角进行修正
void repair_if(syntax_tree *cur) {
	if(cur == NULL)
		return;
	// StmtList --> Stmt StmtList
	// Stmt --> IF LP Exp RP Stmt
	// Stmt --> IF LP Exp RP Stmt ELSE Stmt
	if(cur->type == _StmtList) {
		syntax_tree *p = cur->c;
		repair *if_head = NULL;
		repair *lp_head = NULL;
		while(p != NULL) {
			// 不与if在同一行的左括号是非法的
			if(p->c->type == _IF && p->c->lineno != p->c->b->lineno) {
				// if入栈
				repair *q = (repair *)malloc(sizeof(repair));
				q->data = p->c;
				q->lineno = p->c->lineno;
				q->next = if_head;
				if_head = q;
				// 左括号入栈
				q = (repair *)malloc(sizeof(repair));
				q->data = p->c->b;
				q->lineno = p->c->b->lineno;
				q->next = lp_head;
				lp_head = q;
			}
			// StmtList --> Stmt StmtList(Stmt StmtList(Stmt ...))
			// p每次移向下一个Stmt
			p = p->b->c;
		}
		
		// 假设是一一匹配的，只不过打乱了顺序，需要重新建立匹配关系
		// 匹配一个删除一个
		while(if_head) {
			repair *q = if_head, *r = lp_head;
			// 考虑q与lp列表的开头匹配
			if(q->lineno == r->lineno) {
				// 移花接木
				q->data->b = r->data;
				// 设置新首结点
				lp_head = r->next;
				// 删除原首结点
				free(r);
			}
			// q不与lp列表的开头匹配, 在后面的结点中寻找匹配结点
			else {
				for(repair *s = lp_head; s->next != NULL; s = s->next) {
					r = s->next;
					if(q->lineno == r->lineno) {
						q->data->b = r->data;
						s->next = r->next;
						free(r);
						break;
					}
				}
			}
			// 设置if列表的新首结点
			if_head = q->next;
			// 删除if列表的原首结点
			free(q);
		}
		// 删除两个链表
		while(if_head) {
			repair *q = if_head;
			if_head = if_head->next;
			free(q);
		}
		while(lp_head) {
			repair *q = lp_head;
			lp_head = lp_head->next;
			free(q);
		}
	}
	// 递归遍历, 寻找下一处StmtList
	repair_if(cur->c);
	repair_if(cur->b);
}

// 遍历语法树, 打印语法结构信息
void syntax_tree_trav(syntax_tree *cur, int tabs) {
	if(cur == NULL)
		return;
	if(cur->lineno == -1) {
		syntax_tree_trav(cur->b, tabs);
		return;
	}
	if(cur->type >= _Program) {
		for(int i = 0; i < tabs; i++)
			printf("  ");
		printf("%s (%d)\n", token_names[cur->type], cur->lineno);
		syntax_tree_trav(cur->c, tabs + 1);	
		syntax_tree_trav(cur->b, tabs);
	}
	else {
		for(int i = 0; i < tabs; i++)
			printf("  ");
		printf("%s", token_names[cur->type]);
		if(cur->type == _INT)
			printf(": %d\n", cur->val_int);
		else if(cur->type == _FLOAT)
			printf(": %.6lf\n", cur->val_double);
		else if(cur->type == _TYPE) {
			if(cur->val_type == SYM_INT)
				printf(": int\n");
			else
				printf(": float\n");
		}
		else if(cur->type == _ID)
			printf(": %s\n", cur->val_id);
		else
			printf("\n");
		syntax_tree_trav(cur->b, tabs);
	}
}

void ir_proc() {
	// read函数没有任何参数, 返回值为整型, 即读入的整数值	
	avl_insert("read");
	symbol_tree *p = bi_search("read");
	p->ret_type = SYM_INT;
	p->def_dec_status = 1;

	// 这是一个通配符, 用于指明write函数所需的参数类型
	p = avl_insert("*");
	p->type = SYM_INT;

	// write函数包含一个整数类型的参数, 即要输出的整数值, 返回值也为整型, 固定返回0
	avl_insert("write");
	symbol_tree *q = bi_search("write");
	q->ret_type = SYM_INT;
	q->def_dec_status = 1;
	
	q->arg_list = (symbol_list *)malloc(sizeof(symbol_list));
	q->arg_list->data = p;
	q->arg_list->next = NULL;
	
	syntax_tree_to_ir(wl_head->data);
	undef_func(symbol_root);
	
	if(!has_error) {
		label_cut();
		FILE *fp_ir = fopen(ir_fname, "w");
		for(ir_list *r = ir_head; r != NULL; r = r->next) {
			switch(r->type) {
				case IR_LABEL: fprintf(fp_ir, "LABEL %s :\n", r->args[0]); break;
				case IR_FUNC: fprintf(fp_ir, "FUNCTION %s :\n", r->args[0]); break;
				case IR_GOTO: fprintf(fp_ir, "GOTO %s\n", r->args[0]); break;
				case IR_RET: fprintf(fp_ir, "RETURN %s\n", r->args[0]); break;
				case IR_ARG: fprintf(fp_ir, "ARG %s\n", r->args[0]); break;
				case IR_PARAM: fprintf(fp_ir, "PARAM %s\n", r->args[0]); break;
				case IR_READ: fprintf(fp_ir, "READ %s\n", r->args[0]); break;
				case IR_WRITE: fprintf(fp_ir, "WRITE %s\n", r->args[0]); break;
				case IR_ASSIGN: fprintf(fp_ir, "%s := %s\n", r->args[0], r->args[1]); break;
				case IR_DEC: fprintf(fp_ir, "DEC %s %s\n", r->args[0], r->args[1]); break;
				case IR_CALL: fprintf(fp_ir, "%s := CALL %s\n", r->args[0], r->args[1]); break;
				case IR_ARITH: fprintf(fp_ir, "%s := %s %s %s\n", r->args[0], r->args[1], r->args[2], r->args[3]); break;
				case IR_IF: fprintf(fp_ir, "IF %s %s %s GOTO %s\n", r->args[0], r->args[1], r->args[2], r->args[3]);
			}
		}
		fclose(fp_ir);

		fp_dc = fopen(dc_fname, "w");
		ir_to_dc();
		fclose(fp_dc);
	}
}
