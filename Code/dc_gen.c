#include"semantic.h"
// 如果当前语句不是最后一条语句, 且当前语句的下一条语句是标号, 实参准备, 或函数调用语句, 需要将寄存器值写入内存
#define TEST_AND_SAVE \
	if(ir_idx != ir_cnt - 1 && (ir_arr[ir_idx + 1]->type == IR_LABEL || ir_arr[ir_idx + 1]->type == IR_ARG || ir_arr[ir_idx + 1]->type == IR_CALL))\
		reg_save(4, 26);

extern FILE *fp_dc;
extern ir_list *ir_head;
extern int ir_var_id;

int *ir_dc_map = NULL;
int *dc_ir_map = NULL;
int ir_cnt = 0;
int var_cnt = 0;
array_info *array_list = NULL;
func_info *func_list = NULL;
ir_list **ir_arr = NULL;
int *reg_var_map = NULL;
int *var_reg_map = NULL;
int *need_update = NULL;

// 下一个被替换的寄存器
int reg_to_rep = 5;
// 当前语句已占用的寄存器
int reg_used1 = -1, reg_used2 = -1; 

// 当前函数的栈中起始局部变量的编号
int start_idx = 0;

void reg_replace(int, int);
void reg_alloc(int, int);
int var_to_reg(int, int, int, int);
void reg_save(int, int);
void global_prepare();
void dc_assign(int);
int lhs_proc(int, int *, int *);
void dc_arith(int);
void cond_jump(int);
void dc_if(int);
int stk_prepare(char *);
void dc_call(int);
void dc_func(int);
void dc_ret(int);
void dc_read(int);
void dc_write(int);

void ir_to_dc() {
	global_prepare();
	for(int i = 0; i < ir_cnt; i++) {
		switch(ir_arr[i]->type) {
			case IR_LABEL:	fprintf(fp_dc, "%s:\n", ir_arr[i]->args[0]); break;
			case IR_ASSIGN:	dc_assign(i); break;
			case IR_ARITH:	dc_arith(i); break;
			
			case IR_GOTO:	reg_save(4, 26);
							fprintf(fp_dc, "j %s\n", ir_arr[i]->args[0]);
							break;
			
			case IR_IF:		dc_if(i); break;
			
			case IR_DEC:	i++;
							if(i != ir_cnt - 1 && ir_arr[i + 1]->type == IR_LABEL)
								reg_save(4, 26);
							break;
			
			case IR_CALL:	dc_call(i); break;
			case IR_FUNC:	dc_func(i); break;
			case IR_PARAM:	need_update[ir_dc_map[atoi(ir_arr[i]->args[0] + 1)]] = 0; break;
			case IR_RET:	dc_ret(i); break;
			case IR_READ:	dc_read(i); break;
			case IR_WRITE:	dc_write(i);
		}
	}
}

void reg_replace(int reg_idx, int var_idx) {
	int last_var = reg_var_map[reg_idx];
	// 如果被替换的变量的内存需要更新
	if(need_update[last_var]){
		// 找到变量在当前函数的栈中的偏移量: 4 * (start_idx - last_var), 由于栈是向下生长的, 所以这是一个负数
		fprintf(fp_dc, "sw $%d, %d($fp)\n", reg_idx, 4 * (start_idx - last_var));
		need_update[last_var] = 0;
	}
	reg_var_map[reg_idx] = var_idx;
	var_reg_map[var_idx] = reg_idx;
}

void reg_alloc(int var_idx, int mode) {
	// 选择一个寄存器, 5号到25号是用户可以用的寄存器
	int reg_idx = 5;
	for(; reg_idx < 26 && reg_var_map[reg_idx] != -1; reg_idx++);
	// 寄存器全被占用
	if(reg_idx == 26) {
		int old_rep = reg_to_rep;
		// 只要没被当前语句占用, 就可以替换
		// 先从old_rep开始往后找, 找不到再从5号向前找, 因为当前语句最多占用两个寄存器, 所以一定能找到可替换的寄存器
		for(; (reg_to_rep == reg_used1 || reg_to_rep == reg_used2) && reg_to_rep < 26; reg_to_rep++);
		if(reg_to_rep == 26) {
			for(reg_to_rep = 5; (reg_to_rep == reg_used1 || reg_to_rep == reg_used2) && reg_to_rep < old_rep; reg_to_rep++);
		}
		reg_replace(reg_to_rep, var_idx);
		// 刚刚替换的不宜再替换, 所以加一
		reg_to_rep++;
		if(reg_to_rep == 26)
			reg_to_rep = 5;
	}
	else {
		reg_var_map[reg_idx] = var_idx;
		var_reg_map[var_idx] = reg_idx;
	}
	// 不需要在内存中取值mode为0, 需要mode为1
	// lhs一般不需要, rhs一般需要
	if(mode == 1)
		fprintf(fp_dc, "lw $%d, %d($fp)\n", var_reg_map[var_idx], 4 * (start_idx - var_idx));
}

