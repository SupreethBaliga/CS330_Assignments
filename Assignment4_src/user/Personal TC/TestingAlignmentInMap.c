#include<ulib.h>


int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
  // MMAP_START = 0x180200000
  char *addr1 = mmap(180300001, 22, PROT_READ|PROT_WRITE, 0);
  if((long)addr1 < 0)
  {
    printf("Mapping failed1");
    return 1;
  }
  // Vm_Area count should be 1
  // Expected output will have address printed. In your case address printed might be different.
  // But See the printed address, (i.e) the start and the end address of the dumped vm area is page aligned irrespective of the length provided.
  pmap(1);


  // Access flag is different should create a new vm_area
  char *addr2 = mmap(180200001, 4096, PROT_WRITE, 1); //should fail
  if((long)addr2 < 0)
  {
    printf("Mapping failed 2\n");
    return 1;
  }
  //  Vm_Area count should be 2
  pmap(1);

  // Access flag is different should create a new vm_area
  char *addr3 = mmap(NULL, 8192, PROT_WRITE|PROT_READ, 0); 
  if((long)addr3 < 0)
  {
    printf("Mapping failed 3\n");
    return 1;
  }

  pmap(1);

  // Access flag is different should create a new vm_area
  char *addr4 = mmap(180300001, 8192, PROT_WRITE|PROT_READ, 0); 
  if((long)addr4 < 0)
  {
    printf("Mapping failed 4\n");
    return 1;
  }

  //  Vm_Area count should be 2
  pmap(1);

  int mun = munmap(addr3,100000000);

}
