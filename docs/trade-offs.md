# Trade-offs

## Storage access

### Global

This strategy is implemented by [Skia's ChromeTracingTracer][skia-ctt].

+ Design is easy to understand and implement
+ Memory usage is bounded
- Parallel threads cause high contention
- Each sample must store the ID of its originating thread

### Thread-local

This strategy is implemented by [Chromium's TraceLog][chromium-trace] and by
[easy_profiler][easy_profiler].

+ Parallel threads cause no contention
+ Each sample need not store the ID of its thread
- Many threads cause high memory usage
- Design is easy to understand, but difficult to implement (e.g. aggregation)

### Processor-affine

+ Memory usage is bounded
+ Parallel threads cause little or no contention
- Design is difficult to implement
- Each sample must store the ID of its originating thread

### Processor-local

This strategy is implemented by [LTTng-UST][lttng-ust].

+ Memory usage is bounded
+ Parallel threads cause no contention
+ With OS-supplied context switch data, each sample need not store the ID of its
  thread
- Design is difficult to implement
- Without OS-supplied context switch data, each sample must store the ID of its
  originating thread
- Requires OS support

## Storage data structure

### Vector of samples

This strategy is implemented by [easy_profiler][easy_profiler].

    vector<sample> samples;

+ Appending has low overhead
+ Design is easy to understand and implement
- Appending overhead has high variance
- Variable-length samples are difficult to implement
- Async signal safety is difficult to implement

### Ring queue of samples

    size_t read_index;
    size_t write_index;
    array<sample, capacity> samples;

+ Appending has very low overhead
+ Appending has constant overhead
+ Design is relatively easy to understand and implement
- Variable-length samples are difficult to implement
- Async signal safety is difficult to implement

### Ring queue of sample arrays

    struct block_pointer {
        unique_ptr<sample[]> data;
        size_t capacity;
        size_t size;
    };
    size_t read_index;
    size_t write_index;
    array<block_pointer, capacity> blocks;

This strategy is implemented by [Phosphor][phosphor].

+ Appending has low overhead
+ Appending has bounded overhead
+ Design is relatively easy to understand and implement
- Variable-length samples are difficult to implement
- Async signal safety is difficult to implement

### Ring queue of byte arrays

This strategy is implemented by [Chromium's TraceLog][chromium-trace] and by
[LTTng-UST][lttng-ust].

    struct block_pointer {
        unique_ptr<byte[]> data;
        size_t capacity;
        size_t size;
    };
    size_t read_index;
    size_t write_index;
    array<block_pointer, capacity> blocks;

+ Appending has low overhead
+ Appending has bounded overhead
+ Design is relatively easy to understand and implement
+ Variable-length samples are easy to implement
- Async signal safety is difficult to implement

### Vector of byte arrays

    struct block_pointer {
        unique_ptr<byte[]> data;
        size_t capacity;
        size_t size;
    };
    vector<block_pointer> old_blocks;
    block_pointer newest_block;

This strategy is implemented by [Skia's ChromeTracingTracer][skia-ctt].

+ Appending has low overhead
+ Design is easy to understand and implement
+ Variable-length samples are easy to implement
- Appending overhead has high variance
- Async signal safety is difficult to implement

## Timestamp storage

On amd64/x86_64, high-resolution timestamps are calculated from three
components:

* The CPU's timestamp counter (provided by the `rdtsc` or `rdtscp` instruction)
* Timestamp counter offsets (provided by the OS)
* Timestamp counter frequency scale factors (provided by the OS)

The timestamp counter offsets and frequency scale factors can change at
any moment. This has two consequences:

* These variables are read when calculating the timestamp. Their values cannot
  be cached.
* These variables must be read atomically with the CPU's timestamp counter,
  typically requiring another variable which stores a generation number.

TODO: Investigate the trade-offs between `rdtsc` and `rdtscp`.

### tsc

+ The clock can represent timestamps across 100 years (assuming a 5 GHz clock)
+ Reading the clock requires minimal computation
+ The clock has reasonably low access time
+ The clock has high precision and sensitivity
- The clock has low accuracy (because frequency and offset are not adjusted)
- The clock has unknown frequency
- Timestamps may differ between processors (sockets)

### tsc + offset + scale

+ The clock can represent timestamps across 500 years
+ Reading the clock requires minimal computation
+ The clock has the best accuracy, precision, and sensitivity
- The clock has relatively high access time
- Storage is has two or more words of overhead
- Reading the clock requires atomic synchronization with auxiliary variables

### 64-bit nanosecond timestamp

+ Storage is 8 bytes
+ The clock can represent timestamps across 500 years
+ The clock has the best accuracy, precision, and sensitivity
- The clock has relatively high access time
- Reading clock requires some computation
- Reading the clock requires atomic synchronization with auxiliary variables

### 56-bit nanosecond timestamp with fixed offset

+ The clock can represent timestamps across 2 years
+ Storage is 7 bytes
+ The clock has the best accuracy, precision, and sensitivity
- The clock has relatively high access time
- Reading clock requires some computation
- Reading the clock requires atomic synchronization with auxiliary variables
- Timestamps cannot be compared across program invocations
- The clock requires extra initialization

### 56-bit nanosecond timestamp

+ The clock can represent timestamps across 1 year
+ Storage is 7 bytes
+ The clock has the best accuracy, precision, and sensitivity
- The clock has relatively high access time
- Reading clock requires some computation
- Reading the clock requires atomic synchronization with auxiliary variables
- Timestamps can wrap

### 32-bit millisecond timestamp

+ Storage is 4 bytes
+ The clock has the best accuracy and precision
- The clock can represent timestamps only across 49 days
- The clock has low sensitivity
- The clock has relatively high access time
- Reading clock requires some computation
- Reading the clock requires atomic synchronization with auxiliary variables

### Low-resolution timestamp

Operating systems provide a low-resolution timestamp, possibly updated on each
context switch.

+ Storage is 4 or 8 bytes
+ Reading the clock requires no computation
+ The clock has extremely low access time
- Timestamps have very low sensitivity
- Timestamps have very low precison

## Sample storage

### Queue per sample site

    struct site_data {
      queue q;
      czstring category;
      czstring name;
      sample_kind kind;
    };
    timestamp time;

+ Storage per sample is optimal (only timestamp and arguments)
+ High-throughput sample sites cannot discard other samples
+ Recording a sample has very low overhead
- Multiple sites increase memory usage
- Temporal locality of queue is lower when multiple sample sites are executed
- Sample sites require extra relocations during program load (if not PIC)

#### Pointer to sample site metadata per sample

    struct site_data {
      czstring category;
      czstring name;
      sample_kind kind;
    };
    timestamp time;
    site_data *meta;
    parameters data;

+ Storage per sample is relatively low
+ Recording a sample has relatively low overhead (loading a pointer)
- Sample sites require extra relocations during program load (if not PIC)

#### Pointer to sample site instruction per sample

    struct site_data {
      uint64_t instruction_pointer;
      czstring category;
      czstring name;
      sample_kind kind;
    };
    timestamp time;
    uint64_t instruction_pointer;
    parameters data;

+ Storage per sample is relatively low
+ Recording a sample has very low overhead
+ Sample sites require no extra relocations during program load
- Implementation is platform-specific
- Dynamically-loaded libraries complicate implementation

[chromium-trace]: https://chromium.googlesource.com/chromium/src/base/
[easy_profiler]: https://github.com/yse/easy_profiler
[lttng-ust]: https://lttng.org/man/3/lttng-ust/v2.10/
[phosphor]: http://couchbase.github.io/phosphor/
[skia-ctt]: https://skia.org/dev/tools/tracing
