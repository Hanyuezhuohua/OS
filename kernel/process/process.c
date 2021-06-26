#include "process.h"
#include "lock.h"
#include "pagetable.h"
#include "elf.h"
#include "memlayout.h"
#include "printk.h"

#define MYKSTACK(p) (TRAMPOLINE - ((p) + 1) * (NTHREAD + 1) * PGSIZE)
#define PFLAGS2PTEFLAGS(PF)                                         \
	(((PF)&PF_X ? PTE_X : 0) | ((PF)&PF_W ? PTE_W : 0) | \
	 ((PF)&PF_R ? PTE_R : 0))

extern const char binary_putc_start;
extern char trampoline[], usertrap1[], usertrap2[];
extern char endTextSect[];
extern void swtch(context_t *old, context_t *new);
extern void usertrap(void);
extern void usertrapret();
extern void my_ret(void);
extern pagetable_t kernel_pagetable;
thread_t *running[NCPU];
process_t process[NPROC];
context_t contexts[NCPU];
struct list_head sched_list[NCPU];
struct lock pidlock, tidlock, schedlock;
int _pid, _tid;


// 将ELF文件映射到给定页表的地址空间，返回pc的数值
// 关于 ELF 文件，请参考：https://docs.oracle.com/cd/E19683-01/816-1386/chapter6-83432/index.html
static uint64 load_binary(pagetable_t *target_page_table, const char *bin){
	struct elf_file *elf;
    int i;
    uint64 seg_sz, p_vaddr, seg_map_sz, file_sz;
	elf = elf_parse_file(bin);

	/* load each segment in the elf binary */
	for (i = 0; i < elf->header.e_phnum; ++i) {
		if (elf->p_headers[i].p_type == PT_LOAD) {
            // 根据 ELF 文件格式做段映射
            // 从ELF中获得这一段的段大小
            seg_sz = elf->p_headers[i].p_memsz;
            // 对应段的在内存中的虚拟地址
            p_vaddr = elf->p_headers[i].p_vaddr;
            // 对映射大小做页对齐
			seg_map_sz = ROUNDUP(seg_sz + p_vaddr, PGSIZE) - PGROUNDDOWN(p_vaddr);
            // 接下来代码的期望目的：将程序代码映射/复制到对应的内存空间
            // 一种可能的实现如下：
            /* 
             * 在 target_page_table 中分配一块大小
             * 通过 memcpy 将某一段复制进入这一块空间
             * 页表映射修改
             */
            file_sz = elf->p_headers[i].p_filesz;
            for(uint64 offset = 0; offset < seg_map_sz; offset += PGSIZE){
                uint64 pa = (uint64)mm_kalloc();
                if(pa == 0){
                    BUG("elf load alloc fail!");
                }
                memset((void*)pa, 0, PGSIZE);
                if(offset + PGSIZE == seg_map_sz){
                    memcpy((void *)pa, (void*)(((uint64)bin) + elf->p_headers[i].p_offset + offset), file_sz - offset);
                }
                else memcpy((void *)pa, (void*)(((uint64)bin) + elf->p_headers[i].p_offset + offset), PGSIZE);
                if(pt_map_pages(*target_page_table, p_vaddr + offset, pa, PGSIZE, PFLAGS2PTEFLAGS(elf->p_headers[i].p_flags) | PTE_U) < 0){
                    BUG("elf load map fail!");
                } 
            }
		}
	}
	/* PC: the entry point */
	return elf->header.e_entry;
}

/* 分配一个进程，需要至少完成以下目标：
 * 
 * 分配一个主线程
 * 创建一张进程页表
 * 分配pid、tid
 * 设置初始化线程上下文
 * 设置初始化线程返回地址寄存器ra，栈寄存器sp
 * 
 * 这个函数传入参数为一个二进制的代码和一个线程指针(具体传入规则可以自己修改)
 * 此外程序首次进入用户态之前，应该设置好trap处理向量为usertrap（或者你自定义的）
 */
