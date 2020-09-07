#include<stdio.h>
#include<string.h>

void yyrestart(FILE *);
int yyparse(void);
char ir_fname[128], dc_fname[128];

int main(int argc, char **argv) {
	if(argc <= 1)
		return -1;
	FILE *f = fopen(argv[1], "r");
	if(!f) {
		perror(argv[1]);
		return -1;
	}
	strcpy(ir_fname, argv[2]);
	strcpy(dc_fname, argv[3]);
	yyrestart(f);
	yyparse();
	return 0;
}
