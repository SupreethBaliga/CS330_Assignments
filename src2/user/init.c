#include<ulib.h>

int fn_1()
{
	printf("In fn1\n");	
	return 0;
}

int fn_2()
{
	printf("In fn2\n");	
	return 0;
}

int fn_3()
{
	printf("In fn3\n");	
	return 0;
}
int fn_4()
{
	printf("In fn4\n");	
	return 0;
}
int fn_5()
{
	printf("In fn5\n");	
	return 0;
}
int fn_6()
{
	printf("In fn6\n");	
	return 0;
}

int fn_7()
{
	printf("In fn7\n");	
	return 0;
}

int fn_8()
{
	printf("In fn8\n");	
	return 0;
}

int fn_9()
{
	printf("In fn9\n");	
	return 0;
}


int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
	int cpid;
	long ret = 0;
	int i, bt_count;
	
	ret = become_debugger();
	
	cpid = fork();
	
	if(cpid < 0){
		printf("Error in fork\n");
	}
	else if(cpid == 0){
		printf("fn_1 : %x\n", fn_1);
		printf("fn_2 : %x\n", fn_2);
		printf("fn_3 : %x\n", fn_3);
		printf("fn_4 : %x\n", fn_4);
		printf("fn_5 : %x\n", fn_5);
		printf("fn_6 : %x\n", fn_6);
		printf("fn_7 : %x\n", fn_7);
		printf("fn_8 : %x\n", fn_8);
		printf("fn_9 : %x\n", fn_9);

		fn_1();
		fn_2();
		fn_3();
		fn_4();
		fn_5();
		fn_6();
		fn_7();
		fn_8();
		fn_9();

	}
	else{
		ret = set_breakpoint(fn_1);
		printf("setting breakpoint return value: %x\n",ret);
		ret = set_breakpoint(fn_2);
		printf("setting breakpoint return value: %x\n",ret);
		ret = set_breakpoint(fn_3);
		printf("setting breakpoint return value: %x\n",ret);
		ret = set_breakpoint(fn_4);
		printf("setting breakpoint return value: %x\n",ret);
		ret = set_breakpoint(fn_5);
		printf("setting breakpoint return value: %x\n",ret);
		ret = set_breakpoint(fn_6);
		printf("setting breakpoint return value: %x\n",ret);
		ret = set_breakpoint(fn_7);
		printf("setting breakpoint return value: %x\n",ret);
		ret = set_breakpoint(fn_8);
		printf("setting breakpoint return value: %x\n",ret);
		ret = set_breakpoint(fn_9);
		printf("setting breakpoint return value: %x\n",ret);

		
		while(ret=wait_and_continue()) {
			printf("Breakpoint hit at : %x\n", ret);
		}	
	}
	
	return 0;
}