// 对于给定的变量, 找到寄存器编号, 如果还没分配, 就分配一个
int var_to_reg(int ir_idx, int arg_idx, int offset, int mode) {
	// 取出ir变量号
	int var_idx = atoi(ir_arr[ir_idx]->args[arg_idx] + offset);
	// 转化为dc变量号
	var_idx = ir_dc_map[var_idx];
	// 如果未分配
	if(var_reg_map[var_idx] == -1)
		reg_alloc(var_idx, mode);
	return var_idx;
}

void reg_save(int start, int end) {
	for(int i = start; i < end; i++) {
		int last_var = reg_var_map[i];
		// 如果寄存器没有保存变量, 就跳过
		if(last_var == -1)
			continue;
		reg_var_map[i] = -1;
		var_reg_map[last_var] = -1;
		if(need_update[last_var]) {
			fprintf(fp_dc, "sw $%d, %d($fp)\n", i, 4 * (start_idx - last_var));
			need_update[last_var] = 0;
		}
	}
}

void global_prepare() {
	// t后面的数字 --> 位向量中的位置
	ir_dc_map = (int *)malloc(ir_var_id * sizeof(int));
	for(int i = 0; i < ir_var_id; i++)
		ir_dc_map[i] = -1;

	array_info *array_list_tail = NULL;
	func_info *func_list_tail = NULL;
	for(ir_list *p = ir_head; p != NULL; p = p->next, ir_cnt++) {
		switch(p->type) {
			case IR_PARAM:
			case IR_READ:
			case IR_ASSIGN:
			case IR_CALL:
			case IR_ARITH: {
				if(p->args[0][0] == 't') {
					int idx = atoi(p->args[0] + 1);
					if(ir_dc_map[idx] == -1) {
						ir_dc_map[idx] = var_cnt;
						var_cnt++;
					}
				}
				break;
			}
			case IR_DEC: {
				int idx = atoi(p->next->args[0] + 1);
				ir_dc_map[idx] = var_cnt;
				array_info *q = (array_info *)malloc(sizeof(array_info));
				q->var_idx = var_cnt;
				q->size = atoi(p->args[1]);
				q->next = NULL;
				if(array_list == NULL)
					array_list = q;
				else
					array_list_tail->next = q;
				array_list_tail = q;
				var_cnt++;
				ir_cnt++;
				p = p->next;
				break;
			}
			case IR_FUNC: {
				func_info *q = (func_info *)malloc(sizeof(func_info));
				q->ir_idx = ir_cnt;
				q->name = p->args[0];
				q->param_start = var_cnt;
				q->next = NULL;
				if(func_list == NULL)
					func_list = q;
				else
					func_list_tail->next = q;
				func_list_tail = q;
			}
		}
	}

	// 位向量中的位置 --> t后面的数字
	dc_ir_map = (int *)malloc(var_cnt * sizeof(int));
	for(int i = 0; i < ir_var_id; i++) {
		if(ir_dc_map[i] > -1)
			dc_ir_map[ir_dc_map[i]] = i;
	}
	
	// 将链表指针存入数组中
	ir_arr = (ir_list **)malloc(ir_cnt * sizeof(ir_list));
	ir_cnt = 0;
	for(ir_list *p = ir_head; p != NULL; p = p->next, ir_cnt++)
		ir_arr[ir_cnt] = p;

	reg_var_map = (int *)malloc(32 * sizeof(int));
	for(int i = 0; i < 32; i++)
		reg_var_map[i] = -1;
	var_reg_map = (int *)malloc(var_cnt * sizeof(int));
	need_update = (int *)malloc(var_cnt * sizeof(int));
	for(int i = 0; i < var_cnt; i++) {
		var_reg_map[i] = -1;
		need_update[i] = 1;
	}

	// 开头声明的一些内容
	fprintf(fp_dc, ".data\n");
	fprintf(fp_dc, "_prompt: .asciiz \"Enter an integer:\"\n");
	fprintf(fp_dc, "_ret: .asciiz \"\\n\"\n");
	fprintf(fp_dc, ".globl main\n");
	fprintf(fp_dc, ".text\n");
}

