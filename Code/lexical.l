%{
	#include"syntax.tab.h"
	#include"semantic.h"
	
	typedef struct worklist {
		struct syntax_tree *data;
		struct worklist *next;
	}worklist;

	extern int has_error;
	extern worklist* wl_head;
	extern char token_names[48][16];
	void proc(int, int);
	extern symbol_tree *avl_insert(char *);
%}

%option yylineno
num 0|([1-9][0-9]*)

%%


{num} {
	// 十进制数
	// 十进制数除了单独的0以外, 不能以0开头, 否则识别为八进制数
	// 语法分析器出现错误, 词法分析不必完整做下去, 不必进入工作集, 只返回类型即可
	if(!has_error)
		proc(_INT, 0);
	return INT;
}

0[xX][0-9a-fA-F]+ {
	// 十六进制数
	if(!has_error)
		proc(_INT, 1);
	return INT;
}

0[0-7]+ {
	// 八进制数
	if(!has_error)
		proc(_INT, 2);
	return INT;
}

(\.[0-9]+)|({num}\.[0-9]*) {
	// 浮点小数
	if(!has_error)
		proc(_FLOAT, 0);
	return FLOAT;
}

; {
	if(!has_error)
		proc(_SEMI, 0);
	return SEMI;
}

, {
	if(!has_error)
		proc(_COMMA, 0);
	return COMMA;
}

= {
	if(!has_error)
		proc(_ASSIGNOP, 0);
	return ASSIGNOP;
}

>|<|>=|<=|==|!= {
	if(!has_error)
		proc(_RELOP, 0);
	return RELOP;
}

"+" {
	if(!has_error)
		proc(_PLUS, 0);
	return PLUS;
}

"-" {
	if(!has_error)
		proc(_MINUS, 0);
	return MINUS;
}

"*" {
	if(!has_error)
		proc(_STAR, 0);
	return STAR;
}

"/" {
	if(!has_error)
		proc(_DIV, 0);
	return DIV;
}

&& {
	if(!has_error)
		proc(_AND, 0);
	return AND;
}

"||" {
	if(!has_error)
		proc(_OR, 0);
	return OR;
}

"." {
	if(!has_error)
		proc(_DOT, 0);
	return DOT;
}

! {
	if(!has_error)
		proc(_NOT, 0);
	return NOT;
}

int|float {
	if(!has_error)
		proc(_TYPE, 0);
	return TYPE;
}

"(" {
	if(!has_error)
		proc(_LP, 0);
	return LP;
}

")" {
	if(!has_error)
		proc(_RP, 0);
	return RP;
}

"[" {
	if(!has_error)
		proc(_LB, 0);
	return LB;
}

"]" {
	if(!has_error)
		proc(_RB, 0);
	return RB;
}

"{" {
	if(!has_error)
		proc(_LC, 0);
	return LC;
}

"}" {
	if(!has_error)
		proc(_RC, 0);
	return RC;
}

struct {
	if(!has_error)
		proc(_STRUCT, 0);
	return STRUCT;
}

return {
	if(!has_error)
		proc(_RETURN, 0);
	return RETURN;
}

if {
	if(!has_error)
		proc(_IF, 0);
	return IF;
}

else {
	if(!has_error)
		proc(_ELSE, 0);
	return ELSE;
}

while {
	if(!has_error)
		proc(_WHILE, 0);
	return WHILE;
}

[a-zA-Z_][0-9a-zA-Z_]* {
	if(!has_error)
		proc(_ID, 0);
	return ID;
}


" "|\t|\n|\r {}


. {
	has_error = 1;
	printf("Error type A at Line %d, Mysterious characters \'%s\'\n", yylineno, yytext);
}
%%

void proc(int id, int type) {
	//printf("id: %s, type: %d\n", token_names[id], type);
	// 填充语法树结点
	syntax_tree *p = (syntax_tree *)malloc(sizeof(syntax_tree));
	p->type = id;
	p->lineno = yylineno;
	// 几种特殊情况处理一下
	if(id == _INT) {
		if(type == 0)
			sscanf(yytext, "%d", &(p->val_int));
		else if(type == 1)
			sscanf(yytext, "%x", &(p->val_int));
		else
			sscanf(yytext, "%o", &(p->val_int));
	}
	else if(id == _FLOAT)
		sscanf(yytext, "%lf", &(p->val_double));
	else if(id == _ID) {
		p->val_id = (char *)malloc((strlen(yytext) + 1) * sizeof(char));
		strcpy(p->val_id, yytext);
		avl_insert(p->val_id);
	}
	else if(id == _RELOP)
		strcpy(p->val_relop, yytext);
	else if(id == _TYPE) {
		if(strcmp(yytext, "int") == 0)
			p->val_type = SYM_INT;
		else
			p->val_type = SYM_FLOAT;	
	}
	// 开始时, 创建孤立结点, 在语法分析中, 将结点连缀起来
	p->c = NULL;
	p->b = NULL;
	// 孤立结点进入工作表, 等待语法分析取出结点
	// 头部插入法, 最后插入的在最前面
	worklist *q = (worklist *)malloc(sizeof(worklist));
	q->data = p;
	q->next = wl_head;
	wl_head = q;
}
