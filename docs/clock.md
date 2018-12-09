# Clocks

## Definitions

**Clock accuracy**: The difference between the clock's rate and the rate of real
time.

**Clock access time**: The time needed to query the clock.

**Clock granularity**: A synonym of clock sensitivity.

**Clock precision**: The variance or standard deviation of samples. (TODO:
Define this term more precisely.)

**Clock resolution**: The smallest representible interval. [POSIX `time_t`][1] has a
resolution of 1 second. [Win32 FILETIME][2] has a resolution of 100 nanoseconds.
Some literature calls resolution "precision".

**Clock sensitivity**: The smallest measurable interval; the highest update
frequency. A 1000 Hz clock with perfect precision has a resolution of 1
millisecond. Some literature calls sensitivity "precision" or "resolution".

[1]: http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_types.h.html
[2]: https://docs.microsoft.com/en-gb/windows/desktop/api/minwinbase/ns-minwinbase-filetime