void dc_assign(int ir_idx) {
	if(ir_arr[ir_idx]->args[1][0] == '#') {
		// *x := #y, x为地址, y为立即数 
		if(ir_arr[ir_idx]->args[0][0] == '*') {
			int idx = var_to_reg(ir_idx, 0, 2, 1);
			fprintf(fp_dc, "li $3, %d\n", atoi(ir_arr[ir_idx]->args[1] + 1));
			fprintf(fp_dc, "sw $3, 0($%d)\n", var_reg_map[idx]);
		}
		// x := #y, x为普通变量, y为立即数
		else {
			int idx = var_to_reg(ir_idx, 0, 1, 0);
			fprintf(fp_dc, "li $%d, %d\n", var_reg_map[idx], atoi(ir_arr[ir_idx]->args[1] + 1));
			need_update[idx] = 1;
		}
	}
	else if(ir_arr[ir_idx]->args[1][0] == '*') {
		// 取y的值 
		int idx1 = var_to_reg(ir_idx, 1, 2, 1);
		reg_used1 = var_reg_map[idx1];
		// *x = *y, x为地址, y为地址
		if(ir_arr[ir_idx]->args[0][0] == '*') {
			int idx = var_to_reg(ir_idx, 0, 2, 1);
			fprintf(fp_dc, "lw $3, 0($%d)\n", var_reg_map[idx1]);
			fprintf(fp_dc, "sw $3, 0($%d)\n", var_reg_map[idx]);
		}
		// x = *y, x为普通变量, y为地址
		else {
			int idx = var_to_reg(ir_idx, 0, 1, 0);
			fprintf(fp_dc, "lw $%d, 0($%d)\n", var_reg_map[idx], var_reg_map[idx1]);
			need_update[idx] = 1;
		}
		reg_used1 = -1;
	}
	else {
		// 取y的值
		int idx1 = var_to_reg(ir_idx, 1, 1, 1);
		reg_used1 = var_reg_map[idx1];
		// *x = y, x为地址, y为普通变量
		if(ir_arr[ir_idx]->args[0][0] == '*') {
			int idx = var_to_reg(ir_idx, 0, 2, 1);
			fprintf(fp_dc, "sw $%d, 0($%d)\n", var_reg_map[idx1], var_reg_map[idx]);
		}
		// x = y, x为普通变量, y为普通变量
		else {
			int idx = var_to_reg(ir_idx, 0, 1, 0);
			fprintf(fp_dc, "move $%d, $%d\n", var_reg_map[idx], var_reg_map[idx1]);
			need_update[idx] = 1;
		}
		reg_used1 = -1;
	}
	TEST_AND_SAVE
}

int lhs_proc(int ir_idx, int *reg_a, int *reg_b) {
	int idx = 0;
	// *x := y op z, x是地址
	if(ir_arr[ir_idx]->args[0][0] == '*') {
		idx = var_to_reg(ir_idx, 0, 2, 1);
		*reg_a = 3;
		*reg_b = 2;
	}
	// x := y op z, x是普通变量
	else {
		idx = var_to_reg(ir_idx, 0, 1, 0);
		*reg_a = var_reg_map[idx];
		*reg_b = 3;
	}
	return idx;
}

