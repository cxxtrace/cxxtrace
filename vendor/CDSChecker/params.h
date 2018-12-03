#ifndef __PARAMS_H__
#define __PARAMS_H__

/**
 * Model checker parameter structure. Holds run-time configuration options for
 * the model checker.
 */
struct model_params {
	int maxreads;
	int maxfuturedelay;
	bool yieldon;
	bool yieldblock;
	unsigned int fairwindow;
	unsigned int enabledcount;
	unsigned int bound;
	unsigned int uninitvalue;
	int maxexecutions;

	/** @brief Maximum number of future values that can be sent to the same
	 *  read */
	int maxfuturevalues;

	/** @brief Only generate a new future value/expiration pair if the
	 *  expiration time exceeds the existing one by more than the slop
	 *  value */
	unsigned int expireslop;

	/** @brief Verbosity (0 = quiet; 1 = noisy; 2 = noisier) */
	int verbose;

	/** @brief Command-line argument count to pass to user program */
	int argc;

	/** @brief Command-line arguments to pass to user program */
	char **argv;
};

#endif /* __PARAMS_H__ */
