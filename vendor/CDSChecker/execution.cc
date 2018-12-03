#include <stdio.h>
#include <algorithm>
#include <mutex>
#include <new>
#include <stdarg.h>

#include "model.h"
#include "execution.h"
#include "action.h"
#include "nodestack.h"
#include "schedule.h"
#include "common.h"
#include "clockvector.h"
#include "cyclegraph.h"
#include "promise.h"
#include "datarace.h"
#include "threads-model.h"
#include "bugmessage.h"

#define INITIAL_THREAD_ID	0

/**
 * Structure for holding small ModelChecker members that should be snapshotted
 */
struct model_snapshot_members {
	model_snapshot_members() :
		/* First thread created will have id INITIAL_THREAD_ID */
		next_thread_id(INITIAL_THREAD_ID),
		used_sequence_numbers(0),
		next_backtrack(NULL),
		bugs(),
		failed_promise(false),
		hard_failed_promise(false),
		too_many_reads(false),
		no_valid_reads(false),
		bad_synchronization(false),
		bad_sc_read(false),
		asserted(false)
	{ }

	~model_snapshot_members() {
		for (unsigned int i = 0; i < bugs.size(); i++)
			delete bugs[i];
		bugs.clear();
	}

	unsigned int next_thread_id;
	modelclock_t used_sequence_numbers;
	ModelAction *next_backtrack;
	SnapVector<bug_message *> bugs;
	bool failed_promise;
	bool hard_failed_promise;
	bool too_many_reads;
	bool no_valid_reads;
	/** @brief Incorrectly-ordered synchronization was made */
	bool bad_synchronization;
	bool bad_sc_read;
	bool asserted;

	SNAPSHOTALLOC
};

/** @brief Constructor */
ModelExecution::ModelExecution(ModelChecker *m,
		const struct model_params *params,
		Scheduler *scheduler,
		NodeStack *node_stack) :
	model(m),
	params(params),
	scheduler(scheduler),
	action_trace(),
	thread_map(2), /* We'll always need at least 2 threads */
	obj_map(),
	condvar_waiters_map(),
	obj_thrd_map(),
	promises(),
	futurevalues(),
	pending_rel_seqs(),
	thrd_last_action(1),
	thrd_last_fence_release(),
	node_stack(node_stack),
	priv(new struct model_snapshot_members()),
	mo_graph(new CycleGraph())
{
	/* Initialize a model-checker thread, for special ModelActions */
	model_thread = new Thread(get_next_id());
	add_thread(model_thread);
	scheduler->register_engine(this);
	node_stack->register_engine(this);
}

/** @brief Destructor */
ModelExecution::~ModelExecution()
{
	for (unsigned int i = 0; i < get_num_threads(); i++)
		delete get_thread(int_to_id(i));

	for (unsigned int i = 0; i < promises.size(); i++)
		delete promises[i];

	delete mo_graph;
	delete priv;
}

int ModelExecution::get_execution_number() const
{
	return model->get_execution_number();
}

static action_list_t * get_safe_ptr_action(HashTable<const void *, action_list_t *, uintptr_t, 4> * hash, void * ptr)
{
	action_list_t *tmp = hash->get(ptr);
	if (tmp == NULL) {
		tmp = new action_list_t();
		hash->put(ptr, tmp);
	}
	return tmp;
}

static SnapVector<action_list_t> * get_safe_ptr_vect_action(HashTable<void *, SnapVector<action_list_t> *, uintptr_t, 4> * hash, void * ptr)
{
	SnapVector<action_list_t> *tmp = hash->get(ptr);
	if (tmp == NULL) {
		tmp = new SnapVector<action_list_t>();
		hash->put(ptr, tmp);
	}
	return tmp;
}

action_list_t * ModelExecution::get_actions_on_obj(void * obj, thread_id_t tid) const
{
	SnapVector<action_list_t> *wrv = obj_thrd_map.get(obj);
	if (wrv==NULL)
		return NULL;
	unsigned int thread=id_to_int(tid);
	if (thread < wrv->size())
		return &(*wrv)[thread];
	else
		return NULL;
}

/** @return a thread ID for a new Thread */
thread_id_t ModelExecution::get_next_id()
{
	return priv->next_thread_id++;
}

/** @return the number of user threads created during this execution */
unsigned int ModelExecution::get_num_threads() const
{
	return priv->next_thread_id;
}

/** @return a sequence number for a new ModelAction */
modelclock_t ModelExecution::get_next_seq_num()
{
	return ++priv->used_sequence_numbers;
}

/**
 * @brief Should the current action wake up a given thread?
 *
 * @param curr The current action
 * @param thread The thread that we might wake up
 * @return True, if we should wake up the sleeping thread; false otherwise
 */
bool ModelExecution::should_wake_up(const ModelAction *curr, const Thread *thread) const
{
	const ModelAction *asleep = thread->get_pending();
	/* Don't allow partial RMW to wake anyone up */
	if (curr->is_rmwr())
		return false;
	/* Synchronizing actions may have been backtracked */
	if (asleep->could_synchronize_with(curr))
		return true;
	/* All acquire/release fences and fence-acquire/store-release */
	if (asleep->is_fence() && asleep->is_acquire() && curr->is_release())
		return true;
	/* Fence-release + store can awake load-acquire on the same location */
	if (asleep->is_read() && asleep->is_acquire() && curr->same_var(asleep) && curr->is_write()) {
		ModelAction *fence_release = get_last_fence_release(curr->get_tid());
		if (fence_release && *(get_last_action(thread->get_id())) < *fence_release)
			return true;
	}
	return false;
}

void ModelExecution::wake_up_sleeping_actions(ModelAction *curr)
{
	for (unsigned int i = 0; i < get_num_threads(); i++) {
		Thread *thr = get_thread(int_to_id(i));
		if (scheduler->is_sleep_set(thr)) {
			if (should_wake_up(curr, thr))
				/* Remove this thread from sleep set */
				scheduler->remove_sleep(thr);
		}
	}
}

/** @brief Alert the model-checker that an incorrectly-ordered
 * synchronization was made */
void ModelExecution::set_bad_synchronization()
{
	priv->bad_synchronization = true;
}

/** @brief Alert the model-checker that an incorrectly-ordered
 * synchronization was made */
void ModelExecution::set_bad_sc_read()
{
	priv->bad_sc_read = true;
}

bool ModelExecution::assert_bug(const char *msg)
{
	priv->bugs.push_back(new bug_message(msg));

	if (isfeasibleprefix()) {
		set_assert();
		return true;
	}
	return false;
}

/** @return True, if any bugs have been reported for this execution */
bool ModelExecution::have_bug_reports() const
{
	return priv->bugs.size() != 0;
}

SnapVector<bug_message *> * ModelExecution::get_bugs() const
{
	return &priv->bugs;
}

/**
 * Check whether the current trace has triggered an assertion which should halt
 * its execution.
 *
 * @return True, if the execution should be aborted; false otherwise
 */
bool ModelExecution::has_asserted() const
{
	return priv->asserted;
}

/**
 * Trigger a trace assertion which should cause this execution to be halted.
 * This can be due to a detected bug or due to an infeasibility that should
 * halt ASAP.
 */
void ModelExecution::set_assert()
{
	priv->asserted = true;
}

/**
 * Check if we are in a deadlock. Should only be called at the end of an
 * execution, although it should not give false positives in the middle of an
 * execution (there should be some ENABLED thread).
 *
 * @return True if program is in a deadlock; false otherwise
 */
bool ModelExecution::is_deadlocked() const
{
	bool blocking_threads = false;
	for (unsigned int i = 0; i < get_num_threads(); i++) {
		thread_id_t tid = int_to_id(i);
		if (is_enabled(tid))
			return false;
		Thread *t = get_thread(tid);
		if (!t->is_model_thread() && t->get_pending())
			blocking_threads = true;
	}
	return blocking_threads;
}

/**
 * @brief Check if we are yield-blocked
 *
 * A program can be "yield-blocked" if all threads are ready to execute a
 * yield.
 *
 * @return True if the program is yield-blocked; false otherwise
 */
bool ModelExecution::is_yieldblocked() const
{
	if (!params->yieldblock)
		return false;

	for (unsigned int i = 0; i < get_num_threads(); i++) {
		thread_id_t tid = int_to_id(i);
		Thread *t = get_thread(tid);
		if (t->get_pending() && t->get_pending()->is_yield())
			return true;
	}
	return false;
}

/**
 * Check if this is a complete execution. That is, have all thread completed
 * execution (rather than exiting because sleep sets have forced a redundant
 * execution).
 *
 * @return True if the execution is complete.
 */
bool ModelExecution::is_complete_execution() const
{
	if (is_yieldblocked())
		return false;
	for (unsigned int i = 0; i < get_num_threads(); i++)
		if (is_enabled(int_to_id(i)))
			return false;
	return true;
}

/**
 * @brief Find the last fence-related backtracking conflict for a ModelAction
 *
 * This function performs the search for the most recent conflicting action
 * against which we should perform backtracking, as affected by fence
 * operations. This includes pairs of potentially-synchronizing actions which
 * occur due to fence-acquire or fence-release, and hence should be explored in
 * the opposite execution order.
 *
 * @param act The current action
 * @return The most recent action which conflicts with act due to fences
 */
ModelAction * ModelExecution::get_last_fence_conflict(ModelAction *act) const
{
	/* Only perform release/acquire fence backtracking for stores */
	if (!act->is_write())
		return NULL;

	/* Find a fence-release (or, act is a release) */
	ModelAction *last_release;
	if (act->is_release())
		last_release = act;
	else
		last_release = get_last_fence_release(act->get_tid());
	if (!last_release)
		return NULL;

	/* Skip past the release */
	const action_list_t *list = &action_trace;
	action_list_t::const_reverse_iterator rit;
	for (rit = list->rbegin(); rit != list->rend(); rit++)
		if (*rit == last_release)
			break;
	ASSERT(rit != list->rend());

	/* Find a prior:
	 *   load-acquire
	 * or
	 *   load --sb-> fence-acquire */
	ModelVector<ModelAction *> acquire_fences(get_num_threads(), NULL);
	ModelVector<ModelAction *> prior_loads(get_num_threads(), NULL);
	bool found_acquire_fences = false;
	for ( ; rit != list->rend(); rit++) {
		ModelAction *prev = *rit;
		if (act->same_thread(prev))
			continue;

		int tid = id_to_int(prev->get_tid());

		if (prev->is_read() && act->same_var(prev)) {
			if (prev->is_acquire()) {
				/* Found most recent load-acquire, don't need
				 * to search for more fences */
				if (!found_acquire_fences)
					return NULL;
			} else {
				prior_loads[tid] = prev;
			}
		}
		if (prev->is_acquire() && prev->is_fence() && !acquire_fences[tid]) {
			found_acquire_fences = true;
			acquire_fences[tid] = prev;
		}
	}

	ModelAction *latest_backtrack = NULL;
	for (unsigned int i = 0; i < acquire_fences.size(); i++)
		if (acquire_fences[i] && prior_loads[i])
			if (!latest_backtrack || *latest_backtrack < *acquire_fences[i])
				latest_backtrack = acquire_fences[i];
	return latest_backtrack;
}

/**
 * @brief Find the last backtracking conflict for a ModelAction
 *
 * This function performs the search for the most recent conflicting action
 * against which we should perform backtracking. This primary includes pairs of
 * synchronizing actions which should be explored in the opposite execution
 * order.
 *
 * @param act The current action
 * @return The most recent action which conflicts with act
 */
