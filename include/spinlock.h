
// Minimal spinlock for kernel (not reentrant)
typedef struct { volatile int locked; } spinlock_t;
static inline void spinlock_init(spinlock_t *l) { l->locked = 0; }
static inline void spinlock_acquire(spinlock_t *l) {
    while (__sync_lock_test_and_set(&l->locked, 1)) { while (l->locked); }
}
static inline void spinlock_release(spinlock_t *l) { __sync_lock_release(&l->locked); }
