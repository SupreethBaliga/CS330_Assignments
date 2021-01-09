
#define ROCK        0 
#define PAPER       1 
#define SCISSORS    2 

#define STDIN 		0
#define STDOUT 		1
#define STDERR		2

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include "gameUtils.h"
#include <fcntl.h>
#include <sys/stat.h>
char lines[150][150];

int findWinner(char c1, char c2) {
	if(c1==c2) return 0;
	else if((c1=='0' && c2=='2') || (c1=='1' && c2=='0') || (c1=='2' && c2=='1')) return 1;
	else return 2;
}
int getWalkOver(int numPlayers); // Returns a number between [1, numPlayers]

int carry_out_round(int rounds, int indices[],int arr[],int total,int** inp,int** pipfd) {
	char* g = "GO";
	int walkover=-1;
	if(total%2) {
		int k = getWalkOver(total);
		walkover = indices[k-1];
		for(int i=k-1;i<total-1;i++) {
			indices[i]=indices[i+1];
		}
		total--;
	}
	char temp[total][2];
	int pts[total];
	for(int i=0;i<total;i++) pts[i]=0;
	for(int i=1;i<=rounds;i++) {
		for(int j=0;j<total;j++) {
			if(write(inp[indices[j]][1],g,3)==0) exit(-1);
			if(read(pipfd[indices[j]][0],temp[j],1)==0) exit(-1);
		}
		for(int j=0;j<total;j+=2) {
			int k = findWinner(temp[j][0],temp[j+1][0]);
			if(k==1) {
				pts[j]++;
			}
			else if(k==2) {
				pts[j+1]++;
			}
			else continue;
		}
	}
	int x=0; // arr index
	for(int i=0;i<total;i+=2) {
		int win;
		if(pts[i]>=pts[i+1]) win = indices[i];
		else win = indices[i+1];
		if(walkover!=-1 && walkover<win) {
			arr[x] = walkover;
			walkover=-1;
			i-=2;
			x++;
			continue;	
		}
		arr[x] = win;
		int removed = indices[i]+indices[i+1]-win;
		close(inp[removed][1]); //terminate the losing process
		x++;
	}
	if(walkover!=-1) {
		arr[x] = walkover;
		x++;
	}
	return x;
}

void print_arr(int arr[],int total) {
	for(int i=0;i<total-1;i++) {
		printf("p%d ",arr[i]);
	}
	printf("p%d",arr[total-1]);
	if(total>1) printf("\n");
}

void umpire2(int rounds, char* players) {
	int input = open(players,O_RDONLY);
	char input_text[2];
	int l=0,k=0;

	while(read(input,input_text,1)!=0) {
		if(input_text[0]=='\n') {
			lines[l][k]='\0';
			k=0;
			l++;
		}
		else {
			lines[l][k] = input_text[0];
			k++;
		}
	}
	int total = atoi(lines[0]);
	// check if all the files present
	for(int i=1;i<=total;i++) {
		int k = open(lines[i],O_RDONLY);
		if(k<0) exit(-1);
		close(k);
	}
	int** inp = (int**)malloc((total)*sizeof(int*));
	int** pipfd = (int**)malloc((total)*sizeof(int*));
	for(int i=0;i<total;i++) {
		inp[i] = (int*)malloc(2*sizeof(int));
		pipfd[i] = (int*)malloc(2*sizeof(int));
	}
	for(int i=0;i<total;i++) {
		if(pipe(inp[i])<0) exit(-1);
		if(pipe(pipfd[i])<0) exit(-1);
	}
	int idx=-1,pid=-1;
	for(idx=0;idx<total;idx++) {
		pid=fork();
		if(!pid) break;
	}
	if(!pid) {
		close(inp[idx][1]);
		// close(0);
		// dup(inp[idx][0]);
		dup2(inp[idx][0],0);
		// close(1);
		// dup(pipfd[idx][1]);
		dup2(pipfd[idx][1],1);
		char* args[2];
		args[0] = lines[idx+1];
		args[1] = NULL;
		if(execv(args[0],args)) exit(-1);
	}
	else {
		for(int i=0;i<total;i++) {
 			close(pipfd[i][1]);
 		}
 		int* indices = (int*)malloc(total*sizeof(int));
 		int* arr = (int*)malloc(total*sizeof(int));
 		for(int i=0;i<total;i++) indices[i]=i;
 		print_arr(indices,total);
 		while(total>1) {
 			total=carry_out_round(rounds,indices,arr,total,inp,pipfd);
 			for(int i=0;i<total;i++) indices[i] = arr[i];
 			print_arr(indices,total);
 		}
	}
}

int main(int argc, char *argv[])
{
	if(argc==4 && strlen(argv[1])==2 && argv[1][0]=='-' && argv[1][1]=='r') {
		umpire2(atoi(argv[2]),argv[3]);
	}
	else {
		umpire2(10,argv[1]);
	}
	return 0;
}
