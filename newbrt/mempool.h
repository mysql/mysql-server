/* a memory pool is a contiguous region of memory that supports single
   allocations from the pool.  these allocated regions are never recycled.
   when the memory pool no longer has free space, the allocated chunks 
   must be relocated by the application to a new memory pool. */ 

struct mempool {
    void *base;           /* the base address of the memory */
    int free_offset;      /* the offset of the memory pool free space */
    int size;             /* the size of the memory */
    int frag_size;        /* the size of the fragmented memory */
};

/* initialize the memory pool with the base address and size of a
   contiguous chunk of memory */
void mempool_init(struct mempool *mp, void *base, int size);

/* finalize the memory pool */
void mempool_fini(struct mempool *mp);

/* get the base address and size of the memory pool */
void mempool_get_base_size(struct mempool *mp, void **base_ptr, int *size_ptr);

/* get the amount of fragmented space in the memory pool */
int mempool_get_frag_size(struct mempool *mp);

/* allocate a chunk of memory from the memory pool suitably aligned */
void *mempool_malloc(struct mempool *mp, int size, int alignment);

/* free a previously allocated chunk of memory.  the free only updates
   a count of the amount of free space in the memory pool.  the memory
   pool does not keep track of the locations of the free chunks */
void mempool_mfree(struct mempool *mp, void *vp, int size);
