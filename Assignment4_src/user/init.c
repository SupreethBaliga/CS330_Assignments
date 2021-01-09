#include <ulib.h>

int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
 int pageSize = 4096;
 u32 MB = 1 << 20;

 u64 aligned = 0x180400000; // 2MB aligned address
 char *addr = (char*)mmap((void *)aligned, 2*MB, PROT_WRITE | PROT_READ, 0);
 addr[0] = 'X';

 char* hpaddr = (char*)make_hugepage((void*)addr,2*MB,PROT_READ|PROT_WRITE,0);
 pmap(1);
 hpaddr[0] = 'X';
// addr[0] = 'X';
 if(hpaddr[0]!='X') printf("whoop whoop!\n");
 munmap(addr, pageSize);
 // this should give a segfault
 if(addr[0] =='X') printf("OMG\n");
 printf("FAILED\n");
	
 return 0;
}