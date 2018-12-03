/** @file promise.h
 *
 *  @brief Promise class --- tracks future obligations for execution
 *  related to weakly ordered writes.
 */

#ifndef __PROMISE_H__
#define __PROMISE_H__

#include <inttypes.h>

#include "modeltypes.h"
#include "mymemory.h"
#include "stl-model.h"

class ModelAction;
class ModelExecution;

struct future_value {
	uint64_t value;
	modelclock_t expiration;
	thread_id_t tid;
};

class Promise {
 public:
	Promise(const ModelExecution *execution, ModelAction *read, struct future_value fv);
	bool add_reader(ModelAction *reader);
	ModelAction * get_reader(unsigned int i) const;
	unsigned int get_num_readers() const { return readers.size(); }
	bool eliminate_thread(thread_id_t tid);
	void add_thread(thread_id_t tid);
	bool thread_is_available(thread_id_t tid) const;
	bool thread_was_available(thread_id_t tid) const;
	unsigned int max_available_thread_idx() const;
	bool has_failed() const;
	void set_write(const ModelAction *act) { write = act; }
	const ModelAction * get_write() const { return write; }
	int get_num_available_threads() const { return num_available_threads; }
	int get_num_was_available_threads() const { return num_was_available_threads; }
	bool is_compatible(const ModelAction *act) const;
	bool is_compatible_exclusive(const ModelAction *act) const;
	bool same_value(const ModelAction *write) const;
	bool same_location(const ModelAction *act) const;

	modelclock_t get_expiration() const { return fv.expiration; }
	uint64_t get_value() const { return fv.value; }
	struct future_value get_fv() const { return fv; }

	int get_index() const;

	void print() const;

	bool equals(const Promise *x) const { return this == x; }
	bool equals(const ModelAction *x) const { return false; }

	SNAPSHOTALLOC
 private:
	/** @brief The execution which created this Promise */
	const ModelExecution *execution;

	/** @brief Thread ID(s) for thread(s) that potentially can satisfy this
	 *  promise */
	SnapVector<bool> available_thread;
	SnapVector<bool> was_available_thread;

	int num_available_threads;
	int num_was_available_threads;

	const future_value fv;

	/** @brief The action(s) which read the promised future value */
	SnapVector<ModelAction *> readers;

	const ModelAction *write;
};

#endif