ModelAction * ModelExecution::get_last_conflict(ModelAction *act) const
{
	switch (act->get_type()) {
	case ATOMIC_FENCE:
		/* Only seq-cst fences can (directly) cause backtracking */
		if (!act->is_seqcst())
			break;
	case ATOMIC_READ:
	case ATOMIC_WRITE:
	case ATOMIC_RMW: {
		ModelAction *ret = NULL;

		/* linear search: from most recent to oldest */
		action_list_t *list = obj_map.get(act->get_location());
		action_list_t::reverse_iterator rit;
		for (rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *prev = *rit;
			if (prev == act)
				continue;
			if (prev->could_synchronize_with(act)) {
				ret = prev;
				break;
			}
		}

		ModelAction *ret2 = get_last_fence_conflict(act);
		if (!ret2)
			return ret;
		if (!ret)
			return ret2;
		if (*ret < *ret2)
			return ret2;
		return ret;
	}
	case ATOMIC_LOCK:
	case ATOMIC_TRYLOCK: {
		/* linear search: from most recent to oldest */
		action_list_t *list = obj_map.get(act->get_location());
		action_list_t::reverse_iterator rit;
		for (rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *prev = *rit;
			if (act->is_conflicting_lock(prev))
				return prev;
		}
		break;
	}
	case ATOMIC_UNLOCK: {
		/* linear search: from most recent to oldest */
		action_list_t *list = obj_map.get(act->get_location());
		action_list_t::reverse_iterator rit;
		for (rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *prev = *rit;
			if (!act->same_thread(prev) && prev->is_failed_trylock())
				return prev;
		}
		break;
	}
	case ATOMIC_WAIT: {
		/* linear search: from most recent to oldest */
		action_list_t *list = obj_map.get(act->get_location());
		action_list_t::reverse_iterator rit;
		for (rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *prev = *rit;
			if (!act->same_thread(prev) && prev->is_failed_trylock())
				return prev;
			if (!act->same_thread(prev) && prev->is_notify())
				return prev;
		}
		break;
	}

	case ATOMIC_NOTIFY_ALL:
	case ATOMIC_NOTIFY_ONE: {
		/* linear search: from most recent to oldest */
		action_list_t *list = obj_map.get(act->get_location());
		action_list_t::reverse_iterator rit;
		for (rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *prev = *rit;
			if (!act->same_thread(prev) && prev->is_wait())
				return prev;
		}
		break;
	}
	default:
		break;
	}
	return NULL;
}

/** This method finds backtracking points where we should try to
 * reorder the parameter ModelAction against.
 *
 * @param the ModelAction to find backtracking points for.
 */
void ModelExecution::set_backtracking(ModelAction *act)
{
	Thread *t = get_thread(act);
	ModelAction *prev = get_last_conflict(act);
	if (prev == NULL)
		return;

	Node *node = prev->get_node()->get_parent();

	/* See Dynamic Partial Order Reduction (addendum), POPL '05 */
	int low_tid, high_tid;
	if (node->enabled_status(t->get_id()) == THREAD_ENABLED) {
		low_tid = id_to_int(act->get_tid());
		high_tid = low_tid + 1;
	} else {
		low_tid = 0;
		high_tid = get_num_threads();
	}

	for (int i = low_tid; i < high_tid; i++) {
		thread_id_t tid = int_to_id(i);

		/* Make sure this thread can be enabled here. */
		if (i >= node->get_num_threads())
			break;

		/* See Dynamic Partial Order Reduction (addendum), POPL '05 */
		/* Don't backtrack into a point where the thread is disabled or sleeping. */
		if (node->enabled_status(tid) != THREAD_ENABLED)
			continue;

		/* Check if this has been explored already */
		if (node->has_been_explored(tid))
			continue;

		/* See if fairness allows */
		if (params->fairwindow != 0 && !node->has_priority(tid)) {
			bool unfair = false;
			for (int t = 0; t < node->get_num_threads(); t++) {
				thread_id_t tother = int_to_id(t);
				if (node->is_enabled(tother) && node->has_priority(tother)) {
					unfair = true;
					break;
				}
			}
			if (unfair)
				continue;
		}

		/* See if CHESS-like yield fairness allows */
		if (params->yieldon) {
			bool unfair = false;
			for (int t = 0; t < node->get_num_threads(); t++) {
				thread_id_t tother = int_to_id(t);
				if (node->is_enabled(tother) && node->has_priority_over(tid, tother)) {
					unfair = true;
					break;
				}
			}
			if (unfair)
				continue;
		}

		/* Cache the latest backtracking point */
		set_latest_backtrack(prev);

		/* If this is a new backtracking point, mark the tree */
		if (!node->set_backtrack(tid))
			continue;
		DEBUG("Setting backtrack: conflict = %d, instead tid = %d\n",
					id_to_int(prev->get_tid()),
					id_to_int(t->get_id()));
		if (DBG_ENABLED()) {
			prev->print();
			act->print();
		}
	}
}

/**
 * @brief Cache the a backtracking point as the "most recent", if eligible
 *
 * Note that this does not prepare the NodeStack for this backtracking
 * operation, it only caches the action on a per-execution basis
 *
 * @param act The operation at which we should explore a different next action
 * (i.e., backtracking point)
 * @return True, if this action is now the most recent backtracking point;
 * false otherwise
 */
bool ModelExecution::set_latest_backtrack(ModelAction *act)
{
	if (!priv->next_backtrack || *act > *priv->next_backtrack) {
		priv->next_backtrack = act;
		return true;
	}
	return false;
}

/**
 * Returns last backtracking point. The model checker will explore a different
 * path for this point in the next execution.
 * @return The ModelAction at which the next execution should diverge.
 */
ModelAction * ModelExecution::get_next_backtrack()
{
	ModelAction *next = priv->next_backtrack;
	priv->next_backtrack = NULL;
	return next;
}

/**
 * Processes a read model action.
 * @param curr is the read model action to process.
 * @return True if processing this read updates the mo_graph.
 */
bool ModelExecution::process_read(ModelAction *curr)
{
	Node *node = curr->get_node();
	while (true) {
		bool updated = false;
		switch (node->get_read_from_status()) {
		case READ_FROM_PAST: {
			const ModelAction *rf = node->get_read_from_past();
			ASSERT(rf);

			mo_graph->startChanges();

			ASSERT(!is_infeasible());
			if (!check_recency(curr, rf)) {
				if (node->increment_read_from()) {
					mo_graph->rollbackChanges();
					continue;
				} else {
					priv->too_many_reads = true;
				}
			}

			updated = r_modification_order(curr, rf);
			read_from(curr, rf);
			mo_graph->commitChanges();
			mo_check_promises(curr, true);
			break;
		}
		case READ_FROM_PROMISE: {
			Promise *promise = curr->get_node()->get_read_from_promise();
			if (promise->add_reader(curr))
				priv->failed_promise = true;
			curr->set_read_from_promise(promise);
			mo_graph->startChanges();
			if (!check_recency(curr, promise))
				priv->too_many_reads = true;
			updated = r_modification_order(curr, promise);
			mo_graph->commitChanges();
			break;
		}
		case READ_FROM_FUTURE: {
			/* Read from future value */
			struct future_value fv = node->get_future_value();
			Promise *promise = new Promise(this, curr, fv);
			curr->set_read_from_promise(promise);
			promises.push_back(promise);
			mo_graph->startChanges();
			updated = r_modification_order(curr, promise);
			mo_graph->commitChanges();
			break;
		}
		default:
			ASSERT(false);
		}
		get_thread(curr)->set_return_value(curr->get_return_value());
		return updated;
	}
}

/**
 * Processes a lock, trylock, or unlock model action.  @param curr is
 * the read model action to process.
 *
 * The try lock operation checks whether the lock is taken.  If not,
 * it falls to the normal lock operation case.  If so, it returns
 * fail.
 *
 * The lock operation has already been checked that it is enabled, so
 * it just grabs the lock and synchronizes with the previous unlock.
 *
 * The unlock operation has to re-enable all of the threads that are
 * waiting on the lock.
 *
 * @return True if synchronization was updated; false otherwise
 */
bool ModelExecution::process_mutex(ModelAction *curr)
{
	std::mutex *mutex = curr->get_mutex();
	struct std::mutex_state *state = NULL;

	if (mutex)
		state = mutex->get_state();

	switch (curr->get_type()) {
	case ATOMIC_TRYLOCK: {
		bool success = !state->locked;
		curr->set_try_lock(success);
		if (!success) {
			get_thread(curr)->set_return_value(0);
			break;
		}
		get_thread(curr)->set_return_value(1);
	}
		//otherwise fall into the lock case
	case ATOMIC_LOCK: {
		if (curr->get_cv()->getClock(state->alloc_tid) <= state->alloc_clock)
			assert_bug("Lock access before initialization");
		state->locked = get_thread(curr);
		ModelAction *unlock = get_last_unlock(curr);
		//synchronize with the previous unlock statement
		if (unlock != NULL) {
			synchronize(unlock, curr);
			return true;
		}
		break;
	}
	case ATOMIC_WAIT:
	case ATOMIC_UNLOCK: {
		/* wake up the other threads */
		for (unsigned int i = 0; i < get_num_threads(); i++) {
			Thread *t = get_thread(int_to_id(i));
			Thread *curr_thrd = get_thread(curr);
			if (t->waiting_on() == curr_thrd && t->get_pending()->is_lock())
				scheduler->wake(t);
		}

		/* unlock the lock - after checking who was waiting on it */
		state->locked = NULL;

		if (!curr->is_wait())
			break; /* The rest is only for ATOMIC_WAIT */

		/* Should we go to sleep? (simulate spurious failures) */
		if (curr->get_node()->get_misc() == 0) {
			get_safe_ptr_action(&condvar_waiters_map, curr->get_location())->push_back(curr);
			/* disable us */
			scheduler->sleep(get_thread(curr));
		}
		break;
	}
	case ATOMIC_NOTIFY_ALL: {
		action_list_t *waiters = get_safe_ptr_action(&condvar_waiters_map, curr->get_location());
		//activate all the waiting threads
		for (action_list_t::iterator rit = waiters->begin(); rit != waiters->end(); rit++) {
			scheduler->wake(get_thread(*rit));
		}
		waiters->clear();
		break;
	}
	case ATOMIC_NOTIFY_ONE: {
		action_list_t *waiters = get_safe_ptr_action(&condvar_waiters_map, curr->get_location());
		int wakeupthread = curr->get_node()->get_misc();
		action_list_t::iterator it = waiters->begin();
		advance(it, wakeupthread);
		scheduler->wake(get_thread(*it));
		waiters->erase(it);
		break;
	}

	default:
		ASSERT(0);
	}
	return false;
}

/**
 * @brief Check if the current pending promises allow a future value to be sent
 *
 * It is unsafe to pass a future value back if there exists a pending promise Pr
 * such that:
 *
 *    reader --exec-> Pr --exec-> writer
 *
 * If such Pr exists, we must save the pending future value until Pr is
 * resolved.
 *
 * @param writer The operation which sends the future value. Must be a write.
 * @param reader The operation which will observe the value. Must be a read.
 * @return True if the future value can be sent now; false if it must wait.
 */
bool ModelExecution::promises_may_allow(const ModelAction *writer,
		const ModelAction *reader) const
{
	for (int i = promises.size() - 1; i >= 0; i--) {
		ModelAction *pr = promises[i]->get_reader(0);
		//reader is after promise...doesn't cross any promise
		if (*reader > *pr)
			return true;
		//writer is after promise, reader before...bad...
		if (*writer > *pr)
			return false;
	}
	return true;
}

/**
 * @brief Add a future value to a reader
 *
 * This function performs a few additional checks to ensure that the future
 * value can be feasibly observed by the reader
 *
 * @param writer The operation whose value is sent. Must be a write.
 * @param reader The read operation which may read the future value. Must be a read.
 */