process_t *alloc_proc(const char* bin, thread_t *thr){
    process_t *p;
    for(p = process; p < &process[NPROC]; p++){
        acquire(&p->process_lock);
        if(p->process_state == UNUSED){
            thr = &(p->threads[0]);

            lock_init(&thr->thread_lock);
            acquire(&thr->thread_lock);
            init_list_head(&thr->process_list_thread_node);
            list_append(&thr->process_list_thread_node, &p->thread_list);
            thr->trapframe = (trapframe_t *)mm_kalloc();
            if(thr->trapframe == 0) BUG("alloc trapframe fail!");
            thr->kstack = p->kstack; 

            uint64 kstack_pa = (uint64)mm_kalloc();
            if(kstack_pa == 0) BUG("alloc kstack_pa fail!");
            if(pt_map_pages(kernel_pagetable, thr->kstack, kstack_pa, PGSIZE, PTE_R | PTE_W) < 0){
                BUG("kernel_page map kstack fail!");
            }

            p->pagetable = (pagetable_t)mm_kalloc();
            if(p->pagetable == 0) BUG("alloc process pagetable fail!");
            memset(p->pagetable, 0, PGSIZE);
            if(pt_map_pages(p->pagetable, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X) < 0){
                BUG("process pagetable map trampoline fail!");
            }           
            if(pt_map_pages(p->pagetable, TRAPFRAME, (uint64)(thr->trapframe), PGSIZE, PTE_R | PTE_W) < 0){
                BUG("process pagetable map trapframe fail!");
            } //if we have another thread, map to TRAPFRAME - PGSIZE, because we only have one thread here!
            if(pt_map_pages(p->pagetable, thr->kstack, kstack_pa, PGSIZE, PTE_R | PTE_W) < 0){
                BUG("process pagetable map kstack fail!");
            }       
            acquire(&pidlock);
            p->pid = _pid++;
            release(&pidlock);
            
            acquire(&tidlock);
            thr->tid = _tid++;
            release(&tidlock);

            thr->context.sp = thr->kstack + PGSIZE;
            thr->context.ra = (uint64)my_ret;

            thr->trapframe->epc = load_binary(&p->pagetable, bin);
            thr->trapframe->sp = 10 * PGSIZE;

            uint64 ustack_pa = (uint64)mm_kalloc();
            if(ustack_pa == 0) BUG("alloc ustack fail!");
            if(pt_map_pages(p->pagetable, thr->trapframe->sp - PGSIZE, ustack_pa, PGSIZE, PTE_R | PTE_W | PTE_U) < 0){
                BUG("process pagetable map ustack fail!");                
            }

            p->process_state = RUNNABLE;
            thr->thread_state = RUNNABLE;
            release(&thr->thread_lock);
            release(&p->process_lock);
            return p;
        }
        release(&p->process_lock);
    }
    return NULL;
}

bool load_thread(file_type_t type){
    if(type == PUTC){
        thread_t *t = NULL;
        process_t *p = alloc_proc(&binary_putc_start, t);
        t = &p->threads[0];
        init_list_head(&t->sched_list_thread_node);
        if(!t) return false;
        sched_enqueue(t);
    } else {
        BUG("Not supported");
    }
    return true; 
}

// sched_enqueue和sched_dequeue的主要任务是加入一个任务到队列中和删除一个任务
// 这两个函数的展示了如何使用list.h中可的函数（加入、删除、判断空、取元素）
// 具体可以参考：Stackoverflow上的回答
// https://stackoverflow.com/questions/15832301/understanding-container-of-macro-in-the-linux-kernel
void sched_enqueue(thread_t *target_thread){
    if(target_thread->thread_state == RUNNING) BUG("Running Thread cannot be scheduled.");
    list_add(&target_thread->sched_list_thread_node, &(sched_list[cpuid()]));
}

thread_t *sched_dequeue(){
    if(list_empty(&(sched_list[cpuid()]))) BUG("Scheduler List is empty");
    thread_t *head = container_of(sched_list[cpuid()].next, thread_t, sched_list_thread_node);
    list_del(&head->sched_list_thread_node);
    return head;
}

bool sched_empty(){
    return list_empty(&(sched_list[cpuid()]));
}

// 开始运行某个特定的函数
void thread_run(thread_t *target){
    acquire(&target->thread_lock);
    if(target->thread_state == RUNNABLE){
        target->thread_state = RUNNING;
        running[cpuid()] = target;
        swtch(&contexts[cpuid()], &target->context);        
        //after yield
        running[cpuid()] = 0;
    }
    release(&target->thread_lock);
}

// sched_start函数启动调度，按照调度的队列开始运行。
void sched_start(){
    while(1){
        if(sched_empty()) BUG("Scheduler list empty, no app loaded");
        thread_t *next = sched_dequeue();
        thread_run(next);
    }
}

void sched_init(){
    // 初始化调度队列锁
    lock_init(&schedlock);
    // 初始化队列头
    init_list_head(&(sched_list[cpuid()]));
}

void sched_yield(void){
    thread_t *t = thread_get();
    acquire(&t->thread_lock);
    t->thread_state = RUNNABLE;
    sched_enqueue(t);
    swtch(&t->context, &contexts[cpuid()]);
    release(&t->thread_lock);
}

void sched_exit(int value){
    thread_t *t = thread_get();
    acquire(&t->thread_lock);
    t->thread_state = ZOMBIE;
    t->exit_state = value;
    swtch(&t->context, &contexts[cpuid()]);
    release(&t->thread_lock);
}

void proc_init(){
    // 初始化pid、tid锁
    lock_init(&pidlock);
    lock_init(&tidlock);
    // 接下来代码期望的目的：映射第一个用户线程并且插入调度队列
    for(process_t *p = process; p < &process[NPROC]; p++){
        lock_init(&p->process_lock);
        init_list_head(&p->thread_list);
        p->kstack = MYKSTACK((int) (p - process));
        p->process_state = UNUSED;
    }
    if(!load_thread(PUTC)) BUG("Load failed");
}

void my_ret(void){
    thread_t *t = thread_get();
    release(&t->thread_lock);
    usertrapret();
}

thread_t* thread_get(){
    return running[cpuid()];
}


