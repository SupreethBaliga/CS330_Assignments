    #include<ulib.h>
    #define sz (1ULL * 4096 * 512 * 2)
    // Page fault handler working correctly.
     
    int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
    {
    	int pageSize = 4096;
    	u32 MB = 1 << 20;
     
        u64 aligned = 0x180400000; // 2MB aligned address
     
    	char *paddr = mmap((void *)aligned, 4*MB, PROT_READ|PROT_WRITE, 0);
        pmap(1);
     
      	for (int i = 0; i < sz; i++) {
    		paddr[i] = 'X';
    	}
     
    	char *hpaddr = (char *) make_hugepage(paddr, 2*MB , PROT_READ|PROT_WRITE, 0);
     
    	pmap(1);
     
        if((long)hpaddr < 0){
    		printf("HEHEH\n");
    		return 1;
    	}
     
        int diff = 0;
    	for (int i = 0; i < (sz >> 1); i++) {
            // The content of the pages that get converted to huge pages are preserved.
    		if (*(hpaddr + i) != 'X') {
    			++diff;	
    		}
    	}
    	
    	printf(" diff : %d\n" , diff);
     
    	int ret = munmap(hpaddr, 1); // removes huge page
    	
    	if(ret < 0){
    		printf("Error : munmap\n");
    		return 1;
    	}	
    	pmap(1);
     
    	printf("*(hpaddr + 2MB) = %c\n", *(hpaddr + 2*MB));
     
    	printf("*(hpaddr) = %c\n" , *(hpaddr)); // should seg fault
     
     
    	if(diff)
    		printf(" diff : %d , FAILED\n" , diff);
    	else
    		printf("NOT FUCKKING PASSED\n");
     
    // 	int val = break_hugepage(hpaddr, 2*MB );
    // 	pmap(1);
        
    //     diff = 0;
    // 	for (int i = 0; i < 2097152; i++) {
    //         // The content of the pages that get converted to huge pages are preserved.
    // 		if (hpaddr[i] != 'X') {
    // 			++diff;	
    // 		}else{
    // 			// printf("%c\n",hpaddr[i]);
    // 		}
    // 	}
    	
    //   printf("%d\n",diff);
    // 	if(diff)
    // 		printf("FAILED\n");
    // 	else
    // 		printf("PASSED\n");
     
    }
     