void ModelExecution::add_future_value(const ModelAction *writer, ModelAction *reader)
{
	/* Do more ambitious checks now that mo is more complete */
	if (!mo_may_allow(writer, reader))
		return;

	Node *node = reader->get_node();

	/* Find an ancestor thread which exists at the time of the reader */
	Thread *write_thread = get_thread(writer);
	while (id_to_int(write_thread->get_id()) >= node->get_num_threads())
		write_thread = write_thread->get_parent();

	struct future_value fv = {
		writer->get_write_value(),
		writer->get_seq_number() + params->maxfuturedelay,
		write_thread->get_id(),
	};
	if (node->add_future_value(fv))
		set_latest_backtrack(reader);
}

/**
 * Process a write ModelAction
 * @param curr The ModelAction to process
 * @param work The work queue, for adding fixup work
 * @return True if the mo_graph was updated or promises were resolved
 */
bool ModelExecution::process_write(ModelAction *curr, work_queue_t *work)
{
	/* Readers to which we may send our future value */
	ModelVector<ModelAction *> send_fv;

	const ModelAction *earliest_promise_reader;
	bool updated_promises = false;

	bool updated_mod_order = w_modification_order(curr, &send_fv);
	Promise *promise = pop_promise_to_resolve(curr);

	if (promise) {
		earliest_promise_reader = promise->get_reader(0);
		updated_promises = resolve_promise(curr, promise, work);
	} else
		earliest_promise_reader = NULL;

	for (unsigned int i = 0; i < send_fv.size(); i++) {
		ModelAction *read = send_fv[i];

		/* Don't send future values to reads after the Promise we resolve */
		if (!earliest_promise_reader || *read < *earliest_promise_reader) {
			/* Check if future value can be sent immediately */
			if (promises_may_allow(curr, read)) {
				add_future_value(curr, read);
			} else {
				futurevalues.push_back(PendingFutureValue(curr, read));
			}
		}
	}

	/* Check the pending future values */
	for (int i = (int)futurevalues.size() - 1; i >= 0; i--) {
		struct PendingFutureValue pfv = futurevalues[i];
		if (promises_may_allow(pfv.writer, pfv.reader)) {
			add_future_value(pfv.writer, pfv.reader);
			futurevalues.erase(futurevalues.begin() + i);
		}
	}

	mo_graph->commitChanges();
	mo_check_promises(curr, false);

	get_thread(curr)->set_return_value(VALUE_NONE);
	return updated_mod_order || updated_promises;
}

/**
 * Process a fence ModelAction
 * @param curr The ModelAction to process
 * @return True if synchronization was updated
 */
bool ModelExecution::process_fence(ModelAction *curr)
{
	/*
	 * fence-relaxed: no-op
	 * fence-release: only log the occurence (not in this function), for
	 *   use in later synchronization
	 * fence-acquire (this function): search for hypothetical release
	 *   sequences
	 * fence-seq-cst: MO constraints formed in {r,w}_modification_order
	 */
	bool updated = false;
	if (curr->is_acquire()) {
		action_list_t *list = &action_trace;
		action_list_t::reverse_iterator rit;
		/* Find X : is_read(X) && X --sb-> curr */
		for (rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *act = *rit;
			if (act == curr)
				continue;
			if (act->get_tid() != curr->get_tid())
				continue;
			/* Stop at the beginning of the thread */
			if (act->is_thread_start())
				break;
			/* Stop once we reach a prior fence-acquire */
			if (act->is_fence() && act->is_acquire())
				break;
			if (!act->is_read())
				continue;
			/* read-acquire will find its own release sequences */
			if (act->is_acquire())
				continue;

			/* Establish hypothetical release sequences */
			rel_heads_list_t release_heads;
			get_release_seq_heads(curr, act, &release_heads);
			for (unsigned int i = 0; i < release_heads.size(); i++)
				synchronize(release_heads[i], curr);
			if (release_heads.size() != 0)
				updated = true;
		}
	}
	return updated;
}

/**
 * @brief Process the current action for thread-related activity
 *
 * Performs current-action processing for a THREAD_* ModelAction. Proccesses
 * may include setting Thread status, completing THREAD_FINISH/THREAD_JOIN
 * synchronization, etc.  This function is a no-op for non-THREAD actions
 * (e.g., ATOMIC_{READ,WRITE,RMW,LOCK}, etc.)
 *
 * @param curr The current action
 * @return True if synchronization was updated or a thread completed
 */
bool ModelExecution::process_thread_action(ModelAction *curr)
{
	bool updated = false;

	switch (curr->get_type()) {
	case THREAD_CREATE: {
		thrd_t *thrd = (thrd_t *)curr->get_location();
		struct thread_params *params = (struct thread_params *)curr->get_value();
		Thread *th = new Thread(get_next_id(), thrd, params->func, params->arg, get_thread(curr));
		add_thread(th);
		th->set_creation(curr);
		/* Promises can be satisfied by children */
		for (unsigned int i = 0; i < promises.size(); i++) {
			Promise *promise = promises[i];
			if (promise->thread_is_available(curr->get_tid()))
				promise->add_thread(th->get_id());
		}
		break;
	}
	case THREAD_JOIN: {
		Thread *blocking = curr->get_thread_operand();
		ModelAction *act = get_last_action(blocking->get_id());
		synchronize(act, curr);
		updated = true; /* trigger rel-seq checks */
		break;
	}
	case THREAD_FINISH: {
		Thread *th = get_thread(curr);
		/* Wake up any joining threads */
		for (unsigned int i = 0; i < get_num_threads(); i++) {
			Thread *waiting = get_thread(int_to_id(i));
			if (waiting->waiting_on() == th &&
					waiting->get_pending()->is_thread_join())
				scheduler->wake(waiting);
		}
		th->complete();
		/* Completed thread can't satisfy promises */
		for (unsigned int i = 0; i < promises.size(); i++) {
			Promise *promise = promises[i];
			if (promise->thread_is_available(th->get_id()))
				if (promise->eliminate_thread(th->get_id()))
					priv->failed_promise = true;
		}
		updated = true; /* trigger rel-seq checks */
		break;
	}
	case THREAD_START: {
		check_promises(curr->get_tid(), NULL, curr->get_cv());
		break;
	}
	default:
		break;
	}

	return updated;
}

/**
 * @brief Process the current action for release sequence fixup activity
 *
 * Performs model-checker release sequence fixups for the current action,
 * forcing a single pending release sequence to break (with a given, potential
 * "loose" write) or to complete (i.e., synchronize). If a pending release
 * sequence forms a complete release sequence, then we must perform the fixup
 * synchronization, mo_graph additions, etc.
 *
 * @param curr The current action; must be a release sequence fixup action
 * @param work_queue The work queue to which to add work items as they are
 * generated
 */
void ModelExecution::process_relseq_fixup(ModelAction *curr, work_queue_t *work_queue)
{
	const ModelAction *write = curr->get_node()->get_relseq_break();
	struct release_seq *sequence = pending_rel_seqs.back();
	pending_rel_seqs.pop_back();
	ASSERT(sequence);
	ModelAction *acquire = sequence->acquire;
	const ModelAction *rf = sequence->rf;
	const ModelAction *release = sequence->release;
	ASSERT(acquire);
	ASSERT(release);
	ASSERT(rf);
	ASSERT(release->same_thread(rf));

	if (write == NULL) {
		/**
		 * @todo Forcing a synchronization requires that we set
		 * modification order constraints. For instance, we can't allow
		 * a fixup sequence in which two separate read-acquire
		 * operations read from the same sequence, where the first one
		 * synchronizes and the other doesn't. Essentially, we can't
		 * allow any writes to insert themselves between 'release' and
		 * 'rf'
		 */

		/* Must synchronize */
		if (!synchronize(release, acquire))
			return;

		/* Propagate the changed clock vector */
		propagate_clockvector(acquire, work_queue);
	} else {
		/* Break release sequence with new edges:
		 *   release --mo--> write --mo--> rf */
		mo_graph->addEdge(release, write);
		mo_graph->addEdge(write, rf);
	}

	/* See if we have realized a data race */
	checkDataRaces();
}

/**
 * Initialize the current action by performing one or more of the following
 * actions, as appropriate: merging RMWR and RMWC/RMW actions, stepping forward
 * in the NodeStack, manipulating backtracking sets, allocating and
 * initializing clock vectors, and computing the promises to fulfill.
 *
 * @param curr The current action, as passed from the user context; may be
 * freed/invalidated after the execution of this function, with a different
 * action "returned" its place (pass-by-reference)
 * @return True if curr is a newly-explored action; false otherwise
 */
bool ModelExecution::initialize_curr_action(ModelAction **curr)
{
	ModelAction *newcurr;

	if ((*curr)->is_rmwc() || (*curr)->is_rmw()) {
		newcurr = process_rmw(*curr);
		delete *curr;

		if (newcurr->is_rmw())
			compute_promises(newcurr);

		*curr = newcurr;
		return false;
	}

	(*curr)->set_seq_number(get_next_seq_num());

	newcurr = node_stack->explore_action(*curr, scheduler->get_enabled_array());
	if (newcurr) {
		/* First restore type and order in case of RMW operation */
		if ((*curr)->is_rmwr())
			newcurr->copy_typeandorder(*curr);

		ASSERT((*curr)->get_location() == newcurr->get_location());
		newcurr->copy_from_new(*curr);

		/* Discard duplicate ModelAction; use action from NodeStack */
		delete *curr;

		/* Always compute new clock vector */
		newcurr->create_cv(get_parent_action(newcurr->get_tid()));

		*curr = newcurr;
		return false; /* Action was explored previously */
	} else {
		newcurr = *curr;

		/* Always compute new clock vector */
		newcurr->create_cv(get_parent_action(newcurr->get_tid()));

		/* Assign most recent release fence */
		newcurr->set_last_fence_release(get_last_fence_release(newcurr->get_tid()));

		/*
		 * Perform one-time actions when pushing new ModelAction onto
		 * NodeStack
		 */
		if (newcurr->is_write())
			compute_promises(newcurr);
		else if (newcurr->is_relseq_fixup())
			compute_relseq_breakwrites(newcurr);
		else if (newcurr->is_wait())
			newcurr->get_node()->set_misc_max(2);
		else if (newcurr->is_notify_one()) {
			newcurr->get_node()->set_misc_max(get_safe_ptr_action(&condvar_waiters_map, newcurr->get_location())->size());
		}
		return true; /* This was a new ModelAction */
	}
}

/**
 * @brief Establish reads-from relation between two actions
 *
 * Perform basic operations involved with establishing a concrete rf relation,
 * including setting the ModelAction data and checking for release sequences.
 *
 * @param act The action that is reading (must be a read)
 * @param rf The action from which we are reading (must be a write)
 *
 * @return True if this read established synchronization
 */
bool ModelExecution::read_from(ModelAction *act, const ModelAction *rf)
{
	ASSERT(rf);
	ASSERT(rf->is_write());

	act->set_read_from(rf);
	if (act->is_acquire()) {
		rel_heads_list_t release_heads;
		get_release_seq_heads(act, act, &release_heads);
		int num_heads = release_heads.size();
		for (unsigned int i = 0; i < release_heads.size(); i++)
			if (!synchronize(release_heads[i], act))
				num_heads--;
		return num_heads > 0;
	}
	return false;
}

