//
// Created by Wenxin Zheng on 2021/4/21.
//

#ifndef ACMOS_SPR21_ANSWER_LOCKS_H
#define ACMOS_SPR21_ANSWER_LOCKS_H


int lock_init(struct lock *lock){
    /* Your code here */
    lock->cpuid = 0;
    __sync_lock_release(&lock->locked);
    if(nlock >= MAXLOCKS) BUG("Max lock count reached.");
    locks[nlock++] = lock;
    return 0;
}

void acquire(struct lock *lock){
    /* Your code here */
    if(holding_lock(lock)) return;
    while(__builtin_expect(!!(__sync_lock_test_and_set(&lock->locked, 1)), 0)) ;
    __sync_synchronize();
    lock->cpuid = cpuid() + 1;
}

// Try to acquire the lock once
// Return 0 if succeed, -1 if failed.
int try_acquire(struct lock *lock){
    /* Your code here */
    if(!__sync_lock_test_and_set(&lock->locked, 1)){
        __sync_synchronize();
    	lock->cpuid = cpuid() + 1;
    	return 0;
    }
    return -1;
}

void release(struct lock* lock){
    /* Your code here */
    if(!holding_lock(lock)) return;
    lock->cpuid = 0;
    __sync_synchronize();
    __sync_lock_release(&(lock->locked));
}

int is_locked(struct lock* lock){
    return lock->locked;
}

// private for spin lock
int holding_lock(struct lock* lock){
    /* Your code here */
    if(lock->locked == 1 && lock->cpuid == cpuid() + 1) return 1;
    return 0;
}

#endif  // ACMOS_SPR21_ANSWER_LOCKS_H