void dc_arith(int ir_idx) {
	int reg_a = 0, reg_b = 0, idx = 0;
	if(ir_arr[ir_idx]->args[1][0] == '#') {
		// x := #y op #z, y是立即数, z是立即数
		if(ir_arr[ir_idx]->args[3][0] == '#') {
			idx = lhs_proc(ir_idx, &reg_a, &reg_b);
			fprintf(fp_dc, "li $%d, %d\n", reg_a, atoi(ir_arr[ir_idx]->args[1] + 1));
			fprintf(fp_dc, "li $%d, %d\n", reg_b, atoi(ir_arr[ir_idx]->args[3] + 1));
			switch(ir_arr[ir_idx]->args[2][0]) {
				case '+': fprintf(fp_dc, "add $%d, $%d, $%d\n", reg_a, reg_a, reg_b); break;
				case '-': fprintf(fp_dc, "sub $%d, $%d, $%d\n", reg_a, reg_a, reg_b); break;
				case '*': fprintf(fp_dc, "mul $%d, $%d, $%d\n", reg_a, reg_a, reg_b); break;
				case '/': fprintf(fp_dc, "div $%d, $%d\n", reg_a, reg_b);
			}				
		}
		// x := #y op *z, y是立即数, z是地址
		else if(ir_arr[ir_idx]->args[3][0] == '*') {
			int idx1 = var_to_reg(ir_idx, 3, 2, 1);
			reg_used1 = var_reg_map[idx1];
			idx = lhs_proc(ir_idx, &reg_a, &reg_b);
			fprintf(fp_dc, "lw $%d, 0($%d)\n", reg_a, var_reg_map[idx1]);
			switch(ir_arr[ir_idx]->args[2][0]) {
				case '+': fprintf(fp_dc, "addi $%d, $%d, %d\n", reg_a, reg_a, atoi(ir_arr[ir_idx]->args[1] + 1)); break;
				case '-': fprintf(fp_dc, "li $%d, %d\n", reg_b, atoi(ir_arr[ir_idx]->args[1] + 1));
						  fprintf(fp_dc, "sub $%d, $%d, $%d\n", reg_a, reg_b, reg_a);
						  break;
				case '*': fprintf(fp_dc, "li $%d, %d\n", reg_b, atoi(ir_arr[ir_idx]->args[1] + 1));
						  fprintf(fp_dc, "mul $%d, $%d, $%d\n", reg_a, reg_b, reg_a);
						  break;
				case '/': fprintf(fp_dc, "li $%d, %d\n", reg_b, atoi(ir_arr[ir_idx]->args[1] + 1));
						  fprintf(fp_dc, "div $%d, $%d\n", reg_b, reg_a);
			}
			reg_used1 = -1;
		}
		// x := #y op z, y是立即数, z是普通变量
		else {
			int idx1 = var_to_reg(ir_idx, 3, 1, 1);
			reg_used1 = var_reg_map[idx1];
			idx = lhs_proc(ir_idx, &reg_a, &reg_b);
			switch(ir_arr[ir_idx]->args[2][0]) {
				case '+': fprintf(fp_dc, "addi $%d, $%d, %d\n", reg_a, var_reg_map[idx1], atoi(ir_arr[ir_idx]->args[1] + 1)); break;
				case '-': fprintf(fp_dc, "li $%d, %d\n", reg_b, atoi(ir_arr[ir_idx]->args[1] + 1));
						  fprintf(fp_dc, "sub $%d, $%d, $%d\n", reg_a, reg_b, var_reg_map[idx1]);
						  break;
				case '*': fprintf(fp_dc, "li $%d, %d\n", reg_b, atoi(ir_arr[ir_idx]->args[1] + 1));
						  fprintf(fp_dc, "mul $%d, $%d, $%d\n", reg_a, reg_b, var_reg_map[idx1]);
						  break;
				case '/': fprintf(fp_dc, "li $%d, %d\n", reg_b, atoi(ir_arr[ir_idx]->args[1] + 1));
						  fprintf(fp_dc, "div $%d, $%d\n", reg_b, var_reg_map[idx1]);
			}
			reg_used1 = -1;
		}
	}
	else if(ir_arr[ir_idx]->args[1][0] == '*') {
		int idx1 = var_to_reg(ir_idx, 1, 2, 1);
		reg_used1 = var_reg_map[idx1];
		// x := *y op #z, y是地址, z是立即数
		if(ir_arr[ir_idx]->args[3][0] == '#') {
			idx = lhs_proc(ir_idx, &reg_a, &reg_b);
			fprintf(fp_dc, "lw $%d, 0($%d)\n", reg_a, var_reg_map[idx1]);
			switch(ir_arr[ir_idx]->args[2][0]) {
				case '+': fprintf(fp_dc, "addi $%d, $%d, %d\n", reg_a, reg_a, atoi(ir_arr[ir_idx]->args[3] + 1)); break;
				case '-': fprintf(fp_dc, "addi $%d, $%d, -%d\n", reg_a, reg_a, atoi(ir_arr[ir_idx]->args[3] + 1)); break;
				case '*': fprintf(fp_dc, "li $%d, %d\n", reg_b, atoi(ir_arr[ir_idx]->args[3] + 1));
						  fprintf(fp_dc, "mul $%d, $%d, $%d\n", reg_a, reg_a, reg_b);
						  break;
				case '/': fprintf(fp_dc, "li $%d, %d\n", reg_b, atoi(ir_arr[ir_idx]->args[3] + 1));
						  fprintf(fp_dc, "div $%d, $%d\n", reg_a, reg_b);
			}
			reg_used1 = -1;
		}
		// x := *y op *z, y是地址, z是地址
		else if(ir_arr[ir_idx]->args[3][0] == '*') {
			int idx2 = var_to_reg(ir_idx, 3, 2, 1);
			reg_used2 = var_reg_map[idx2];
			idx = lhs_proc(ir_idx, &reg_a, &reg_b);
			fprintf(fp_dc, "lw $%d, 0($%d)\n", reg_a, var_reg_map[idx1]);
			fprintf(fp_dc, "lw $%d, 0($%d)\n", reg_b, var_reg_map[idx2]);
			switch(ir_arr[ir_idx]->args[2][0]) {
				case '+': fprintf(fp_dc, "add $%d, $%d, $%d\n", reg_a, reg_a, reg_b); break;
				case '-': fprintf(fp_dc, "sub $%d, $%d, $%d\n", reg_a, reg_a, reg_b); break;
				case '*': fprintf(fp_dc, "mul $%d, $%d, $%d\n", reg_a, reg_a, reg_b); break;
				case '/': fprintf(fp_dc, "div $%d, $%d\n", reg_a, reg_b);
			}
			reg_used1 = -1;
			reg_used2 = -1;
		}
		// x := *y op z, y是地址, z是普通变量
		else {
			int idx2 = var_to_reg(ir_idx, 3, 1, 1);
			reg_used2 = var_reg_map[idx2];
			idx = lhs_proc(ir_idx, &reg_a, &reg_b);
			fprintf(fp_dc, "lw $%d, 0($%d)\n", reg_a, var_reg_map[idx1]);
			switch(ir_arr[ir_idx]->args[2][0]) {
				case '+': fprintf(fp_dc, "add $%d, $%d, $%d\n", reg_a, reg_a, var_reg_map[idx2]); break;
				case '-': fprintf(fp_dc, "sub $%d, $%d, $%d\n", reg_a, reg_a, var_reg_map[idx2]); break;
				case '*': fprintf(fp_dc, "mul $%d, $%d, $%d\n", reg_a, reg_a, var_reg_map[idx2]); break;
				case '/': fprintf(fp_dc, "div $%d, $%d\n", reg_a, var_reg_map[idx2]);
			}
			reg_used1 = -1;
			reg_used2 = -1;
		}
	}
	else {
		int idx1 = var_to_reg(ir_idx, 1, 1, 1);
		reg_used1 = var_reg_map[idx1];
		// x := y op #z, y是普通变量, z是立即数
		if(ir_arr[ir_idx]->args[3][0] == '#') {
			idx = lhs_proc(ir_idx, &reg_a, &reg_b);
			switch(ir_arr[ir_idx]->args[2][0]) {
				case '+': fprintf(fp_dc, "addi $%d, $%d, %d\n", reg_a, var_reg_map[idx1], atoi(ir_arr[ir_idx]->args[3] + 1)); break;
				case '-': fprintf(fp_dc, "addi $%d, $%d, -%d\n", reg_a, var_reg_map[idx1], atoi(ir_arr[ir_idx]->args[3] + 1)); break;
				case '*': fprintf(fp_dc, "li $%d, %d\n", reg_b, atoi(ir_arr[ir_idx]->args[3] + 1));
						  fprintf(fp_dc, "mul $%d, $%d, $%d\n", reg_a, var_reg_map[idx1], reg_b);
						  break;
				case '/': fprintf(fp_dc, "li $%d, %d\n", reg_b, atoi(ir_arr[ir_idx]->args[3] + 1));
						  fprintf(fp_dc, "div $%d, $%d\n", var_reg_map[idx1], reg_b);
			}
			reg_used1 = -1;
		}
		// x := y op *z, y是普通变量, z是地址
		else if(ir_arr[ir_idx]->args[3][0] == '*') {
			int idx2 = var_to_reg(ir_idx, 3, 2, 1);
			reg_used2 = var_reg_map[idx2];
			idx = lhs_proc(ir_idx, &reg_a, &reg_b);
			fprintf(fp_dc, "lw $%d, 0($%d)\n", reg_a, var_reg_map[idx2]);
			switch(ir_arr[ir_idx]->args[2][0]) {
				case '+': fprintf(fp_dc, "add $%d, $%d, $%d\n", reg_a, var_reg_map[idx1], reg_a); break;
				case '-': fprintf(fp_dc, "sub $%d, $%d, $%d\n", reg_a, var_reg_map[idx1], reg_a); break;
				case '*': fprintf(fp_dc, "mul $%d, $%d, $%d\n", reg_a, var_reg_map[idx1], reg_a); break;
				case '/': fprintf(fp_dc, "div $%d, $%d\n", var_reg_map[idx1], reg_a);
			}
			reg_used1 = -1;
			reg_used2 = -1;
		}
		// x := y op z, y是普通变量, z是普通变量
		else {
			int idx2 = var_to_reg(ir_idx, 3, 1, 1);
			reg_used2 = var_reg_map[idx2];
			idx = lhs_proc(ir_idx, &reg_a, &reg_b);
			switch(ir_arr[ir_idx]->args[2][0]) {
				case '+': fprintf(fp_dc, "add $%d, $%d, $%d\n", reg_a, var_reg_map[idx1], var_reg_map[idx2]); break;
				case '-': fprintf(fp_dc, "sub $%d, $%d, $%d\n", reg_a, var_reg_map[idx1], var_reg_map[idx2]); break;
				case '*': fprintf(fp_dc, "mul $%d, $%d, $%d\n", reg_a, var_reg_map[idx1], var_reg_map[idx2]); break;
				case '/': fprintf(fp_dc, "div $%d, $%d\n", var_reg_map[idx1], var_reg_map[idx2]);
			}
			reg_used1 = -1;
			reg_used2 = -1;
		}
	}
	if(ir_arr[ir_idx]->args[2][0] == '/')
		fprintf(fp_dc, "mflo $%d\n", reg_a);
	if(ir_arr[ir_idx]->args[0][0] == '*')
		fprintf(fp_dc, "sw $%d, 0($%d)\n", reg_a, var_reg_map[idx]);
	need_update[idx] = 1;
	TEST_AND_SAVE
}