/**
 * @brief Synchronizes two actions
 *
 * When A synchronizes with B (or A --sw-> B), B inherits A's clock vector.
 * This function performs the synchronization as well as providing other hooks
 * for other checks along with synchronization.
 *
 * @param first The left-hand side of the synchronizes-with relation
 * @param second The right-hand side of the synchronizes-with relation
 * @return True if the synchronization was successful (i.e., was consistent
 * with the execution order); false otherwise
 */
bool ModelExecution::synchronize(const ModelAction *first, ModelAction *second)
{
	if (*second < *first) {
		set_bad_synchronization();
		return false;
	}
	check_promises(first->get_tid(), second->get_cv(), first->get_cv());
	return second->synchronize_with(first);
}

/**
 * Check promises and eliminate potentially-satisfying threads when a thread is
 * blocked (e.g., join, lock). A thread which is waiting on another thread can
 * no longer satisfy a promise generated from that thread.
 *
 * @param blocker The thread on which a thread is waiting
 * @param waiting The waiting thread
 */
void ModelExecution::thread_blocking_check_promises(Thread *blocker, Thread *waiting)
{
	for (unsigned int i = 0; i < promises.size(); i++) {
		Promise *promise = promises[i];
		if (!promise->thread_is_available(waiting->get_id()))
			continue;
		for (unsigned int j = 0; j < promise->get_num_readers(); j++) {
			ModelAction *reader = promise->get_reader(j);
			if (reader->get_tid() != blocker->get_id())
				continue;
			if (promise->eliminate_thread(waiting->get_id())) {
				/* Promise has failed */
				priv->failed_promise = true;
			} else {
				/* Only eliminate the 'waiting' thread once */
				return;
			}
		}
	}
}

/**
 * @brief Check whether a model action is enabled.
 *
 * Checks whether an operation would be successful (i.e., is a lock already
 * locked, or is the joined thread already complete).
 *
 * For yield-blocking, yields are never enabled.
 *
 * @param curr is the ModelAction to check whether it is enabled.
 * @return a bool that indicates whether the action is enabled.
 */
bool ModelExecution::check_action_enabled(ModelAction *curr) {
	if (curr->is_lock()) {
		std::mutex *lock = curr->get_mutex();
		struct std::mutex_state *state = lock->get_state();
		if (state->locked)
			return false;
	} else if (curr->is_thread_join()) {
		Thread *blocking = curr->get_thread_operand();
		if (!blocking->is_complete()) {
			thread_blocking_check_promises(blocking, get_thread(curr));
			return false;
		}
	} else if (params->yieldblock && curr->is_yield()) {
		return false;
	}

	return true;
}

/**
 * This is the heart of the model checker routine. It performs model-checking
 * actions corresponding to a given "current action." Among other processes, it
 * calculates reads-from relationships, updates synchronization clock vectors,
 * forms a memory_order constraints graph, and handles replay/backtrack
 * execution when running permutations of previously-observed executions.
 *
 * @param curr The current action to process
 * @return The ModelAction that is actually executed; may be different than
 * curr
 */
ModelAction * ModelExecution::check_current_action(ModelAction *curr)
{
	ASSERT(curr);
	bool second_part_of_rmw = curr->is_rmwc() || curr->is_rmw();
	bool newly_explored = initialize_curr_action(&curr);

	DBG();

	wake_up_sleeping_actions(curr);

	/* Compute fairness information for CHESS yield algorithm */
	if (params->yieldon) {
		curr->get_node()->update_yield(scheduler);
	}

	/* Add the action to lists before any other model-checking tasks */
	if (!second_part_of_rmw)
		add_action_to_lists(curr);

	/* Build may_read_from set for newly-created actions */
	if (newly_explored && curr->is_read())
		build_may_read_from(curr);

	/* Initialize work_queue with the "current action" work */
	work_queue_t work_queue(1, CheckCurrWorkEntry(curr));
	while (!work_queue.empty() && !has_asserted()) {
		WorkQueueEntry work = work_queue.front();
		work_queue.pop_front();

		switch (work.type) {
		case WORK_CHECK_CURR_ACTION: {
			ModelAction *act = work.action;
			bool update = false; /* update this location's release seq's */
			bool update_all = false; /* update all release seq's */

			if (process_thread_action(curr))
				update_all = true;

			if (act->is_read() && !second_part_of_rmw && process_read(act))
				update = true;

			if (act->is_write() && process_write(act, &work_queue))
				update = true;

			if (act->is_fence() && process_fence(act))
				update_all = true;

			if (act->is_mutex_op() && process_mutex(act))
				update_all = true;

			if (act->is_relseq_fixup())
				process_relseq_fixup(curr, &work_queue);

			if (update_all)
				work_queue.push_back(CheckRelSeqWorkEntry(NULL));
			else if (update)
				work_queue.push_back(CheckRelSeqWorkEntry(act->get_location()));
			break;
		}
		case WORK_CHECK_RELEASE_SEQ:
			resolve_release_sequences(work.location, &work_queue);
			break;
		case WORK_CHECK_MO_EDGES: {
			/** @todo Complete verification of work_queue */
			ModelAction *act = work.action;
			bool updated = false;

			if (act->is_read()) {
				const ModelAction *rf = act->get_reads_from();
				const Promise *promise = act->get_reads_from_promise();
				if (rf) {
					if (r_modification_order(act, rf))
						updated = true;
					if (act->is_seqcst()) {
						ModelAction *last_sc_write = get_last_seq_cst_write(act);
						if (last_sc_write != NULL && rf->happens_before(last_sc_write)) {
							set_bad_sc_read();
						}
					}
				} else if (promise) {
					if (r_modification_order(act, promise))
						updated = true;
				}
			}
			if (act->is_write()) {
				if (w_modification_order(act, NULL))
					updated = true;
			}
			mo_graph->commitChanges();

			if (updated)
				work_queue.push_back(CheckRelSeqWorkEntry(act->get_location()));
			break;
		}
		default:
			ASSERT(false);
			break;
		}
	}

	check_curr_backtracking(curr);
	set_backtracking(curr);
	return curr;
}

void ModelExecution::check_curr_backtracking(ModelAction *curr)
{
	Node *currnode = curr->get_node();
	Node *parnode = currnode->get_parent();

	if ((parnode && !parnode->backtrack_empty()) ||
			 !currnode->misc_empty() ||
			 !currnode->read_from_empty() ||
			 !currnode->promise_empty() ||
			 !currnode->relseq_break_empty()) {
		set_latest_backtrack(curr);
	}
}

bool ModelExecution::promises_expired() const
{
	for (unsigned int i = 0; i < promises.size(); i++) {
		Promise *promise = promises[i];
		if (promise->get_expiration() < priv->used_sequence_numbers)
			return true;
	}
	return false;
}

/**
 * This is the strongest feasibility check available.
 * @return whether the current trace (partial or complete) must be a prefix of
 * a feasible trace.
 */
bool ModelExecution::isfeasibleprefix() const
{
	return pending_rel_seqs.size() == 0 && is_feasible_prefix_ignore_relseq();
}

/**
 * Print disagnostic information about an infeasible execution
 * @param prefix A string to prefix the output with; if NULL, then a default
 * message prefix will be provided
 */
void ModelExecution::print_infeasibility(const char *prefix) const
{
	char buf[100];
	char *ptr = buf;
	if (mo_graph->checkForCycles())
		ptr += sprintf(ptr, "[mo cycle]");
	if (priv->failed_promise || priv->hard_failed_promise)
		ptr += sprintf(ptr, "[failed promise]");
	if (priv->too_many_reads)
		ptr += sprintf(ptr, "[too many reads]");
	if (priv->no_valid_reads)
		ptr += sprintf(ptr, "[no valid reads-from]");
	if (priv->bad_synchronization)
		ptr += sprintf(ptr, "[bad sw ordering]");
	if (priv->bad_sc_read)
		ptr += sprintf(ptr, "[bad sc read]");
	if (promises_expired())
		ptr += sprintf(ptr, "[promise expired]");
	if (promises.size() != 0)
		ptr += sprintf(ptr, "[unresolved promise]");
	if (ptr != buf)
		model_print("%s: %s", prefix ? prefix : "Infeasible", buf);
}

/**
 * Returns whether the current completed trace is feasible, except for pending
 * release sequences.
 */
bool ModelExecution::is_feasible_prefix_ignore_relseq() const
{
	return !is_infeasible() && promises.size() == 0 && ! priv->failed_promise;

}

/**
 * Check if the current partial trace is infeasible. Does not check any
 * end-of-execution flags, which might rule out the execution. Thus, this is
 * useful only for ruling an execution as infeasible.
 * @return whether the current partial trace is infeasible.
 */
bool ModelExecution::is_infeasible() const
{
	return mo_graph->checkForCycles() ||
		priv->no_valid_reads ||
		priv->too_many_reads ||
		priv->bad_synchronization ||
		priv->bad_sc_read ||
		priv->hard_failed_promise ||
		promises_expired();
}

/** Close out a RMWR by converting previous RMWR into a RMW or READ. */
ModelAction * ModelExecution::process_rmw(ModelAction *act) {
	ModelAction *lastread = get_last_action(act->get_tid());
	lastread->process_rmw(act);
	if (act->is_rmw()) {
		if (lastread->get_reads_from())
			mo_graph->addRMWEdge(lastread->get_reads_from(), lastread);
		else
			mo_graph->addRMWEdge(lastread->get_reads_from_promise(), lastread);
		mo_graph->commitChanges();
	}
	return lastread;
}

/**
 * A helper function for ModelExecution::check_recency, to check if the current
 * thread is able to read from a different write/promise for 'params.maxreads'
 * number of steps and if that write/promise should become visible (i.e., is
 * ordered later in the modification order). This helps model memory liveness.
 *
 * @param curr The current action. Must be a read.
 * @param rf The write/promise from which we plan to read
 * @param other_rf The write/promise from which we may read
 * @return True if we were able to read from other_rf for params.maxreads steps
 */
template <typename T, typename U>
bool ModelExecution::should_read_instead(const ModelAction *curr, const T *rf, const U *other_rf) const
{
	/* Need a different write/promise */
	if (other_rf->equals(rf))
		return false;

	/* Only look for "newer" writes/promises */
	if (!mo_graph->checkReachable(rf, other_rf))
		return false;

	SnapVector<action_list_t> *thrd_lists = obj_thrd_map.get(curr->get_location());
	action_list_t *list = &(*thrd_lists)[id_to_int(curr->get_tid())];
	action_list_t::reverse_iterator rit = list->rbegin();
	ASSERT((*rit) == curr);
	/* Skip past curr */
	rit++;

	/* Does this write/promise work for everyone? */
	for (int i = 0; i < params->maxreads; i++, rit++) {
		ModelAction *act = *rit;
		if (!act->may_read_from(other_rf))
			return false;
	}
	return true;
}

/**
 * Checks whether a thread has read from the same write or Promise for too many
 * times without seeing the effects of a later write/Promise.
 *
 * Basic idea:
 * 1) there must a different write/promise that we could read from,
 * 2) we must have read from the same write/promise in excess of maxreads times,
 * 3) that other write/promise must have been in the reads_from set for maxreads times, and
 * 4) that other write/promise must be mod-ordered after the write/promise we are reading.
 *
 * If so, we decide that the execution is no longer feasible.
 *
 * @param curr The current action. Must be a read.
 * @param rf The ModelAction/Promise from which we might read.
 * @return True if the read should succeed; false otherwise
 */
