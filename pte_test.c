#include <stdio.h>
#include <stdlib.h>

// 
char **pmem, **swap;

void *ku_mmu_init(unsigned int mem_size, unsigned int swap_size){
	// 페이지 크기가 4바이트이므로, 4로 나눈 나머지는 쓸 수 없다.
	mem_size=mem_size-(mem_size%4);
	printf("ku_os: Your physical memory size is %dbytes.\n", mem_size);
	swap_size=swap_size-(swap_size%4);
	printf("ku_os: Your swap space size is %dbytes.\n", swap_size);

	// 4바이트 만큼씩(페이지 단위) 할당하고 모두 0으로 초기화
	pmem=calloc(mem_size/4, 4);
	swap=calloc(swap_size/4, 4);
	for(int i=0;i<mem_size/4; i++){
		pmem[i]=calloc(4,1);
	}
	for(int i=0;i<swap_size/4; i++){
		swap[i]=calloc(4,1);
	}

	//모두 0으로 초기화 된 것 확인
	for(int i=0;i<mem_size/4; i++){
		printf("%d 번째 페이지 값\n",i);
		for(int j=0;j<4;j++){
			printf("%d : %hhd / ", j, pmem[i][j]);
		}
		printf("\n");
	}
	printf("\n");
	for(int i=0;i<swap_size/4; i++){
		printf("%d 번째 페이지 값\n",i);
		for(int j=0;j<4;j++){
			printf("%d : %hhd / ", j, swap[i][j]);
		}
		printf("\n");
	}
    return pmem;
}

int main(int argc, char *argv[]){
	unsigned int mem_size, swap_size;
	mem_size = strtol(argv[1], NULL, 10);
	swap_size = strtol(argv[2], NULL, 10);

	pmem = ku_mmu_init(mem_size, swap_size);
	
	return 0;
}