void cond_jump(int ir_idx) {
	// IF x relop y GOTO z
	// y是立即数
	if(ir_arr[ir_idx]->args[2][0] == '#')
		fprintf(fp_dc, "li $2, %d\n", atoi(ir_arr[ir_idx]->args[2] + 1));
	// y是地址
	else if(ir_arr[ir_idx]->args[2][0] == '*') {
		int idx = atoi(ir_arr[ir_idx]->args[2] + 2);
		idx = ir_dc_map[idx];
		if(var_reg_map[idx] == -1) {
			fprintf(fp_dc, "lw $2, %d($fp)\n", 4 * (start_idx - idx));
			fprintf(fp_dc, "lw $2, 0($2)\n");
		}
		else
			fprintf(fp_dc, "lw $2, 0($%d)\n", var_reg_map[idx]);
	}
	// y是普通变量
	else {
		int idx = atoi(ir_arr[ir_idx]->args[2] + 1);
		idx = ir_dc_map[idx];
		if(var_reg_map[idx] == -1)
			fprintf(fp_dc, "lw $2, %d($fp)\n", 4 * (start_idx - idx));
		else
			fprintf(fp_dc, "move $2, $%d\n", var_reg_map[idx]);
	}
	reg_save(4, 26);
	char *cond = ir_arr[ir_idx]->args[1], *lb = ir_arr[ir_idx]->args[3];
	if(cond[0] == '=' && cond[1] == '=')
		fprintf(fp_dc, "beq $3, $2, %s\n", lb);
	else if(cond[0] == '!' && cond[1] == '=')
		fprintf(fp_dc, "bne $3, $2, %s\n", lb);
	else if(cond[0] == '>' && cond[1] == '\0')
		fprintf(fp_dc, "bgt $3, $2, %s\n", lb);
	else if(cond[0] == '<' && cond[1] == '\0')
		fprintf(fp_dc, "blt $3, $2, %s\n", lb);
	else if(cond[0] == '>' && cond[1] == '=')
		fprintf(fp_dc, "bge $3, $2, %s\n", lb);
	else if(cond[0] == '<' && cond[1] == '=')
		fprintf(fp_dc, "ble $3, $2, %s\n", lb);
}

