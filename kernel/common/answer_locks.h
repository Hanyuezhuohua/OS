//
// Created by Wenxin Zheng on 2021/4/21.
//

#ifndef ACMOS_SPR21_ANSWER_LOCKS_H
#define ACMOS_SPR21_ANSWER_LOCKS_H


int lock_init(struct lock *lock){
    lock->locked = 0; lock->cpuid = 0;
    if(nlock >= MAXLOCKS) BUG("Max lock count reached.");
    locks[nlock++] = lock;
    return 0;
}

void acquire(struct lock *lock){
    while (!__sync_bool_compare_and_swap(&lock->locked, 0, 1));
    lock->cpuid = cpuid() + 1;
}

// Try to acquire the lock once
// Return 0 if succeed, -1 if failed.
int try_acquire(struct lock *lock){
    if (__sync_bool_compare_and_swap(&lock->locked, 0, 1)) {
    	lock->cpuid = cpuid() + 1;
    	return 0;
    }
    return -1;
}

void release(struct lock* lock){
	if (holding_lock(lock)) {
		lock->cpuid = 0;
		__sync_lock_release(&lock->locked);
	}
}

int is_locked(struct lock* lock){
    return lock->locked;
}

// private for spin lock
int holding_lock(struct lock* lock){
    if (lock->locked && lock->cpuid == cpuid() + 1) return 1;
	else return 0;
}

#endif  // ACMOS_SPR21_ANSWER_LOCKS_H