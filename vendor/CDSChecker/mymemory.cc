#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <string.h>
#include <new>

#include "mymemory.h"
#include "snapshot.h"
#include "common.h"
#include "threads-model.h"
#include "model.h"

#define REQUESTS_BEFORE_ALLOC 1024

size_t allocatedReqs[REQUESTS_BEFORE_ALLOC] = { 0 };
int nextRequest = 0;
int howManyFreed = 0;
int switch_alloc = 0;
#if !USE_MPROTECT_SNAPSHOT
static mspace sStaticSpace = NULL;
#endif

/** Non-snapshotting calloc for our use. */
void *model_calloc(size_t count, size_t size)
{
#if USE_MPROTECT_SNAPSHOT
	static void *(*callocp)(size_t count, size_t size) = NULL;
	char *error;
	void *ptr;

	/* get address of libc malloc */
	if (!callocp) {
		callocp = (void * (*)(size_t, size_t))dlsym(RTLD_NEXT, "calloc");
		if ((error = dlerror()) != NULL) {
			fputs(error, stderr);
			exit(EXIT_FAILURE);
		}
	}
	ptr = callocp(count, size);
	return ptr;
#else
	if (!sStaticSpace)
		sStaticSpace = create_shared_mspace();
	return mspace_calloc(sStaticSpace, count, size);
#endif
}

/** Non-snapshotting malloc for our use. */
void *model_malloc(size_t size)
{
#if USE_MPROTECT_SNAPSHOT
	static void *(*mallocp)(size_t size) = NULL;
	char *error;
	void *ptr;

	/* get address of libc malloc */
	if (!mallocp) {
		mallocp = (void * (*)(size_t))dlsym(RTLD_NEXT, "malloc");
		if ((error = dlerror()) != NULL) {
			fputs(error, stderr);
			exit(EXIT_FAILURE);
		}
	}
	ptr = mallocp(size);
	return ptr;
#else
	if (!sStaticSpace)
		sStaticSpace = create_shared_mspace();
	return mspace_malloc(sStaticSpace, size);
#endif
}

/** @brief Snapshotting malloc, for use by model-checker (not user progs) */
void * snapshot_malloc(size_t size)
{
	void *tmp = mspace_malloc(model_snapshot_space, size);
	ASSERT(tmp);
	return tmp;
}

/** @brief Snapshotting calloc, for use by model-checker (not user progs) */
void * snapshot_calloc(size_t count, size_t size)
{
	void *tmp = mspace_calloc(model_snapshot_space, count, size);
	ASSERT(tmp);
	return tmp;
}

/** @brief Snapshotting realloc, for use by model-checker (not user progs) */
void *snapshot_realloc(void *ptr, size_t size)
{
	void *tmp = mspace_realloc(model_snapshot_space, ptr, size);
	ASSERT(tmp);
	return tmp;
}

/** @brief Snapshotting free, for use by model-checker (not user progs) */
void snapshot_free(void *ptr)
{
	mspace_free(model_snapshot_space, ptr);
}

/** Non-snapshotting free for our use. */
void model_free(void *ptr)
{
#if USE_MPROTECT_SNAPSHOT
	static void (*freep)(void *);
	char *error;

	/* get address of libc free */
	if (!freep) {
		freep = (void (*)(void *))dlsym(RTLD_NEXT, "free");
		if ((error = dlerror()) != NULL) {
			fputs(error, stderr);
			exit(EXIT_FAILURE);
		}
	}
	freep(ptr);
#else
	mspace_free(sStaticSpace, ptr);
#endif
}

/** Bootstrap allocation. Problem is that the dynamic linker calls require
 *  calloc to work and calloc requires the dynamic linker to work. */

#define BOOTSTRAPBYTES 131072
char bootstrapmemory[BOOTSTRAPBYTES];
size_t offset = 0;