void dc_if(int ir_idx) {
	// IF x relop y GOTO z
	// x是立即数
	if(ir_arr[ir_idx]->args[0][0] == '#') {
		fprintf(fp_dc, "li $3, %d\n", atoi(ir_arr[ir_idx]->args[0] + 1));
		cond_jump(ir_idx);
	}
	// x是地址
	if(ir_arr[ir_idx]->args[0][0] == '*') {
		int idx = atoi(ir_arr[ir_idx]->args[0] + 2);
		idx = ir_dc_map[idx];
		if(var_reg_map[idx] == -1) {
			fprintf(fp_dc, "lw $3, %d($fp)\n", 4 * (start_idx - idx));
			fprintf(fp_dc, "lw $3, 0($3)\n");
		}
		else
			fprintf(fp_dc, "lw $3, 0($%d)\n", var_reg_map[idx]);
		cond_jump(ir_idx);
	}
	// x是普通变量
	else {
		int idx = atoi(ir_arr[ir_idx]->args[0] + 1);
		idx = ir_dc_map[idx];
		if(var_reg_map[idx] == -1)
			fprintf(fp_dc, "lw $3, %d($fp)\n", 4 * (start_idx - idx));
		else
			fprintf(fp_dc, "move $3, $%d\n", var_reg_map[idx]);
		cond_jump(ir_idx);
	}
}

