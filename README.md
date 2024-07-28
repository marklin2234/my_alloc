# Project 3

## Implementation: Binned Free List

My approach was using the binned free list. Although there are better algorithms out there, bfl would be the easiest to implement
and we can improve upon it later.

In order to implement the bins, I allocate the first `BINNED_LIST_SIZE * SIZE_T_SIZE` bytes to be our free list, where each `SIZE_T_SIZE`
interval points to a list of free memory blocks of size `(1 << (i + 4))`. Each memory block has a header pointing to the next free
block, `1` if it is the end of the list and `0` if the memory block is allocated.

On initialization, there is only 1 block per bin. When trying to allocate a block of size $k$ that does not exist in the free list for its size,
it will attempt to break down a larger block of size $k'$ into blocks of size $k'-1, k'-2,\ldots, k, k$. When freeing memory, it will attempt 
to coalesce blocks of the same size together (so it can fit into the next bin size).

Finally, when no blocks of size $>= k$ exist, we increase the heap size using `mem_sbrk` and add 1 new block to each bin. 

An optimization I can do is to keep track of the most used memory blocks, and allocate more for those bin sizes instead to increase
throughput of the allocator.


## Performance

![](assets/imgs/performance.png)

Above, you can compare the performance of my allocator to the libc implementation. For the simple traces provided, it does quite well,
but there is a noticeable difference for `trace_c9_v0`. Taking a look at the trace, we notice that the user is trying to allocate many
blocks of increasing size. Since our program is capped to blocks of size $2^{24}$, the bottleneck is that we keep allocating 1 large block,
then we are required to call `mem_sbrk`, a slow syscall, to allocate more memory. This results in poor utilization, since free blocks in 
the lower bins are not being used, and also in poor throughput.

An implementation that can resolve this issue is to have wait and coalesce many smaller blocks into one larger block. Another implementation
where we keep a frequency counter for bin size utilization and allocate/coalesce more blocks for that bin can also improve performance.

The main bottleneck of this algorithm is poor spatial locality. Since the memory blocks are spread out extremely thin, we cannot utilize
cache very well. To offset this limitation, I've attempted to improve the spatial locality, for common trace patterns such as frequent
allocation and free, by inserting free blocks at the head of the list, so they will be the first to be allocated again. (Making our
free list more of a free stack).
