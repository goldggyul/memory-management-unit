# 2021 Operating System
## Assignment #1 : KU_MMU
- Support address translation for CPU
- Input 파일의 형식은 (pid) (virtual address)
- 각 프로세스별로 적절한 page directory를 할당 후 관리가 필요

### PCB
- pid
- page directory의 pfn  

새로운 프로세스가 실행될 때마다 physical memory에 저장

### free list
- physical memory free list
- swap space free list

두 경우 모두 구조는 queue로 현재 비어있는 곳의 pfn을 저장  
이 프로그램의 경우 프로세스는 항상 한 주소만 요구하므로 free list를 위한 별도의 특별한 자료구조는 필요하지 않음  
따라서 간단히 queue로 구현

### swap out
- page replacement : FIFO
- PD, PMD, PT는 swap 되지 않음  

 swap 가능 리스트의 구조는 queue로 할당된 페이지의 pfn을 저장