template <typename T>
bool ModelExecution::check_recency(ModelAction *curr, const T *rf) const
{
	if (!params->maxreads)
		return true;

	//NOTE: Next check is just optimization, not really necessary....
	if (curr->get_node()->get_read_from_past_size() +
			curr->get_node()->get_read_from_promise_size() <= 1)
		return true;

	SnapVector<action_list_t> *thrd_lists = obj_thrd_map.get(curr->get_location());
	int tid = id_to_int(curr->get_tid());
	ASSERT(tid < (int)thrd_lists->size());
	action_list_t *list = &(*thrd_lists)[tid];
	action_list_t::reverse_iterator rit = list->rbegin();
	ASSERT((*rit) == curr);
	/* Skip past curr */
	rit++;

	action_list_t::reverse_iterator ritcopy = rit;
	/* See if we have enough reads from the same value */
	for (int count = 0; count < params->maxreads; ritcopy++, count++) {
		if (ritcopy == list->rend())
			return true;
		ModelAction *act = *ritcopy;
		if (!act->is_read())
			return true;
		if (act->get_reads_from_promise() && !act->get_reads_from_promise()->equals(rf))
			return true;
		if (act->get_reads_from() && !act->get_reads_from()->equals(rf))
			return true;
		if (act->get_node()->get_read_from_past_size() +
				act->get_node()->get_read_from_promise_size() <= 1)
			return true;
	}
	for (int i = 0; i < curr->get_node()->get_read_from_past_size(); i++) {
		const ModelAction *write = curr->get_node()->get_read_from_past(i);
		if (should_read_instead(curr, rf, write))
			return false; /* liveness failure */
	}
	for (int i = 0; i < curr->get_node()->get_read_from_promise_size(); i++) {
		const Promise *promise = curr->get_node()->get_read_from_promise(i);
		if (should_read_instead(curr, rf, promise))
			return false; /* liveness failure */
	}
	return true;
}

/**
 * @brief Updates the mo_graph with the constraints imposed from the current
 * read.
 *
 * Basic idea is the following: Go through each other thread and find
 * the last action that happened before our read.  Two cases:
 *
 * -# The action is a write: that write must either occur before
 * the write we read from or be the write we read from.
 * -# The action is a read: the write that that action read from
 * must occur before the write we read from or be the same write.
 *
 * @param curr The current action. Must be a read.
 * @param rf The ModelAction or Promise that curr reads from. Must be a write.
 * @return True if modification order edges were added; false otherwise
 */
template <typename rf_type>
bool ModelExecution::r_modification_order(ModelAction *curr, const rf_type *rf)
{
	SnapVector<action_list_t> *thrd_lists = obj_thrd_map.get(curr->get_location());
	unsigned int i;
	bool added = false;
	ASSERT(curr->is_read());

	/* Last SC fence in the current thread */
	ModelAction *last_sc_fence_local = get_last_seq_cst_fence(curr->get_tid(), NULL);
	ModelAction *last_sc_write = NULL;
	if (curr->is_seqcst())
		last_sc_write = get_last_seq_cst_write(curr);

	/* Iterate over all threads */
	for (i = 0; i < thrd_lists->size(); i++) {
		/* Last SC fence in thread i */
		ModelAction *last_sc_fence_thread_local = NULL;
		if (int_to_id((int)i) != curr->get_tid())
			last_sc_fence_thread_local = get_last_seq_cst_fence(int_to_id(i), NULL);

		/* Last SC fence in thread i, before last SC fence in current thread */
		ModelAction *last_sc_fence_thread_before = NULL;
		if (last_sc_fence_local)
			last_sc_fence_thread_before = get_last_seq_cst_fence(int_to_id(i), last_sc_fence_local);

		/* Iterate over actions in thread, starting from most recent */
		action_list_t *list = &(*thrd_lists)[i];
		action_list_t::reverse_iterator rit;
		for (rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *act = *rit;

			/* Skip curr */
			if (act == curr)
				continue;
			/* Don't want to add reflexive edges on 'rf' */
			if (act->equals(rf)) {
				if (act->happens_before(curr))
					break;
				else
					continue;
			}

			if (act->is_write()) {
				/* C++, Section 29.3 statement 5 */
				if (curr->is_seqcst() && last_sc_fence_thread_local &&
						*act < *last_sc_fence_thread_local) {
					added = mo_graph->addEdge(act, rf) || added;
					break;
				}
				/* C++, Section 29.3 statement 4 */
				else if (act->is_seqcst() && last_sc_fence_local &&
						*act < *last_sc_fence_local) {
					added = mo_graph->addEdge(act, rf) || added;
					break;
				}
				/* C++, Section 29.3 statement 6 */
				else if (last_sc_fence_thread_before &&
						*act < *last_sc_fence_thread_before) {
					added = mo_graph->addEdge(act, rf) || added;
					break;
				}
			}

			/*
			 * Include at most one act per-thread that "happens
			 * before" curr
			 */
			if (act->happens_before(curr)) {
				if (act->is_write()) {
					added = mo_graph->addEdge(act, rf) || added;
				} else {
					const ModelAction *prevrf = act->get_reads_from();
					const Promise *prevrf_promise = act->get_reads_from_promise();
					if (prevrf) {
						if (!prevrf->equals(rf))
							added = mo_graph->addEdge(prevrf, rf) || added;
					} else if (!prevrf_promise->equals(rf)) {
						added = mo_graph->addEdge(prevrf_promise, rf) || added;
					}
				}
				break;
			}
		}
	}

	/*
	 * All compatible, thread-exclusive promises must be ordered after any
	 * concrete loads from the same thread
	 */
	for (unsigned int i = 0; i < promises.size(); i++)
		if (promises[i]->is_compatible_exclusive(curr))
			added = mo_graph->addEdge(rf, promises[i]) || added;

	return added;
}

/**
 * Updates the mo_graph with the constraints imposed from the current write.
 *
 * Basic idea is the following: Go through each other thread and find
 * the lastest action that happened before our write.  Two cases:
 *
 * (1) The action is a write => that write must occur before
 * the current write
 *
 * (2) The action is a read => the write that that action read from
 * must occur before the current write.
 *
 * This method also handles two other issues:
 *
 * (I) Sequential Consistency: Making sure that if the current write is
 * seq_cst, that it occurs after the previous seq_cst write.
 *
 * (II) Sending the write back to non-synchronizing reads.
 *
 * @param curr The current action. Must be a write.
 * @param send_fv A vector for stashing reads to which we may pass our future
 * value. If NULL, then don't record any future values.
 * @return True if modification order edges were added; false otherwise
 */
bool ModelExecution::w_modification_order(ModelAction *curr, ModelVector<ModelAction *> *send_fv)
{
	SnapVector<action_list_t> *thrd_lists = obj_thrd_map.get(curr->get_location());
	unsigned int i;
	bool added = false;
	ASSERT(curr->is_write());

	if (curr->is_seqcst()) {
		/* We have to at least see the last sequentially consistent write,
			 so we are initialized. */
		ModelAction *last_seq_cst = get_last_seq_cst_write(curr);
		if (last_seq_cst != NULL) {
			added = mo_graph->addEdge(last_seq_cst, curr) || added;
		}
	}

	/* Last SC fence in the current thread */
	ModelAction *last_sc_fence_local = get_last_seq_cst_fence(curr->get_tid(), NULL);

	/* Iterate over all threads */
	for (i = 0; i < thrd_lists->size(); i++) {
		/* Last SC fence in thread i, before last SC fence in current thread */
		ModelAction *last_sc_fence_thread_before = NULL;
		if (last_sc_fence_local && int_to_id((int)i) != curr->get_tid())
			last_sc_fence_thread_before = get_last_seq_cst_fence(int_to_id(i), last_sc_fence_local);

		/* Iterate over actions in thread, starting from most recent */
		action_list_t *list = &(*thrd_lists)[i];
		action_list_t::reverse_iterator rit;
		for (rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *act = *rit;
			if (act == curr) {
				/*
				 * 1) If RMW and it actually read from something, then we
				 * already have all relevant edges, so just skip to next
				 * thread.
				 *
				 * 2) If RMW and it didn't read from anything, we should
				 * whatever edge we can get to speed up convergence.
				 *
				 * 3) If normal write, we need to look at earlier actions, so
				 * continue processing list.
				 */
				if (curr->is_rmw()) {
					if (curr->get_reads_from() != NULL)
						break;
					else
						continue;
				} else
					continue;
			}

			/* C++, Section 29.3 statement 7 */
			if (last_sc_fence_thread_before && act->is_write() &&
					*act < *last_sc_fence_thread_before) {
				added = mo_graph->addEdge(act, curr) || added;
				break;
			}

			/*
			 * Include at most one act per-thread that "happens
			 * before" curr
			 */
			if (act->happens_before(curr)) {
				/*
				 * Note: if act is RMW, just add edge:
				 *   act --mo--> curr
				 * The following edge should be handled elsewhere:
				 *   readfrom(act) --mo--> act
				 */
				if (act->is_write())
					added = mo_graph->addEdge(act, curr) || added;
				else if (act->is_read()) {
					//if previous read accessed a null, just keep going
					if (act->get_reads_from() == NULL) {
						added = mo_graph->addEdge(act->get_reads_from_promise(), curr) || added;
					} else
						added = mo_graph->addEdge(act->get_reads_from(), curr) || added;
				}
				break;
			} else if (act->is_read() && !act->could_synchronize_with(curr) &&
			                             !act->same_thread(curr)) {
				/* We have an action that:
				   (1) did not happen before us
				   (2) is a read and we are a write
				   (3) cannot synchronize with us
				   (4) is in a different thread
				   =>
				   that read could potentially read from our write.  Note that
				   these checks are overly conservative at this point, we'll
				   do more checks before actually removing the
				   pendingfuturevalue.

				 */

				if (send_fv && thin_air_constraint_may_allow(curr, act) && check_coherence_promise(curr, act)) {
					if (!is_infeasible())
						send_fv->push_back(act);
					else if (curr->is_rmw() && act->is_rmw() && curr->get_reads_from() && curr->get_reads_from() == act->get_reads_from())
						add_future_value(curr, act);
				}
			}
		}
	}

	/*
	 * All compatible, thread-exclusive promises must be ordered after any
	 * concrete stores to the same thread, or else they can be merged with
	 * this store later
	 */
	for (unsigned int i = 0; i < promises.size(); i++)
		if (promises[i]->is_compatible_exclusive(curr))
			added = mo_graph->addEdge(curr, promises[i]) || added;

	return added;
}

//This procedure uses cohere to prune future values that are
//guaranteed to generate a coherence violation.
//
//need to see if there is (1) a promise for thread write, (2)
//the promise is sb before write, (3) the promise can only be
//resolved by the thread read, and (4) the promise has same
//location as read/write

bool ModelExecution::check_coherence_promise(const ModelAction * write, const ModelAction *read) {
	thread_id_t write_tid=write->get_tid();
	for(unsigned int i = promises.size(); i>0; i--) {
		Promise *pr=promises[i-1];
		if (!pr->same_location(write))
			continue;
		//the reading thread is the only thread that can resolve the promise
		if (pr->get_num_was_available_threads()==1 && pr->thread_was_available(read->get_tid())) {
			for(unsigned int j=0;j<pr->get_num_readers();j++) {
				ModelAction *prreader=pr->get_reader(j);
				//the writing thread reads from the promise before the write
				if (prreader->get_tid()==write_tid &&
						(*prreader)<(*write)) {
					if ((*read)>(*prreader)) {
						//check that we don't have a read between the read and promise
						//from the same thread as read
						bool okay=false;
						for(const ModelAction *tmp=read;tmp!=prreader;) {
							tmp=tmp->get_node()->get_parent()->get_action();
							if (tmp->is_read() && tmp->same_thread(read)) {
								okay=true;
								break;
							}
						}
						if (okay)
							continue;
					}
					return false;
				}
			}
		}
	}
	return true;
}


