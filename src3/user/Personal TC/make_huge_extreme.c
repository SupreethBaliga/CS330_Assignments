
#include<ulib.h>

int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
	int pageSize = 4096;
	u32 MB = 1 << 20;

  u64 aligned = 0x180400000; // 2MB aligned address

	char *paddr = mmap((void *)aligned, 3*MB, PROT_READ|PROT_WRITE, 0);
    pmap(1);

  for (int i = 0; i < 4096 ; i++) {
		paddr[i] = 'X';
	} 	

	char *hpaddr = (char *) make_hugepage((void*)((u64)paddr+1*MB), 2*MB , PROT_READ|PROT_WRITE, 0);
	if((long)hpaddr <0) {
    printf("Huge page failed\n");
  }
  pmap(1);

}