int stk_prepare(char *name) {
	// 找函数在func_list中的位置
	func_info *p = func_list;
	for(; p != NULL && strcmp(p->name, name) != 0; p = p->next);
	
	// 第一个局部变量的编号
	start_idx = p->param_start;
	// 最后一个局部变量的编号
	int end_idx = var_cnt;
	if(p->next)
		end_idx = p->next->param_start;
		
	// 计算需要为该函数的数组预留的空间
	int arr_size = 0;
	// 找到该函数的第一个数组的位置
	array_info *arr_start = NULL;
	for(array_info *q = array_list; q != NULL && q->var_idx < end_idx; q = q->next) {
		if(q->var_idx >= start_idx) {
			if(arr_start == NULL)
				arr_start = q;
			arr_size += q->size;
		}
	}

	// 总共需要的局部变量空间 = 数组大小 + 4 * 非数组变量的个数
	int size = arr_size + 4 * (end_idx - start_idx);
	// 设置新的栈顶
	fprintf(fp_dc, "addi $sp, $sp, %d\n", -size);
	
	arr_size = -size + 4;
	// 让指针变量指向合适的位置
	for(array_info *q = arr_start; q != NULL && q->var_idx < end_idx; q = q->next) {
		fprintf(fp_dc, "addi $3, $fp, %d\n", arr_size);
		fprintf(fp_dc, "sw $3, %d($fp)\n", 4 * (start_idx - q->var_idx));
		arr_size += q->size;
	}

	// 返回函数声明所在中间代码序号
	return p->ir_idx;
}

void dc_call(int ir_idx) {
	// 统计ARG的个数
	int arg_num = 0;
	int i = ir_idx - 1;
	for(; ir_arr[i]->type == IR_ARG; i--, arg_num++);
	// 保存当前栈底指针寄存器($fp), 返回地址寄存器($ra)到内存中, 用于调用结束后恢复
	fprintf(fp_dc, "sw $ra, 0($sp)\n");
	fprintf(fp_dc, "addi $sp, $sp, -4\n");
	fprintf(fp_dc, "sw $fp, 0($sp)\n");
	fprintf(fp_dc, "addi $sp, $sp, -4\n");

	// 再次保存当前栈底指针寄存器($fp)到寄存器$2中, 方便参数传递时的运算
	// 设置新的栈底指针寄存器值, 即为当前栈顶位置
	fprintf(fp_dc, "move $2, $fp\n");
	fprintf(fp_dc, "move $fp, $sp\n");
	
	// 调用者的第一个局部变量, 用于定位调用者提供的实参
	int old_start = start_idx;
	// x := CALL f
	int idx = stk_prepare(ir_arr[ir_idx]->args[1]);
	// 将调用者的实参填入被调者的栈帧中, 完成参数传递
	for(int i = arg_num - 1; i > -1; i--) {
		int idx1 = atoi(ir_arr[idx + arg_num - i]->args[0] + 1);
		idx1 = ir_dc_map[idx1];
		if(ir_arr[ir_idx - arg_num + i]->args[0][0] == '#') {
			fprintf(fp_dc, "li $3, %d\n", atoi(ir_arr[ir_idx - arg_num + i]->args[0] + 1));
			fprintf(fp_dc, "sw $3, %d($fp)\n", 4 * (start_idx - idx1));
		}
		else if(ir_arr[ir_idx - arg_num + i]->args[0][0] == '*') {
			int idx2 = atoi(ir_arr[ir_idx - arg_num + i]->args[0] + 2);
			idx2 = ir_dc_map[idx2];
			if(var_reg_map[idx2] == -1) {
				fprintf(fp_dc, "lw $3, %d($2)\n", 4 * (old_start - idx2));
				fprintf(fp_dc, "lw $3, 0($3)\n");
			}
			else
				fprintf(fp_dc, "lw $3, 0($%d)\n", var_reg_map[idx2]);
			fprintf(fp_dc, "sw $3, %d($fp)\n", 4 * (start_idx - idx1));
		}
		else {
			int idx2 = atoi(ir_arr[ir_idx - arg_num + i]->args[0] + 1);
			idx2 = ir_dc_map[idx2];
			if(var_reg_map[idx2] == -1) {
				fprintf(fp_dc, "lw $3, %d($2)\n", 4 * (old_start - idx2));
				fprintf(fp_dc, "sw $3, %d($fp)\n", 4 * (start_idx - idx1));
			}
			else
				fprintf(fp_dc, "sw $%d, %d($fp)\n", var_reg_map[idx2], 4 * (start_idx - idx1));
		}
	}
	reg_save(4, 26);
	fprintf(fp_dc, "jal %s\n", ir_arr[ir_idx]->args[1]);
	// 恢复栈底指针寄存器, 返回地址寄存器
	start_idx = old_start;
	fprintf(fp_dc, "lw $fp, 0($sp)\n");
	fprintf(fp_dc, "addi $sp, $sp, 4\n");
	fprintf(fp_dc, "lw $ra, 0($sp)\n");
	
	// 返回值通过寄存器传递
	idx = var_to_reg(ir_idx, 0, 1, 0);
	fprintf(fp_dc, "move $%d, $2\n", var_reg_map[idx]);
	need_update[idx] = 1;
}