/** Arbitrary reads from the future are not allowed.  Section 29.3
 * part 9 places some constraints.  This method checks one result of constraint
 * constraint.  Others require compiler support. */
bool ModelExecution::thin_air_constraint_may_allow(const ModelAction *writer, const ModelAction *reader) const
{
	if (!writer->is_rmw())
		return true;

	if (!reader->is_rmw())
		return true;

	for (const ModelAction *search = writer->get_reads_from(); search != NULL; search = search->get_reads_from()) {
		if (search == reader)
			return false;
		if (search->get_tid() == reader->get_tid() &&
				search->happens_before(reader))
			break;
	}

	return true;
}

/**
 * Arbitrary reads from the future are not allowed. Section 29.3 part 9 places
 * some constraints. This method checks one the following constraint (others
 * require compiler support):
 *
 *   If X --hb-> Y --mo-> Z, then X should not read from Z.
 *   If X --hb-> Y, A --rf-> Y, and A --mo-> Z, then X should not read from Z.
 */
bool ModelExecution::mo_may_allow(const ModelAction *writer, const ModelAction *reader)
{
	SnapVector<action_list_t> *thrd_lists = obj_thrd_map.get(reader->get_location());
	unsigned int i;
	/* Iterate over all threads */
	for (i = 0; i < thrd_lists->size(); i++) {
		const ModelAction *write_after_read = NULL;

		/* Iterate over actions in thread, starting from most recent */
		action_list_t *list = &(*thrd_lists)[i];
		action_list_t::reverse_iterator rit;
		for (rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *act = *rit;

			/* Don't disallow due to act == reader */
			if (!reader->happens_before(act) || reader == act)
				break;
			else if (act->is_write())
				write_after_read = act;
			else if (act->is_read() && act->get_reads_from() != NULL)
				write_after_read = act->get_reads_from();
		}

		if (write_after_read && write_after_read != writer && mo_graph->checkReachable(write_after_read, writer))
			return false;
	}
	return true;
}

/**
 * Finds the head(s) of the release sequence(s) containing a given ModelAction.
 * The ModelAction under consideration is expected to be taking part in
 * release/acquire synchronization as an object of the "reads from" relation.
 * Note that this can only provide release sequence support for RMW chains
 * which do not read from the future, as those actions cannot be traced until
 * their "promise" is fulfilled. Similarly, we may not even establish the
 * presence of a release sequence with certainty, as some modification order
 * constraints may be decided further in the future. Thus, this function
 * "returns" two pieces of data: a pass-by-reference vector of @a release_heads
 * and a boolean representing certainty.
 *
 * @param rf The action that might be part of a release sequence. Must be a
 * write.
 * @param release_heads A pass-by-reference style return parameter. After
 * execution of this function, release_heads will contain the heads of all the
 * relevant release sequences, if any exists with certainty
 * @param pending A pass-by-reference style return parameter which is only used
 * when returning false (i.e., uncertain). Returns most information regarding
 * an uncertain release sequence, including any write operations that might
 * break the sequence.
 * @return true, if the ModelExecution is certain that release_heads is complete;
 * false otherwise
 */
bool ModelExecution::release_seq_heads(const ModelAction *rf,
		rel_heads_list_t *release_heads,
		struct release_seq *pending) const
{
	/* Only check for release sequences if there are no cycles */
	if (mo_graph->checkForCycles())
		return false;

	for ( ; rf != NULL; rf = rf->get_reads_from()) {
		ASSERT(rf->is_write());

		if (rf->is_release())
			release_heads->push_back(rf);
		else if (rf->get_last_fence_release())
			release_heads->push_back(rf->get_last_fence_release());
		if (!rf->is_rmw())
			break; /* End of RMW chain */

		/** @todo Need to be smarter here...  In the linux lock
		 * example, this will run to the beginning of the program for
		 * every acquire. */
		/** @todo The way to be smarter here is to keep going until 1
		 * thread has a release preceded by an acquire and you've seen
		 *	 both. */

		/* acq_rel RMW is a sufficient stopping condition */
		if (rf->is_acquire() && rf->is_release())
			return true; /* complete */
	};
	if (!rf) {
		/* read from future: need to settle this later */
		pending->rf = NULL;
		return false; /* incomplete */
	}

	if (rf->is_release())
		return true; /* complete */

	/* else relaxed write
	 * - check for fence-release in the same thread (29.8, stmt. 3)
	 * - check modification order for contiguous subsequence
	 *   -> rf must be same thread as release */

	const ModelAction *fence_release = rf->get_last_fence_release();
	/* Synchronize with a fence-release unconditionally; we don't need to
	 * find any more "contiguous subsequence..." for it */
	if (fence_release)
		release_heads->push_back(fence_release);

	int tid = id_to_int(rf->get_tid());
	SnapVector<action_list_t> *thrd_lists = obj_thrd_map.get(rf->get_location());
	action_list_t *list = &(*thrd_lists)[tid];
	action_list_t::const_reverse_iterator rit;

	/* Find rf in the thread list */
	rit = std::find(list->rbegin(), list->rend(), rf);
	ASSERT(rit != list->rend());

	/* Find the last {write,fence}-release */
	for (; rit != list->rend(); rit++) {
		if (fence_release && *(*rit) < *fence_release)
			break;
		if ((*rit)->is_release())
			break;
	}
	if (rit == list->rend()) {
		/* No write-release in this thread */
		return true; /* complete */
	} else if (fence_release && *(*rit) < *fence_release) {
		/* The fence-release is more recent (and so, "stronger") than
		 * the most recent write-release */
		return true; /* complete */
	} /* else, need to establish contiguous release sequence */
	ModelAction *release = *rit;

	ASSERT(rf->same_thread(release));

	pending->writes.clear();

	bool certain = true;
	for (unsigned int i = 0; i < thrd_lists->size(); i++) {
		if (id_to_int(rf->get_tid()) == (int)i)
			continue;
		list = &(*thrd_lists)[i];

		/* Can we ensure no future writes from this thread may break
		 * the release seq? */
		bool future_ordered = false;

		ModelAction *last = get_last_action(int_to_id(i));
		Thread *th = get_thread(int_to_id(i));
		if ((last && rf->happens_before(last)) ||
				!is_enabled(th) ||
				th->is_complete())
			future_ordered = true;

		ASSERT(!th->is_model_thread() || future_ordered);

		for (rit = list->rbegin(); rit != list->rend(); rit++) {
			const ModelAction *act = *rit;
			/* Reach synchronization -> this thread is complete */
			if (act->happens_before(release))
				break;
			if (rf->happens_before(act)) {
				future_ordered = true;
				continue;
			}

			/* Only non-RMW writes can break release sequences */
			if (!act->is_write() || act->is_rmw())
				continue;

			/* Check modification order */
			if (mo_graph->checkReachable(rf, act)) {
				/* rf --mo--> act */
				future_ordered = true;
				continue;
			}
			if (mo_graph->checkReachable(act, release))
				/* act --mo--> release */
				break;
			if (mo_graph->checkReachable(release, act) &&
			              mo_graph->checkReachable(act, rf)) {
				/* release --mo-> act --mo--> rf */
				return true; /* complete */
			}
			/* act may break release sequence */
			pending->writes.push_back(act);
			certain = false;
		}
		if (!future_ordered)
			certain = false; /* This thread is uncertain */
	}

	if (certain) {
		release_heads->push_back(release);
		pending->writes.clear();
	} else {
		pending->release = release;
		pending->rf = rf;
	}
	return certain;
}

/**
 * An interface for getting the release sequence head(s) with which a
 * given ModelAction must synchronize. This function only returns a non-empty
 * result when it can locate a release sequence head with certainty. Otherwise,
 * it may mark the internal state of the ModelExecution so that it will handle
 * the release sequence at a later time, causing @a acquire to update its
 * synchronization at some later point in execution.
 *
 * @param acquire The 'acquire' action that may synchronize with a release
 * sequence
 * @param read The read action that may read from a release sequence; this may
 * be the same as acquire, or else an earlier action in the same thread (i.e.,
 * when 'acquire' is a fence-acquire)
 * @param release_heads A pass-by-reference return parameter. Will be filled
 * with the head(s) of the release sequence(s), if they exists with certainty.
 * @see ModelExecution::release_seq_heads
 */
void ModelExecution::get_release_seq_heads(ModelAction *acquire,
		ModelAction *read, rel_heads_list_t *release_heads)
{
	const ModelAction *rf = read->get_reads_from();
	struct release_seq *sequence = (struct release_seq *)snapshot_calloc(1, sizeof(struct release_seq));
	sequence->acquire = acquire;
	sequence->read = read;

	if (!release_seq_heads(rf, release_heads, sequence)) {
		/* add act to 'lazy checking' list */
		pending_rel_seqs.push_back(sequence);
	} else {
		snapshot_free(sequence);
	}
}

/**
 * @brief Propagate a modified clock vector to actions later in the execution
 * order
 *
 * After an acquire operation lazily completes a release-sequence
 * synchronization, we must update all clock vectors for operations later than
 * the acquire in the execution order.
 *
 * @param acquire The ModelAction whose clock vector must be propagated
 * @param work The work queue to which we can add work items, if this
 * propagation triggers more updates (e.g., to the modification order)
 */
void ModelExecution::propagate_clockvector(ModelAction *acquire, work_queue_t *work)
{
	/* Re-check all pending release sequences */
	work->push_back(CheckRelSeqWorkEntry(NULL));
	/* Re-check read-acquire for mo_graph edges */
	work->push_back(MOEdgeWorkEntry(acquire));

	/* propagate synchronization to later actions */
	action_list_t::reverse_iterator rit = action_trace.rbegin();
	for (; (*rit) != acquire; rit++) {
		ModelAction *propagate = *rit;
		if (acquire->happens_before(propagate)) {
			synchronize(acquire, propagate);
			/* Re-check 'propagate' for mo_graph edges */
			work->push_back(MOEdgeWorkEntry(propagate));
		}
	}
}

/**
 * Attempt to resolve all stashed operations that might synchronize with a
 * release sequence for a given location. This implements the "lazy" portion of
 * determining whether or not a release sequence was contiguous, since not all
 * modification order information is present at the time an action occurs.
 *
 * @param location The location/object that should be checked for release
 * sequence resolutions. A NULL value means to check all locations.
 * @param work_queue The work queue to which to add work items as they are
 * generated
 * @return True if any updates occurred (new synchronization, new mo_graph
 * edges)
 */
bool ModelExecution::resolve_release_sequences(void *location, work_queue_t *work_queue)
{
	bool updated = false;
	SnapVector<struct release_seq *>::iterator it = pending_rel_seqs.begin();
	while (it != pending_rel_seqs.end()) {
		struct release_seq *pending = *it;
		ModelAction *acquire = pending->acquire;
		const ModelAction *read = pending->read;

		/* Only resolve sequences on the given location, if provided */
		if (location && read->get_location() != location) {
			it++;
			continue;
		}

		const ModelAction *rf = read->get_reads_from();
		rel_heads_list_t release_heads;
		bool complete;
		complete = release_seq_heads(rf, &release_heads, pending);
		for (unsigned int i = 0; i < release_heads.size(); i++)
			if (!acquire->has_synchronized_with(release_heads[i]))
				if (synchronize(release_heads[i], acquire))
					updated = true;

		if (updated) {
			/* Propagate the changed clock vector */
			propagate_clockvector(acquire, work_queue);
		}
		if (complete) {
			it = pending_rel_seqs.erase(it);
			snapshot_free(pending);
		} else {
			it++;
		}
	}

	// If we resolved promises or data races, see if we have realized a data race.
	checkDataRaces();

	return updated;
}

