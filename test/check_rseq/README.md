# `check_rseq`

The `check_rseq` tool verifies that a program uses Linux restartable sequences
correctly.

[Linux restartable sequence (rseq)][linux-rseq] critical sections require
machine code to be structured in a specific way. For example, in practice, a
critical section may not contain a function call (e.g. an x86_64 `call`
instruction). `check_rseq` looks for problems like this by analyzing the
instructions in an ELF executable.

[linux-rseq]: https://www.efficios.com/blog/2019/02/08/linux-restartable-sequences/