void * HandleEarlyAllocationRequest(size_t sz)
{
	/* Align to 8 byte boundary */
	sz = (sz + 7) & ~7;

	if (sz > (BOOTSTRAPBYTES-offset)) {
		model_print("OUT OF BOOTSTRAP MEMORY.  Increase the size of BOOTSTRAPBYTES in mymemory.cc\n");
		exit(EXIT_FAILURE);
	}

	void *pointer = (void *)&bootstrapmemory[offset];
	offset += sz;
	return pointer;
}

/** @brief Global mspace reference for the model-checker's snapshotting heap */
mspace model_snapshot_space = NULL;

#if USE_MPROTECT_SNAPSHOT

/** @brief Global mspace reference for the user's snapshotting heap */
mspace user_snapshot_space = NULL;

/** Check whether this is bootstrapped memory that we should not free */
static bool DontFree(void *ptr)
{
	return (ptr >= (&bootstrapmemory[0]) && ptr < (&bootstrapmemory[BOOTSTRAPBYTES]));
}

/**
 * @brief The allocator function for "user" allocation
 *
 * Should only be used for allocations which will not disturb the allocation
 * patterns of a user thread.
 */
static void * user_malloc(size_t size)
{
	void *tmp = mspace_malloc(user_snapshot_space, size);
	ASSERT(tmp);
	return tmp;
}

/**
 * @brief Snapshotting malloc implementation for user programs
 *
 * Do NOT call this function from a model-checker context. Doing so may disrupt
 * the allocation patterns of a user thread.
 */
void *malloc(size_t size)
{
	if (user_snapshot_space) {
		if (switch_alloc) {
			return model_malloc(size);
		}
		/* Only perform user allocations from user context */
		ASSERT(!model || thread_current());
		return user_malloc(size);
	} else
		return HandleEarlyAllocationRequest(size);
}

/** @brief Snapshotting free implementation for user programs */
void free(void * ptr)
{
	if (!DontFree(ptr)) {
		if (switch_alloc) {
			return model_free(ptr);
		}
		mspace_free(user_snapshot_space, ptr);
	}
}

/** @brief Snapshotting realloc implementation for user programs */
void *realloc(void *ptr, size_t size)
{
	void *tmp = mspace_realloc(user_snapshot_space, ptr, size);
	ASSERT(tmp);
	return tmp;
}

/** @brief Snapshotting calloc implementation for user programs */
void * calloc(size_t num, size_t size)
{
	if (user_snapshot_space) {
		void *tmp = mspace_calloc(user_snapshot_space, num, size);
		ASSERT(tmp);
		return tmp;
	} else {
		void *tmp = HandleEarlyAllocationRequest(size * num);
		memset(tmp, 0, size * num);
		return tmp;
	}
}

/** @brief Snapshotting allocation function for use by the Thread class only */
void * Thread_malloc(size_t size)
{
	return user_malloc(size);
}

/** @brief Snapshotting free function for use by the Thread class only */
void Thread_free(void *ptr)
{
	free(ptr);
}

/** @brief Snapshotting new operator for user programs */
void * operator new(size_t size) throw(std::bad_alloc)
{
	return malloc(size);
}

/** @brief Snapshotting delete operator for user programs */
void operator delete(void *p) throw()
{
	free(p);
}

/** @brief Snapshotting new[] operator for user programs */
void * operator new[](size_t size) throw(std::bad_alloc)
{
	return malloc(size);
}

/** @brief Snapshotting delete[] operator for user programs */
void operator delete[](void *p, size_t size)
{
	free(p);
}

#else /* !USE_MPROTECT_SNAPSHOT */

/** @brief Snapshotting allocation function for use by the Thread class only */
void * Thread_malloc(size_t size)
{
	return malloc(size);
}

/** @brief Snapshotting free function for use by the Thread class only */
void Thread_free(void *ptr)
{
	free(ptr);
}

#endif /* !USE_MPROTECT_SNAPSHOT */
