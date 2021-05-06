#define PAGE_SIZE 4

// physical memory와 swap space의 주소값, 한 칸 당 페이지 하나, 페이지는 1byte가 4개
char *ku_mmu_pmem, *ku_mmu_swap;
unsigned int ku_mmu_pmem_size, ku_mmu_swap_size;
// 현재까지 생성된 프로세스 개수
int ku_mmu_pcount = 0;
// pid 저장 배열의 현재 최대 크기, realloc 위함
int ku_mmu_psize = 10;
// 현재까지 생성된 pid들 저장
char *ku_mmu_pids;
// 각 pid별 pdbr 저장
char **ku_mmu_pdbrs;

// PFN, Swap Space Offset Mask
const char PFN_MASK=0xFC;
const char SWAP_MASK=0xFE;

typedef struct queue
{
    int front; // queue의 시작 인덱스
    int back;  // queue의 값이 들어갈 인덱스
    int size;  // queue의 크기
    int count; // 들어가있는 원소 개수
    char *base; // 저장 공간 배열 주소
} queue;

// Free list
queue ku_mmu_pfree = {0, 0, 0, 0, NULL};
queue ku_mmu_sfree = {0, 0, 0, 0, NULL};
// For page replacement(FIFO), store page order
queue ku_mmu_pte_orders = {0, 0, 0, 0, NULL};

int pop(queue *q)
{
    if (q->count == 0)
        return -1;
    int r = (q->base)[q->front]; // return값
    q->front = q->front + 1;
    q->count = q->count - 1;
    if (q->front == q->size)
        q->front = 0;
    return r;
}
char push(queue *q, int num)
{
    if (q->size == q->count)
        return -1;
    q->base[q->back] = num;
    q->back = q->back + 1;
    q->count = q->count + 1;
    if (q->back == q->size)
        q->back = 0;
    return 0;
}

void *ku_mmu_init(unsigned int pmem_size, unsigned int swap_size)
{
    if (pmem_size > 256)
    {
        // PFN이 6bits이므로 그 이상 사용 불가
        pmem_size = 256;
    }
    if (swap_size > 512)
    {
        // Swap space offset이 7bits이므로 그 이상 사용 불가
        swap_size = 512;
    }

    ku_mmu_pmem_size=pmem_size; ku_mmu_swap_size=swap_size;

    // 1바이트 만큼씩 할당하고 모두 0으로 초기화
    ku_mmu_pmem = calloc(pmem_size, 1);
    ku_mmu_swap = calloc(swap_size, 1);
    //printf("memory base address: %p, swap base address: %p\n",ku_mmu_pmem,ku_mmu_swap);

    // 사실상 쓸 수 있는 공간
    // 페이지 크기가 4바이트이므로, 4로 나눈 나머지는 쓸 수 없다.
    pmem_size=pmem_size-(pmem_size%4);
    swap_size=swap_size-(swap_size%4);

    // 사용된 page 순서 저장할 배열 할당 (for page replacement)
    ku_mmu_pte_orders.size = pmem_size/4;
    ku_mmu_pte_orders.base = malloc(ku_mmu_pte_orders.size);

    // pid를 저장할 배열 할당
    ku_mmu_pids = malloc(sizeof(char) * ku_mmu_psize);
    // pdbr을 저장할 배열 할당
    ku_mmu_pdbrs = malloc(sizeof(char*) * ku_mmu_psize);

    // free list -> free인 index 저장됨
    ku_mmu_pfree.size = pmem_size / 4;
    ku_mmu_sfree.size = swap_size/ 4;
    ku_mmu_pfree.base = malloc(ku_mmu_pfree.size);
    ku_mmu_sfree.base = malloc(ku_mmu_sfree.size);

    // Update free list
    for (int i = 0; i < pmem_size / 4; i++)
    {
        int r = push(&ku_mmu_pfree, i);
        if (r == -1)
        {
            return NULL;
        }
    }

    // swap space의 0번 페이지는 쓰이지 않음
    for (int i = 1; i < swap_size / 4; i++)
    {
        int r = push(&ku_mmu_sfree, i);
        if (r == -1)
        {
            return NULL;
        }
    }
    return ku_mmu_pmem;
}

char get_pfn()
{
    if (ku_mmu_pfree.count == 0)
    {
        // swap out 필요
        // swap 불가능
        if (ku_mmu_sfree.count == 0)
        {
            return -1;
        }
        // 메모리에서 swap 할 수 있는 것의 개수
        if (ku_mmu_pte_orders.count == 0)
        {
            return -1;
        }
        // swap 가능 -> Page replacement policy: FIFO
        char pte_offset = pop(&ku_mmu_pte_orders);
        char swap_offset = pop(&ku_mmu_sfree);
        if (pte_offset == -1 || swap_offset == -1)
        {
            return -1;
        }
        // PTE에 저장되어 있는 page의 PFN
        char pfn = ((unsigned char)(*(ku_mmu_pmem + pte_offset)&PFN_MASK) >> 2);

        /************************
        page내용 swap space에 저장
        ************************/

        // swap space에 임의값 저장 (확인 위함)
        *(ku_mmu_swap+swap_offset*PAGE_SIZE)=1;
        // PT에 Swap entry 저장
        *(ku_mmu_pmem + pte_offset) = (swap_offset << 1);
        return pfn;
    }
    else
    {
        char pfn = pop(&ku_mmu_pfree);
        if (pfn == -1)
        {
            return -1;
        }
        return pfn;
    }
    return -1;
}