/**
 * Performs various bookkeeping operations for the current ModelAction. For
 * instance, adds action to the per-object, per-thread action vector and to the
 * action trace list of all thread actions.
 *
 * @param act is the ModelAction to add.
 */
void ModelExecution::add_action_to_lists(ModelAction *act)
{
	int tid = id_to_int(act->get_tid());
	ModelAction *uninit = NULL;
	int uninit_id = -1;
	action_list_t *list = get_safe_ptr_action(&obj_map, act->get_location());
	if (list->empty() && act->is_atomic_var()) {
		uninit = get_uninitialized_action(act);
		uninit_id = id_to_int(uninit->get_tid());
		list->push_front(uninit);
	}
	list->push_back(act);

	action_trace.push_back(act);
	if (uninit)
		action_trace.push_front(uninit);

	SnapVector<action_list_t> *vec = get_safe_ptr_vect_action(&obj_thrd_map, act->get_location());
	if (tid >= (int)vec->size())
		vec->resize(priv->next_thread_id);
	(*vec)[tid].push_back(act);
	if (uninit)
		(*vec)[uninit_id].push_front(uninit);

	if ((int)thrd_last_action.size() <= tid)
		thrd_last_action.resize(get_num_threads());
	thrd_last_action[tid] = act;
	if (uninit)
		thrd_last_action[uninit_id] = uninit;

	if (act->is_fence() && act->is_release()) {
		if ((int)thrd_last_fence_release.size() <= tid)
			thrd_last_fence_release.resize(get_num_threads());
		thrd_last_fence_release[tid] = act;
	}

	if (act->is_wait()) {
		void *mutex_loc = (void *) act->get_value();
		get_safe_ptr_action(&obj_map, mutex_loc)->push_back(act);

		SnapVector<action_list_t> *vec = get_safe_ptr_vect_action(&obj_thrd_map, mutex_loc);
		if (tid >= (int)vec->size())
			vec->resize(priv->next_thread_id);
		(*vec)[tid].push_back(act);
	}
}

/**
 * @brief Get the last action performed by a particular Thread
 * @param tid The thread ID of the Thread in question
 * @return The last action in the thread
 */
ModelAction * ModelExecution::get_last_action(thread_id_t tid) const
{
	int threadid = id_to_int(tid);
	if (threadid < (int)thrd_last_action.size())
		return thrd_last_action[id_to_int(tid)];
	else
		return NULL;
}

/**
 * @brief Get the last fence release performed by a particular Thread
 * @param tid The thread ID of the Thread in question
 * @return The last fence release in the thread, if one exists; NULL otherwise
 */
ModelAction * ModelExecution::get_last_fence_release(thread_id_t tid) const
{
	int threadid = id_to_int(tid);
	if (threadid < (int)thrd_last_fence_release.size())
		return thrd_last_fence_release[id_to_int(tid)];
	else
		return NULL;
}

/**
 * Gets the last memory_order_seq_cst write (in the total global sequence)
 * performed on a particular object (i.e., memory location), not including the
 * current action.
 * @param curr The current ModelAction; also denotes the object location to
 * check
 * @return The last seq_cst write
 */
ModelAction * ModelExecution::get_last_seq_cst_write(ModelAction *curr) const
{
	void *location = curr->get_location();
	action_list_t *list = obj_map.get(location);
	/* Find: max({i in dom(S) | seq_cst(t_i) && isWrite(t_i) && samevar(t_i, t)}) */
	action_list_t::reverse_iterator rit;
	for (rit = list->rbegin(); (*rit) != curr; rit++)
		;
	rit++; /* Skip past curr */
	for ( ; rit != list->rend(); rit++)
		if ((*rit)->is_write() && (*rit)->is_seqcst())
			return *rit;
	return NULL;
}

/**
 * Gets the last memory_order_seq_cst fence (in the total global sequence)
 * performed in a particular thread, prior to a particular fence.
 * @param tid The ID of the thread to check
 * @param before_fence The fence from which to begin the search; if NULL, then
 * search for the most recent fence in the thread.
 * @return The last prior seq_cst fence in the thread, if exists; otherwise, NULL
 */
ModelAction * ModelExecution::get_last_seq_cst_fence(thread_id_t tid, const ModelAction *before_fence) const
{
	/* All fences should have location FENCE_LOCATION */
	action_list_t *list = obj_map.get(FENCE_LOCATION);

	if (!list)
		return NULL;

	action_list_t::reverse_iterator rit = list->rbegin();

	if (before_fence) {
		for (; rit != list->rend(); rit++)
			if (*rit == before_fence)
				break;

		ASSERT(*rit == before_fence);
		rit++;
	}

	for (; rit != list->rend(); rit++)
		if ((*rit)->is_fence() && (tid == (*rit)->get_tid()) && (*rit)->is_seqcst())
			return *rit;
	return NULL;
}

/**
 * Gets the last unlock operation performed on a particular mutex (i.e., memory
 * location). This function identifies the mutex according to the current
 * action, which is presumed to perform on the same mutex.
 * @param curr The current ModelAction; also denotes the object location to
 * check
 * @return The last unlock operation
 */
ModelAction * ModelExecution::get_last_unlock(ModelAction *curr) const
{
	void *location = curr->get_location();
	action_list_t *list = obj_map.get(location);
	/* Find: max({i in dom(S) | isUnlock(t_i) && samevar(t_i, t)}) */
	action_list_t::reverse_iterator rit;
	for (rit = list->rbegin(); rit != list->rend(); rit++)
		if ((*rit)->is_unlock() || (*rit)->is_wait())
			return *rit;
	return NULL;
}

ModelAction * ModelExecution::get_parent_action(thread_id_t tid) const
{
	ModelAction *parent = get_last_action(tid);
	if (!parent)
		parent = get_thread(tid)->get_creation();
	return parent;
}

/**
 * Returns the clock vector for a given thread.
 * @param tid The thread whose clock vector we want
 * @return Desired clock vector
 */
ClockVector * ModelExecution::get_cv(thread_id_t tid) const
{
	return get_parent_action(tid)->get_cv();
}

/**
 * @brief Find the promise (if any) to resolve for the current action and
 * remove it from the pending promise vector
 * @param curr The current ModelAction. Should be a write.
 * @return The Promise to resolve, if any; otherwise NULL
 */
Promise * ModelExecution::pop_promise_to_resolve(const ModelAction *curr)
{
	for (unsigned int i = 0; i < promises.size(); i++)
		if (curr->get_node()->get_promise(i)) {
			Promise *ret = promises[i];
			promises.erase(promises.begin() + i);
			return ret;
		}
	return NULL;
}

/**
 * Resolve a Promise with a current write.
 * @param write The ModelAction that is fulfilling Promises
 * @param promise The Promise to resolve
 * @param work The work queue, for adding new fixup work
 * @return True if the Promise was successfully resolved; false otherwise
 */
bool ModelExecution::resolve_promise(ModelAction *write, Promise *promise,
		work_queue_t *work)
{
	ModelVector<ModelAction *> actions_to_check;

	for (unsigned int i = 0; i < promise->get_num_readers(); i++) {
		ModelAction *read = promise->get_reader(i);
		if (read_from(read, write)) {
			/* Propagate the changed clock vector */
			propagate_clockvector(read, work);
		}
		actions_to_check.push_back(read);
	}
	/* Make sure the promise's value matches the write's value */
	ASSERT(promise->is_compatible(write) && promise->same_value(write));
	if (!mo_graph->resolvePromise(promise, write))
		priv->hard_failed_promise = true;

	/**
	 * @todo  It is possible to end up in an inconsistent state, where a
	 * "resolved" promise may still be referenced if
	 * CycleGraph::resolvePromise() failed, so don't delete 'promise'.
	 *
	 * Note that the inconsistency only matters when dumping mo_graph to
	 * file.
	 *
	 * delete promise;
	 */

	//Check whether reading these writes has made threads unable to
	//resolve promises
	for (unsigned int i = 0; i < actions_to_check.size(); i++) {
		ModelAction *read = actions_to_check[i];
		mo_check_promises(read, true);
	}

	return true;
}

/**
 * Compute the set of promises that could potentially be satisfied by this
 * action. Note that the set computation actually appears in the Node, not in
 * ModelExecution.
 * @param curr The ModelAction that may satisfy promises
 */
void ModelExecution::compute_promises(ModelAction *curr)
{
	for (unsigned int i = 0; i < promises.size(); i++) {
		Promise *promise = promises[i];
		if (!promise->is_compatible(curr) || !promise->same_value(curr))
			continue;

		bool satisfy = true;
		for (unsigned int j = 0; j < promise->get_num_readers(); j++) {
			const ModelAction *act = promise->get_reader(j);
			if (act->happens_before(curr) ||
					act->could_synchronize_with(curr)) {
				satisfy = false;
				break;
			}
		}
		if (satisfy)
			curr->get_node()->set_promise(i);
	}
}

/** Checks promises in response to change in ClockVector Threads. */
void ModelExecution::check_promises(thread_id_t tid, ClockVector *old_cv, ClockVector *merge_cv)
{
	for (unsigned int i = 0; i < promises.size(); i++) {
		Promise *promise = promises[i];
		if (!promise->thread_is_available(tid))
			continue;
		for (unsigned int j = 0; j < promise->get_num_readers(); j++) {
			const ModelAction *act = promise->get_reader(j);
			if ((!old_cv || !old_cv->synchronized_since(act)) &&
					merge_cv->synchronized_since(act)) {
				if (promise->eliminate_thread(tid)) {
					/* Promise has failed */
					priv->failed_promise = true;
					return;
				}
			}
		}
	}
}

void ModelExecution::check_promises_thread_disabled()
{
	for (unsigned int i = 0; i < promises.size(); i++) {
		Promise *promise = promises[i];
		if (promise->has_failed()) {
			priv->failed_promise = true;
			return;
		}
	}
}

/**
 * @brief Checks promises in response to addition to modification order for
 * threads.
 *
 * We test whether threads are still available for satisfying promises after an
 * addition to our modification order constraints. Those that are unavailable
 * are "eliminated". Once all threads are eliminated from satisfying a promise,
 * that promise has failed.
 *
 * @param act The ModelAction which updated the modification order
 * @param is_read_check Should be true if act is a read and we must check for
 * updates to the store from which it read (there is a distinction here for
 * RMW's, which are both a load and a store)
 */
void ModelExecution::mo_check_promises(const ModelAction *act, bool is_read_check)
{
	const ModelAction *write = is_read_check ? act->get_reads_from() : act;

	for (unsigned int i = 0; i < promises.size(); i++) {
		Promise *promise = promises[i];

		// Is this promise on the same location?
		if (!promise->same_location(write))
			continue;

		for (unsigned int j = 0; j < promise->get_num_readers(); j++) {
			const ModelAction *pread = promise->get_reader(j);
			if (!pread->happens_before(act))
			       continue;
			if (mo_graph->checkPromise(write, promise)) {
				priv->hard_failed_promise = true;
				return;
			}
			break;
		}

		// Don't do any lookups twice for the same thread
		if (!promise->thread_is_available(act->get_tid()))
			continue;

		if (mo_graph->checkReachable(promise, write)) {
			if (mo_graph->checkPromise(write, promise)) {
				priv->hard_failed_promise = true;
				return;
			}
		}
	}
}

/**
 * Compute the set of writes that may break the current pending release
 * sequence. This information is extracted from previou release sequence
 * calculations.
 *
 * @param curr The current ModelAction. Must be a release sequence fixup
 * action.
 */
