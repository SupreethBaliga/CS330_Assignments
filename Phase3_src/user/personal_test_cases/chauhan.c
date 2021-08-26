#include<ulib.h>



int fn_3()
{
    printf("In fn3\n");    
    return 0;
}

int fn_2(int * a)
{
    printf("In fn2\n");    
    fn_3();
    return a[3];
}

int fn_1()
{
    printf("In fn1\n");    
    int a[9];
    for(int i = 0; i < 9; i++)
        a[i] = i;
    fn_2(a);
    return 0;
}


int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
    int cpid;
    long ret = 0;
    int i, bt_count;
    unsigned long long bt_info[MAX_BACKTRACE];
    
    ret = become_debugger();
    
    cpid = fork();
    
    if(cpid < 0){
        printf("Error in fork\n");
    }
    else if(cpid == 0){
        fn_1();    
    }
    else{
        ret = set_breakpoint(fn_3);
        ret = set_breakpoint(fn_2);
        ret = set_breakpoint(fn_1);
        
        ret = wait_and_continue();
        ret = wait_and_continue();
        ret = wait_and_continue();
        
        printf("BACKTRACE INFORMATION\n");
        bt_count = backtrace((void*)&bt_info);
        
        printf("Backtrace count: %d\n", bt_count);
        for(int i = 0; i < bt_count; i++)
        {
            printf("#%d %x\n", i, bt_info[i]);
        }
        printf("%x %x %x %x\n",fn_1, fn_2, fn_3, main);
        // for exit
        ret = wait_and_continue();    
    }
    
    return 0;
} 