void dc_func(int ir_idx) {
	fprintf(fp_dc, "%s:\n", ir_arr[ir_idx]->args[0]);
	// main函数没有显式的调用者, 在这里要额外为其准备栈帧
	if(strcmp(ir_arr[ir_idx]->args[0], "main") == 0) {
		fprintf(fp_dc, "move $fp, $sp\n");
		stk_prepare("main");
	}
	// 其他函数在函数列表中查找, 设置第一个局部变量的编号
	else {
		func_info *p = func_list;
		for(; p != NULL && strcmp(p->name, ir_arr[ir_idx]->args[0]) != 0; p = p->next);
		start_idx = p->param_start;
	}
}

void dc_ret(int ir_idx) {
	// RETURN x
	// 载入返回值到寄存器$2
	if(ir_arr[ir_idx]->args[0][0] == '#')
		fprintf(fp_dc, "li $2, %d\n", atoi(ir_arr[ir_idx]->args[0] + 1));
	else if(ir_arr[ir_idx]->args[0][0] == '*') {
		int idx = atoi(ir_arr[ir_idx]->args[0] + 2);
		idx = ir_dc_map[idx];
		if(var_reg_map[idx] == -1) {
			fprintf(fp_dc, "lw $3, %d($fp)\n", 4 * (start_idx - idx));
			fprintf(fp_dc, "lw $2, 0($3)\n");
		}
		else
			fprintf(fp_dc, "lw $2, 0($%d)\n", var_reg_map[idx]);
	}
	else {
		int idx = atoi(ir_arr[ir_idx]->args[0] + 1);
		idx = ir_dc_map[idx];
		if(var_reg_map[idx] == -1)
			fprintf(fp_dc, "lw $2, %d($fp)\n", 4 * (start_idx - idx));
		else
			fprintf(fp_dc, "move $2, $%d\n", var_reg_map[idx]);
	}
	// 退掉整个栈帧
	fprintf(fp_dc, "addi $sp, $fp, 4\n");
	// 清空寄存器
	for(int j = 4; j < 26; j++) {
		int idx = reg_var_map[j];
		if(idx != -1)
			var_reg_map[idx] = -1;
		reg_var_map[j] = -1;
	}
	fprintf(fp_dc, "jr $ra\n");
}

void dc_read(int ir_idx) {
	fprintf(fp_dc, "li $2, 4\n");
	fprintf(fp_dc, "la $4, _prompt\n");
	fprintf(fp_dc, "syscall\n");
	fprintf(fp_dc, "li $2, 5\n");
	fprintf(fp_dc, "syscall\n");
	int idx = var_to_reg(ir_idx, 0, 1, 0);
	fprintf(fp_dc, "move $%d, $2\n", var_reg_map[idx]);
	need_update[idx] = 1;
	TEST_AND_SAVE
}

void dc_write(int ir_idx) {
	reg_save(4, 5);
	if(ir_arr[ir_idx]->args[0][0] == '#')
		fprintf(fp_dc, "li $4, %d\n", atoi(ir_arr[ir_idx]->args[0] + 1));
	else if(ir_arr[ir_idx]->args[0][0] == '*') {
		int idx = var_to_reg(ir_idx, 0, 2, 1);
		fprintf(fp_dc, "lw $4, 0($%d)\n", var_reg_map[idx]);
	}
	else {
		int idx = var_to_reg(ir_idx, 0, 1, 1);
		fprintf(fp_dc, "move $4, $%d\n", var_reg_map[idx]);
	}
	fprintf(fp_dc, "li $2, 1\n");
	fprintf(fp_dc, "syscall\n");
	fprintf(fp_dc, "li $2, 4\n");
	fprintf(fp_dc, "la $4, _ret\n");
	fprintf(fp_dc, "syscall\n");
	TEST_AND_SAVE
}
