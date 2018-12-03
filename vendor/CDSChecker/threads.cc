/** @file threads.cc
 *  @brief Thread functions.
 */

#include <string.h>

#include <threads.h>
#include <mutex>
#include "common.h"
#include "threads-model.h"
#include "action.h"

/* global "model" object */
#include "model.h"

/** Allocate a stack for a new thread. */
static void * stack_allocate(size_t size)
{
	return Thread_malloc(size);
}

/** Free a stack for a terminated thread. */
static void stack_free(void *stack)
{
	Thread_free(stack);
}

/**
 * @brief Get the current Thread
 *
 * Must be called from a user context
 *
 * @return The currently executing thread
 */
Thread * thread_current(void)
{
	ASSERT(model);
	return model->get_current_thread();
}

/**
 * Provides a startup wrapper for each thread, allowing some initial
 * model-checking data to be recorded. This method also gets around makecontext
 * not being 64-bit clean
 */
void thread_startup()
{
	Thread * curr_thread = thread_current();

	/* Add dummy "start" action, just to create a first clock vector */
	model->switch_to_master(new ModelAction(THREAD_START, std::memory_order_seq_cst, curr_thread));

	/* Call the actual thread function */
	curr_thread->start_routine(curr_thread->arg);

	/* Finish thread properly */
	model->switch_to_master(new ModelAction(THREAD_FINISH, std::memory_order_seq_cst, curr_thread));
}

/**
 * Create a thread context for a new thread so we can use
 * setcontext/getcontext/swapcontext to swap it out.
 * @return 0 on success; otherwise, non-zero error condition
 */
int Thread::create_context()
{
	int ret;

	ret = getcontext(&context);
	if (ret)
		return ret;

	/* Initialize new managed context */
	stack = stack_allocate(STACK_SIZE);
	context.uc_stack.ss_sp = stack;
	context.uc_stack.ss_size = STACK_SIZE;
	context.uc_stack.ss_flags = 0;
	context.uc_link = model->get_system_context();
	makecontext(&context, thread_startup, 0);

	return 0;
}

/**
 * Swaps the current context to another thread of execution. This form switches
 * from a user Thread to a system context.
 * @param t Thread representing the currently-running thread. The current
 * context is saved here.
 * @param ctxt Context to which we will swap. Must hold a valid system context.
 * @return Does not return, unless we return to Thread t's context. See
 * swapcontext(3) (returns 0 for success, -1 for failure).
 */
int Thread::swap(Thread *t, ucontext_t *ctxt)
{
	t->set_state(THREAD_READY);
	return model_swapcontext(&t->context, ctxt);
}

/**
 * Swaps the current context to another thread of execution. This form switches
 * from a system context to a user Thread.
 * @param ctxt System context variable to which to save the current context.
 * @param t Thread to which we will swap. Must hold a valid user context.
 * @return Does not return, unless we return to the system context (ctxt). See
 * swapcontext(3) (returns 0 for success, -1 for failure).
 */
int Thread::swap(ucontext_t *ctxt, Thread *t)
{
	t->set_state(THREAD_RUNNING);
	return model_swapcontext(ctxt, &t->context);
}


/** Terminate a thread and free its stack. */
void Thread::complete()
{
	ASSERT(!is_complete());
	DEBUG("completed thread %d\n", id_to_int(get_id()));
	state = THREAD_COMPLETED;
	if (stack)
		stack_free(stack);
}

/**
 * @brief Construct a new model-checker Thread
 *
 * A model-checker Thread is used for accounting purposes only. It will never
 * have its own stack, and it should never be inserted into the Scheduler.
 *
 * @param tid The thread ID to assign
 */
Thread::Thread(thread_id_t tid) :
	parent(NULL),
	creation(NULL),
	pending(NULL),
	start_routine(NULL),
	arg(NULL),
	stack(NULL),
	user_thread(NULL),
	id(tid),
	state(THREAD_READY), /* Thread is always ready? */
	last_action_val(0),
	model_thread(true)
{
	memset(&context, 0, sizeof(context));
}

/**
 * Construct a new thread.
 * @param t The thread identifier of the newly created thread.
 * @param func The function that the thread will call.
 * @param a The parameter to pass to this function.
 */
Thread::Thread(thread_id_t tid, thrd_t *t, void (*func)(void *), void *a, Thread *parent) :
	parent(parent),
	creation(NULL),
	pending(NULL),
	start_routine(func),
	arg(a),
	user_thread(t),
	id(tid),
	state(THREAD_CREATED),
	last_action_val(VALUE_NONE),
	model_thread(false)
{
	int ret;

	/* Initialize state */
	ret = create_context();
	if (ret)
		model_print("Error in create_context\n");

	user_thread->priv = this;
}

/** Destructor */
Thread::~Thread()
{
	if (!is_complete())
		complete();
}

/** @return The thread_id_t corresponding to this Thread object. */
thread_id_t Thread::get_id() const
{
	return id;
}

/**
 * Set a thread's THREAD_* state (@see thread_state)
 * @param s The state to enter
 */
void Thread::set_state(thread_state s)
{
	ASSERT(s == THREAD_COMPLETED || state != THREAD_COMPLETED);
	state = s;
}

/**
 * Get the Thread that this Thread is immediately waiting on
 * @return The thread we are waiting on, if any; otherwise NULL
 */
Thread * Thread::waiting_on() const
{
	if (!pending)
		return NULL;

	if (pending->get_type() == THREAD_JOIN)
		return pending->get_thread_operand();
	else if (pending->is_lock())
		return (Thread *)pending->get_mutex()->get_state()->locked;
	return NULL;
}

/**
 * Check if this Thread is waiting (blocking) on a given Thread, directly or
 * indirectly (via a chain of waiting threads)
 *
 * @param t The Thread on which we may be waiting
 * @return True if we are waiting on Thread t; false otherwise
 */
bool Thread::is_waiting_on(const Thread *t) const
{
	Thread *wait;
	for (wait = waiting_on(); wait != NULL; wait = wait->waiting_on())
		if (wait == t)
			return true;
	return false;
}
