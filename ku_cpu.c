#include <stdio.h>
#include <stdlib.h>
#include "./ku_mmu.h"

// command : ku_cpu <input_file> <pmem_size> <swap_size>

int ku_traverse(void *, char, void *);

void ku_mmu_fin(FILE *fd, void *pmem)
{
	if(fd) fclose(fd);
	if(pmem) free(pmem);
}

int main(int argc, char *argv[])
{
	FILE *fd=NULL;
	char fpid, pid=0, va, pa;
	unsigned int pmem_size, swap_size;
	void *ku_cr3, *pmem=NULL;

	if(argc != 4){
		printf("ku_cpu: Wrong number of arguments\n");
		return 1;
	}

	fd = fopen(argv[1], "r");
	if(!fd){
		printf("ku_cpu: Fail to open the input file\n");
		return 1;
	}

	pmem_size = strtol(argv[2], NULL, 10);
	swap_size = strtol(argv[3], NULL, 10);
	pmem = ku_mmu_init(pmem_size, swap_size);
	if(!pmem){
		printf("ku_cpu: Fail to allocate the physical memory\n");
		ku_mmu_fin(fd, pmem);
		return 1;
	}
	
	while(fscanf(fd, "%hhd %hhd", &fpid, &va) != EOF){

		if(pid != fpid){
			
			if(ku_run_proc(fpid, &ku_cr3) == 0)
				pid = fpid; /* context switch */
			else{
				printf("ku_cpu: Context switch is failed\n");
				ku_mmu_fin(fd, pmem);
				return 1;
			}
			printf("ku_os: fpid[%d], ku_cr3[%p]\n\n", fpid, ku_cr3);
			ku_mmu_test(ku_mmu_pmem,ku_mmu_pmem_size);
		}
		pa = ku_traverse(ku_cr3, va, pmem);
		if(pa == 0){
			if(ku_page_fault(pid, va) != 0){
				printf("ku_cpu: Fault handler is failed\n");
				ku_mmu_fin(fd, pmem);
				return 1;
			}
			printf("[%d] VA: %hhd -> Page Fault\n", pid, va);
			
			ku_mmu_test(ku_mmu_pmem,ku_mmu_pmem_size);
			/* Retry after page fault */
			
			pa = ku_traverse(ku_cr3, va, pmem); 
			// my traverse
			// char pd_offset=0,pmd_offset=0,pt_offset=0;
			// pd_offset=((unsigned char)va&0xC0)>>6;
			// pmd_offset=(va&0x30)>>4;
			// pt_offset=(va&0xC)>>2;
			// printf("pd offset:%hd, pmd offset:%hd, pt offset:%hd\n", pd_offset, pmd_offset, pt_offset);
			// char pde=*((char*)ku_cr3+pd_offset);
			// printf("pde:%#x \n",pde);
			// char pmd=(unsigned char)pde>>2;
			// pmd+=pmd_offset;
			// printf("pmd:%#x \n",pmd);
			// char pmde=*((char*)pmem+pmd);
			// printf("pmde:%#x \n",pmde);
			// char pt=(unsigned char)pmde>>2;
			// printf("pt:%#x \n",pt);
			// char pte=*((char*)pmem+pt+pt_offset);
			// printf("pte:%#x \n",pte);
			// my traverse

			if(pa == 0){
				printf("ku_cpu: Addr tanslation is failed\n");
				ku_mmu_fin(fd, pmem);
				return 1;
			}
		}

		printf("[%d] VA: %hhd -> PA: %hhd\n", pid, va, pa);
	}

	ku_mmu_fin(fd, pmem);
	return 0;
}
