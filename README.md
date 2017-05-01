# Parallel Bucketsort

A parallel bucketsort using OpenMPI process level parallelism written using C.

There are two schools of parallel bucket sort, the simple version involves the main process placing the values to be sorted into buckets, while a more complete version uses each process to undertake this task, as well as sorting the sub arrays. This code uses the latter implementation.