int ku_run_proc(char pid, struct ku_pte **ku_cr3)
{
    // pid 생성 여부 확인
    for (int i = 0; i < ku_mmu_pcount; i++)
    {
        if (pid == ku_mmu_pids[i])
        {
            // 이미 실행됐던 프로세스
            *ku_cr3 = ku_mmu_pdbrs[i];
            return 0;
        }
    }

    // 새로 프로세스 생성
    // va 0번에 PCB 저장하기 위해 PD, PDM, PT 0번 각각 생성후 페이지에 PCB 정보를 저장한다.
    // PCB는 각각 char pid, pd의 PFN 정보를 갖고 있다.
    // 따라서 기본적으로 페이지 4개를 할당받아야함

    // 새로운 process 생성, pid와 pdbr 배열에 넣어줌
    // 0번에 PCB 저장
    ku_mmu_pids[ku_mmu_pcount] = pid;

    // page directory 생성
    char pd_pfn = get_pfn();
    if (pd_pfn == -1)
    {
        return -1;
    }
    ku_mmu_pdbrs[ku_mmu_pcount] =ku_mmu_pmem+pd_pfn*PAGE_SIZE;
    *ku_cr3 = ku_mmu_pdbrs[ku_mmu_pcount];
    // 0번에 page middle directory 생성, pde update
    char pmd_pfn = get_pfn();
    if (pmd_pfn == -1)
    {
        return -1;
    }
    ku_mmu_pmem[pd_pfn*PAGE_SIZE] = (pmd_pfn << 2) + 1;
    // 0번에 page table 생성, pmde update
    char pt_pfn = get_pfn();
    if (pt_pfn == -1)
    {
        return -1;
    }
    ku_mmu_pmem[pmd_pfn*PAGE_SIZE] = (pt_pfn << 2) + 1;
    // 0번에 page -> PCB 저장, pte update
    char pg_pfn = get_pfn();
    if (pg_pfn == -1)
    {
        return -1;
    }
    ku_mmu_pmem[pt_pfn*PAGE_SIZE] = (pg_pfn << 2) + 1;

    // PCB 업뎃 (0번 : pid, 1번 : pd의 pfn)
    ku_mmu_pmem[pg_pfn*PAGE_SIZE] = pid;
    ku_mmu_pmem[pg_pfn*PAGE_SIZE + 1] = pd_pfn;

    // 생성된 process 개수
    ku_mmu_pcount++;
    if (ku_mmu_pcount == ku_mmu_psize)
    {
        // pid와 pdbr 저장하는 배열의 크기 재할당
        ku_mmu_psize *= 2;
        // 안전하게 재할당하기 위하여 값 확인 필요
        char *temp_pids = realloc(ku_mmu_pids, sizeof(char) * ku_mmu_psize);
        char **temp_pdbrs = realloc(ku_mmu_pdbrs, sizeof(char* ) * ku_mmu_psize);
        if (temp_pids == NULL || temp_pdbrs == NULL)
        {
            return -1;
        }
        ku_mmu_pids = temp_pids;
        ku_mmu_pdbrs = temp_pdbrs;
    }

    return 0;
}

int ku_page_fault(char pid, char va)
{
    // pdbr 찾기 위해 몇번에 저장되어 있는 pid인지 search
    int index = -1;
    for (int i = 0; i < ku_mmu_pcount; i++)
    {
        if (ku_mmu_pids[i] == pid)
        {
            index = i;
            break;
        }
    }
    if (index == -1)
    {
        return -1;
    }
    // process의 pdbr
    char* pdbr = ku_mmu_pdbrs[index];
    // 각각 pd, pmd, pt offset
    char pd_offset = (unsigned char)(va & 0xC0) >> 6;
    char pmd_offset = (va & 0x30) >> 4;
    char pt_offset = (va & 0x0C)>>2;

    char pde = *(pdbr + pd_offset);

    // pmd는 swap out 안되므로, swap out은 안 되어있음
    if (pde == 0)
    { // page middle directory 새로 할당
        char pmd_pfn = get_pfn();
        if (pmd_pfn == -1)
            return -1;
        pde = (pmd_pfn << 2) + 1;
        *(pdbr+pd_offset)=pde;
    }

    char pmd_pfn = (unsigned char)(pde&PFN_MASK) >> 2;
    char pmde = *(ku_mmu_pmem + pmd_pfn*PAGE_SIZE + pmd_offset);

    // pt는 swap out 안되므로, swap out은 안 되어있음
    if (pmde == 0)
    { //page table 새로 할당
        char pt_pfn = get_pfn();
        if (pt_pfn == -1)
            return -1;
        pmde = (pt_pfn << 2) + 1;
        *(ku_mmu_pmem + pmd_pfn*PAGE_SIZE + pmd_offset)=pmde;
    }
    char pt_pfn = (unsigned char)(pmde&PFN_MASK) >> 2;
    char pte = *(ku_mmu_pmem + pt_pfn*PAGE_SIZE + pt_offset);

    // page는 swap out 가능
    // present 상태는 아님(page fault가 났으므로)
    // swap out인지 demand paging이 필요한지
    
    if (pte != 0)
    {   
        if((pte&1)==1){
            //present bit이 1임 -> page fault 났으면 안됨
            return 0;
        }
        // swap in 필요
        char swap_offset = (unsigned char)(pte & SWAP_MASK) >> 1;

        /***********************
        ...page 내용 가져오기...
        ************************/

        // swap space의 내용은 지워주기
        *(ku_mmu_swap+swap_offset*PAGE_SIZE)=0;
    }

    // 새로운 페이지 할당
    char pg_pfn = get_pfn();
    if (pg_pfn == -1)
        return -1;
    // pte 업뎃
    *(ku_mmu_pmem + pt_pfn*PAGE_SIZE + pt_offset) = (pg_pfn << 2) + 1;

    // For page replacement
    push(&ku_mmu_pte_orders, pt_pfn*PAGE_SIZE + pt_offset);

    return 0;
}