void ModelExecution::compute_relseq_breakwrites(ModelAction *curr)
{
	if (pending_rel_seqs.empty())
		return;

	struct release_seq *pending = pending_rel_seqs.back();
	for (unsigned int i = 0; i < pending->writes.size(); i++) {
		const ModelAction *write = pending->writes[i];
		curr->get_node()->add_relseq_break(write);
	}

	/* NULL means don't break the sequence; just synchronize */
	curr->get_node()->add_relseq_break(NULL);
}

/**
 * Build up an initial set of all past writes that this 'read' action may read
 * from, as well as any previously-observed future values that must still be valid.
 *
 * @param curr is the current ModelAction that we are exploring; it must be a
 * 'read' operation.
 */
void ModelExecution::build_may_read_from(ModelAction *curr)
{
	SnapVector<action_list_t> *thrd_lists = obj_thrd_map.get(curr->get_location());
	unsigned int i;
	ASSERT(curr->is_read());

	ModelAction *last_sc_write = NULL;

	if (curr->is_seqcst())
		last_sc_write = get_last_seq_cst_write(curr);

	/* Iterate over all threads */
	for (i = 0; i < thrd_lists->size(); i++) {
		/* Iterate over actions in thread, starting from most recent */
		action_list_t *list = &(*thrd_lists)[i];
		action_list_t::reverse_iterator rit;
		for (rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *act = *rit;

			/* Only consider 'write' actions */
			if (!act->is_write() || act == curr)
				continue;

			/* Don't consider more than one seq_cst write if we are a seq_cst read. */
			bool allow_read = true;

			if (curr->is_seqcst() && (act->is_seqcst() || (last_sc_write != NULL && act->happens_before(last_sc_write))) && act != last_sc_write)
				allow_read = false;
			else if (curr->get_sleep_flag() && !curr->is_seqcst() && !sleep_can_read_from(curr, act))
				allow_read = false;

			if (allow_read) {
				/* Only add feasible reads */
				mo_graph->startChanges();
				r_modification_order(curr, act);
				if (!is_infeasible())
					curr->get_node()->add_read_from_past(act);
				mo_graph->rollbackChanges();
			}

			/* Include at most one act per-thread that "happens before" curr */
			if (act->happens_before(curr))
				break;
		}
	}

	/* Inherit existing, promised future values */
	for (i = 0; i < promises.size(); i++) {
		const Promise *promise = promises[i];
		const ModelAction *promise_read = promise->get_reader(0);
		if (promise_read->same_var(curr)) {
			/* Only add feasible future-values */
			mo_graph->startChanges();
			r_modification_order(curr, promise);
			if (!is_infeasible())
				curr->get_node()->add_read_from_promise(promise_read);
			mo_graph->rollbackChanges();
		}
	}

	/* We may find no valid may-read-from only if the execution is doomed */
	if (!curr->get_node()->read_from_size()) {
		priv->no_valid_reads = true;
		set_assert();
	}

	if (DBG_ENABLED()) {
		model_print("Reached read action:\n");
		curr->print();
		model_print("Printing read_from_past\n");
		curr->get_node()->print_read_from_past();
		model_print("End printing read_from_past\n");
	}
}

bool ModelExecution::sleep_can_read_from(ModelAction *curr, const ModelAction *write)
{
	for ( ; write != NULL; write = write->get_reads_from()) {
		/* UNINIT actions don't have a Node, and they never sleep */
		if (write->is_uninitialized())
			return true;
		Node *prevnode = write->get_node()->get_parent();

		bool thread_sleep = prevnode->enabled_status(curr->get_tid()) == THREAD_SLEEP_SET;
		if (write->is_release() && thread_sleep)
			return true;
		if (!write->is_rmw())
			return false;
	}
	return true;
}

/**
 * @brief Get an action representing an uninitialized atomic
 *
 * This function may create a new one or try to retrieve one from the NodeStack
 *
 * @param curr The current action, which prompts the creation of an UNINIT action
 * @return A pointer to the UNINIT ModelAction
 */
ModelAction * ModelExecution::get_uninitialized_action(const ModelAction *curr) const
{
	Node *node = curr->get_node();
	ModelAction *act = node->get_uninit_action();
	if (!act) {
		act = new ModelAction(ATOMIC_UNINIT, std::memory_order_relaxed, curr->get_location(), params->uninitvalue, model_thread);
		node->set_uninit_action(act);
	}
	act->create_cv(NULL);
	return act;
}

static void print_list(const action_list_t *list)
{
	action_list_t::const_iterator it;

	model_print("------------------------------------------------------------------------------------\n");
	model_print("#    t    Action type     MO       Location         Value               Rf  CV\n");
	model_print("------------------------------------------------------------------------------------\n");

	unsigned int hash = 0;

	for (it = list->begin(); it != list->end(); it++) {
		const ModelAction *act = *it;
		if (act->get_seq_number() > 0)
			act->print();
		hash = hash^(hash<<3)^((*it)->hash());
	}
	model_print("HASH %u\n", hash);
	model_print("------------------------------------------------------------------------------------\n");
}

#if SUPPORT_MOD_ORDER_DUMP
void ModelExecution::dumpGraph(char *filename) const
{
	char buffer[200];
	sprintf(buffer, "%s.dot", filename);
	FILE *file = fopen(buffer, "w");
	fprintf(file, "digraph %s {\n", filename);
	mo_graph->dumpNodes(file);
	ModelAction **thread_array = (ModelAction **)model_calloc(1, sizeof(ModelAction *) * get_num_threads());

	for (action_list_t::const_iterator it = action_trace.begin(); it != action_trace.end(); it++) {
		ModelAction *act = *it;
		if (act->is_read()) {
			mo_graph->dot_print_node(file, act);
			if (act->get_reads_from())
				mo_graph->dot_print_edge(file,
						act->get_reads_from(),
						act,
						"label=\"rf\", color=red, weight=2");
			else
				mo_graph->dot_print_edge(file,
						act->get_reads_from_promise(),
						act,
						"label=\"rf\", color=red");
		}
		if (thread_array[act->get_tid()]) {
			mo_graph->dot_print_edge(file,
					thread_array[id_to_int(act->get_tid())],
					act,
					"label=\"sb\", color=blue, weight=400");
		}

		thread_array[act->get_tid()] = act;
	}
	fprintf(file, "}\n");
	model_free(thread_array);
	fclose(file);
}
#endif

/** @brief Prints an execution trace summary. */
void ModelExecution::print_summary() const
{
#if SUPPORT_MOD_ORDER_DUMP
	char buffername[100];
	sprintf(buffername, "exec%04u", get_execution_number());
	mo_graph->dumpGraphToFile(buffername);
	sprintf(buffername, "graph%04u", get_execution_number());
	dumpGraph(buffername);
#endif

	model_print("Execution trace %d:", get_execution_number());
	if (isfeasibleprefix()) {
		if (is_yieldblocked())
			model_print(" YIELD BLOCKED");
		if (scheduler->all_threads_sleeping())
			model_print(" SLEEP-SET REDUNDANT");
		if (have_bug_reports())
			model_print(" DETECTED BUG(S)");
	} else
		print_infeasibility(" INFEASIBLE");
	model_print("\n");

	print_list(&action_trace);
	model_print("\n");

	if (!promises.empty()) {
		model_print("Pending promises:\n");
		for (unsigned int i = 0; i < promises.size(); i++) {
			model_print(" [P%u] ", i);
			promises[i]->print();
		}
		model_print("\n");
	}
}

/**
 * Add a Thread to the system for the first time. Should only be called once
 * per thread.
 * @param t The Thread to add
 */
void ModelExecution::add_thread(Thread *t)
{
	unsigned int i = id_to_int(t->get_id());
	if (i >= thread_map.size())
		thread_map.resize(i + 1);
	thread_map[i] = t;
	if (!t->is_model_thread())
		scheduler->add_thread(t);
}

/**
 * @brief Get a Thread reference by its ID
 * @param tid The Thread's ID
 * @return A Thread reference
 */
Thread * ModelExecution::get_thread(thread_id_t tid) const
{
	unsigned int i = id_to_int(tid);
	if (i < thread_map.size())
		return thread_map[i];
	return NULL;
}

/**
 * @brief Get a reference to the Thread in which a ModelAction was executed
 * @param act The ModelAction
 * @return A Thread reference
 */
Thread * ModelExecution::get_thread(const ModelAction *act) const
{
	return get_thread(act->get_tid());
}

/**
 * @brief Get a Promise's "promise number"
 *
 * A "promise number" is an index number that is unique to a promise, valid
 * only for a specific snapshot of an execution trace. Promises may come and go
 * as they are generated an resolved, so an index only retains meaning for the
 * current snapshot.
 *
 * @param promise The Promise to check
 * @return The promise index, if the promise still is valid; otherwise -1
 */
int ModelExecution::get_promise_number(const Promise *promise) const
{
	for (unsigned int i = 0; i < promises.size(); i++)
		if (promises[i] == promise)
			return i;
	/* Not found */
	return -1;
}

/**
 * @brief Check if a Thread is currently enabled
 * @param t The Thread to check
 * @return True if the Thread is currently enabled
 */
bool ModelExecution::is_enabled(Thread *t) const
{
	return scheduler->is_enabled(t);
}

/**
 * @brief Check if a Thread is currently enabled
 * @param tid The ID of the Thread to check
 * @return True if the Thread is currently enabled
 */
bool ModelExecution::is_enabled(thread_id_t tid) const
{
	return scheduler->is_enabled(tid);
}

/**
 * @brief Select the next thread to execute based on the curren action
 *
 * RMW actions occur in two parts, and we cannot split them. And THREAD_CREATE
 * actions should be followed by the execution of their child thread. In either
 * case, the current action should determine the next thread schedule.
 *
 * @param curr The current action
 * @return The next thread to run, if the current action will determine this
 * selection; otherwise NULL
 */
Thread * ModelExecution::action_select_next_thread(const ModelAction *curr) const
{
	/* Do not split atomic RMW */
	if (curr->is_rmwr())
		return get_thread(curr);
	/* Follow CREATE with the created thread */
	if (curr->get_type() == THREAD_CREATE)
		return curr->get_thread_operand();
	return NULL;
}

/** @return True if the execution has taken too many steps */
bool ModelExecution::too_many_steps() const
{
	return params->bound != 0 && priv->used_sequence_numbers > params->bound;
}

/**
 * Takes the next step in the execution, if possible.
 * @param curr The current step to take
 * @return Returns the next Thread to run, if any; NULL if this execution
 * should terminate
 */
Thread * ModelExecution::take_step(ModelAction *curr)
{
	Thread *curr_thrd = get_thread(curr);
	ASSERT(curr_thrd->get_state() == THREAD_READY);

	ASSERT(check_action_enabled(curr)); /* May have side effects? */
	curr = check_current_action(curr);
	ASSERT(curr);

	if (curr_thrd->is_blocked() || curr_thrd->is_complete())
		scheduler->remove_thread(curr_thrd);

	return action_select_next_thread(curr);
}

/**
 * Launch end-of-execution release sequence fixups only when
 * the execution is otherwise feasible AND there are:
 *
 * (1) pending release sequences
 * (2) pending assertions that could be invalidated by a change
 * in clock vectors (i.e., data races)
 * (3) no pending promises
 */
void ModelExecution::fixup_release_sequences()
{
	while (!pending_rel_seqs.empty() &&
			is_feasible_prefix_ignore_relseq() &&
			haveUnrealizedRaces()) {
		model_print("*** WARNING: release sequence fixup action "
				"(%zu pending release seuqence(s)) ***\n",
				pending_rel_seqs.size());
		ModelAction *fixup = new ModelAction(MODEL_FIXUP_RELSEQ,
				std::memory_order_seq_cst, NULL, VALUE_NONE,
				model_thread);
		take_step(fixup);
	};
}
