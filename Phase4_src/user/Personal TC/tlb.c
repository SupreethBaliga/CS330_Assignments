#include <ulib.h>

int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
 int pageSize = 4096;
 u32 MB = 1 << 20;

 u64 aligned = 0x180400000; // 2MB aligned address
 char *addr = mmap((void *)aligned, pageSize, PROT_WRITE | PROT_READ, 0);
 addr[0] = 'X';
 if(addr[0]!='X') printf("whoop whoop!\n");
 munmap(addr, pageSize);
 // this should give a segfault
 if(addr[0] =='X') printf("OMG\n");
 printf("FAILED\n");
	
 return 0;
}