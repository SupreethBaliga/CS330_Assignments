#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<fcntl.h>
int findWinner(char c1, char c2) {
	if(c1==c2) return 0;
	else if((c1=='0' && c2=='2') || (c1=='1' && c2=='0') || (c1=='2' && c2=='1')) return 1;
	else return 2;
}


void executeUmpire(char* player1, char* player2) {
	int pts1=0,pts2=0;
	char* g = "GO";
	char temp1[2],temp2[2];
	//checking existence of files
	int k;
	k = open(player1,O_RDONLY);
	if(k<0) exit(-1);
	close(k);
	k = open(player2,O_RDONLY);
	if(k<0) exit(-1);
	close(k);
	int pipfd1[2]; int pipfd2[2];
	int inp1[2]; int inp2[2];
	if(pipe(pipfd1)<0) exit(-1);
	if(pipe(pipfd2)<0) exit(-1);
	if(pipe(inp1)<0) exit(-1);
	if(pipe(inp2)<0) exit(-1);
	int idx,pid=-1;
	for(idx=1;idx<=2;idx++) {
		pid=fork();
		if(pid<0) exit(-1);
		if(pid==0) break;
	}
	if(pid==0) {
		if(idx==1) {
			close(inp1[1]);
			// close(0);
			// dup(inp1[0]);
			dup2(inp1[0],0);
			// close(1);
			// dup(pipfd1[1]);
			dup2(pipfd1[1],1);
			if(execl(player1,player1,NULL)) exit(-1);
		}
		else {
			close(inp2[1]);
			// close(0);
			// dup(inp2[0]);
			dup2(inp2[0],0);
			// close(1);
			// dup(pipfd2[1]);
			dup2(pipfd2[1],1);
			if(execl(player2,player2,NULL)) exit(-1);
		}
		exit(-1);
	}
	else {
		close(pipfd1[1]); close(pipfd2[1]);
		char temp1[2],temp2[2];
		for(int i=1;i<=10;i++) {
			if(write(inp1[1],g,3)==0) exit(-1);
			if(write(inp2[1],g,3)==0) exit(-1);
			if(read(pipfd1[0],temp1,1)==0) exit(-1);
			if(read(pipfd2[0],temp2,1)==0) exit(-1);
			int k = findWinner(temp1[0],temp2[0]);
			if(k==1) pts1++;
			else if(k==2) pts2++;
			else continue;
		}
		close(inp1[1]); close(inp2[1]);
	}
	printf("%d %d",pts1,pts2);
	return;
}

int main(int argc, char* argv[]) {
	executeUmpire(argv[1],argv[2]);
	return 0;
}
