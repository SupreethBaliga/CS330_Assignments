#include<ulib.h>

int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
    int pages = 4096;

    char * mm1 = mmap(NULL, pages*2, PROT_READ|PROT_WRITE, 0);
    pmap(1);
    for(int i=10;i<8000;i++) {
        *(mm1 +i) = 'X';
    }
    int diff=0;
    for(int i=0;i<2*pages;i++) {
        if(*(mm1+i) == 'X') diff++;
    }
    printf("%d\n",diff);
    return 0;
}
