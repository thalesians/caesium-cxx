/**
 * Copyright (c) 2012-2014, Stephen Blackheath and Anthony Jones
 * Released under a BSD3 licence.
 *
 * C++ implementation courtesy of International Telematics Ltd.
 */
#ifndef _SODIUM_LOCKPOOL_H_
#define _SODIUM_LOCKPOOL_H_

#include <sodium/config.h>

#ifdef __APPLE__
#include <os/lock.h>
#elif defined(__TI_COMPILER_VERSION__)
#else
#include <mutex>
#endif
#include <stdint.h>
#include <limits.h>

namespace sodium {
    namespace impl {
        struct spin_lock {
#if defined(SODIUM_SINGLE_THREADED)
            inline void lock() {}
            inline void unlock() {}
#elif defined(__APPLE__)
            os_unfair_lock sl;
            spin_lock() : sl(OS_UNFAIR_LOCK_INIT) {
            }
            inline void lock() {
                os_unfair_lock_lock(&sl);
            }
            inline void unlock() {
                os_unfair_lock_unlock(&sl);
            }
#elif defined(HAVE_PTHREAD_SPIN_LOCK)
            bool initialized;
            pthread_spinlock_t sl;
            spin_lock() : initialized(true) {
                pthread_spin_init(&sl, PTHREAD_PROCESS_PRIVATE);
            }
            inline void lock() {
                // Make sure nothing bad happens if this is called before the constructor.
                // This can happen during static initialization if data structures that use
                // this lock pool are declared statically.
                if (initialized) pthread_spin_lock(&sl);
            }
            inline void unlock() {
                if (initialized) pthread_spin_unlock(&sl);
            }
#else
            bool initialized;
            std::mutex m;
            spin_lock() : initialized(true) {
            }
            inline void lock() {
                // Make sure nothing bad happens if this is called before the constructor.
                // This can happen during static initialization if data structures that use
                // this lock pool are declared statically.
                if (initialized) m.lock();
            }
            inline void unlock() {
                if (initialized) m.unlock();
            }
#endif
        };
		#if defined(SODIUM_SINGLE_THREADED)
			#define SODIUM_IMPL_LOCK_POOL_BITS 1
		#else
			#define SODIUM_IMPL_LOCK_POOL_BITS 7
        #endif
        extern spin_lock lock_pool[1<<SODIUM_IMPL_LOCK_POOL_BITS];

        // Use Knuth's integer hash function ("The Art of Computer Programming", section 6.4)
        inline spin_lock* spin_get_and_lock(void* addr)
        {
#if defined(SODIUM_SINGLE_THREADED)
        	return &lock_pool[0];
#else
            spin_lock* l = &lock_pool[(uint32_t)((uint32_t)
                (uintptr_t)(addr)
                * (uint32_t)2654435761U) >> (32 - SODIUM_IMPL_LOCK_POOL_BITS)];
            l->lock();
            return l;
#endif
        }
    }
}

#endif
