#include"semantic.h"
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

symbol_tree *symbol_root = NULL;
int node_cnt = 0;

/*
 * A(2)         B(0)
 *  \          / \
 *   B(1) --> A(0)C
 *    \
 *     C
 */
/*
 *  C(-2)    C
 * /        /
 *A(1) --> B(n)
 * \      /
 *  B(n) A(0,-1)
*/
symbol_tree *left_rotate(symbol_tree *p, int bf_mode) {
	// p指向A
	// root_new指向B
	symbol_tree *root_new = p->right;
	// B的左子树成为A的右子树
	p->right = root_new->left;
	// A成为B的左子树
	root_new->left = p;
	// 单旋转, 令A的平衡因子都为0
	if(!bf_mode)
		p->bf = 0;
	// 作为双旋转的一部分, 根据B的旧平衡因子设置A的新平衡因子
	else {
		if(root_new->bf <= 0)
			p->bf = 0;
		else
			p->bf = -1;	
	}
	// 返回新的根结点B
	return root_new;
}


/*
 *     A(-2)    B(0)
 *    /         /\
 *   B(-1) --> C  A(0)
 *  /
 * C
 */
/*
 *C(2)      C(2)
 * \         \
 *  A(-1) --> B(n)
 * /           \
 *B(n)          A(0,1)
 */
symbol_tree *right_rotate(symbol_tree *p, int bf_mode) {
	// p指向A
	// root_new指向B
	symbol_tree *root_new = p->left;
	// B的右子树成为A的左子树
	p->left = root_new->right;
	// A成为B的右子树
	root_new->right = p;
	// 单旋转, 令A的平衡因子都为0
	if(!bf_mode)
		p->bf = 0;
	// 作为双旋转的一部分, 根据B的旧平衡因子设置A的新平衡因子
	else {
		if(root_new->bf >= 0)
			p->bf = 0;
		else
			p->bf = 1;
	}
	// 返回新的根结点B
	return root_new;
}

symbol_tree *avl_insert(char *name) {
	symbol_tree *q = NULL, *p = symbol_root, **stk = (symbol_tree **)malloc((node_cnt + 1) * sizeof(symbol_tree *));
	int top = 0;
	// 寻找插入位置
	while(p != NULL) {
		// 新增符号不与任意已有符号相同, 才能插入
		if(strcmp(name, p->name) != 0) {
			q = p;
			
			// 建立一个用于记录由根结点到当前结点路径的栈
			// 当前结点入栈
			stk[top] = p;
			top++;
			
			// 新符号小于当前结点值, 沿左子树继续找
			if(strcmp(name, p->name) < 0)
				p = p->left;
			// 新符号大于当前结点值, 沿右子树继续找
			else
				p = p->right;
		}
		// 新增符号与某个已有符号相同, 放弃插入
		else {
			free(stk);
			return NULL;
		}
	}
	
	node_cnt++;
	// 建立新的AVL树结点，平衡因子初始化为零
	p = (symbol_tree *)malloc(sizeof(symbol_tree));
	p->name = (char *)malloc((strlen(name) + 1) * sizeof(char));
	strcpy(p->name, name);
	p->bf = 0;
	p->left = NULL;
	p->right = NULL;
	
	// 初始化变量类型, 以及返回值类型, 均为"未定义"
	p->type = SYM_UNDEF;
	p->ret_type = SYM_UNDEF;
	
	symbol_tree *ret = p;
	// 空树, 插入结点成为根结点
	if(q == NULL) {
		symbol_root = p;
		free(stk);
		return ret;
	}
	// 非空树，根据与原来的叶结点的大小关系, 选择成为其左子结点或右子结点
	else if(strcmp(name, q->name) < 0)
		q->left = p;
	else
		q->right = p;

	while(top > 0) {
		// 栈中退出一个元素, q指向退出的元素
		top--;
		q = stk[top];

		// 若当前结点是其父结点的左子结点, 父结点bf - 1
		if(p == q->left)
			q->bf--;
		// 若当前结点是其父结点的右子结点, 父结点bf + 1
		else
			q->bf++;

		// 若插入后, 当前父结点恰达到平衡, 不用旋转, 并且其父结构平衡性必无问题, 不需要回溯
		if(q->bf == 0)
			break;

		// 若当前父结点平衡因子绝对值为1, 暂时不用旋转, 但需要继续回溯
		if(q->bf == 1 || q->bf == -1)
			p = q;
		// 若当前父结点平衡因子绝对值大于1, 需要旋转
		else {
			// pq同号旋转一次: 同正左旋, 同负右旋
			// pq异号旋转两次: 下正上负-->先左后右, 下负上正-->先右后左
			if(p->bf > 0 && q->bf > 0)
				q = left_rotate(q, 0);
			else if(p->bf > 0 && q->bf < 0) {
				q->left = left_rotate(p, 1);
				q = right_rotate(q, 1);
			}
			else if(p->bf < 0 && q->bf < 0)
				q = right_rotate(q, 0);
			else {
				q->right = right_rotate(p, 1);
				q = left_rotate(q, 1);
			}
			// 新的根结点平衡因子为0
			q->bf = 0;
			// 最多调整一次
			break;
		}
	}

	// 若调整到根结点, 将调整后的根结点取代原根结点
	if(top == 0)
		symbol_root = q;
	// 若调整到树的中间, 找当前结点的父节点
	else {
		symbol_tree *r = stk[top - 1];
		// 调整好的子树根结点插回其父节点
		if(strcmp(q->name, r->name) < 0)
			r->left = q;
		else
			r->right = q;
	}
	free(stk);
	
	return ret;
}
