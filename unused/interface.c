/*
 * Interface(s)
 */
enum class AccessMode {
	NoAccess,
	ReadOnly,
	ReadWrite
};


/************************************
 * Thread version
 * Assumes clean DRF accesses to region (so needs a control synchronisation system to ensure these DRF accesses)
 */

/* All-in-one : return when all these regions are cached in the right mode.
 * (send queries for not-ready regions, and wait for answers)
 */
void region_ensure_accesses (void ** regions, enum AccessMode * modes, int nb_region);

// Cache flush if needed (mppa)
void region_access_finish (void ** regions, enum AccessMode * modes, int nb_region);

/* Region splitting/merge
 *
 * r
 * |----+----+----|
 *   cb[0] cb[1]
 */

// Require ReadWrite access for region
void region_split_irregular (void * region, int * chunk_bounds, int nb_chunk);
void region_split_regular (void * region, int chunk_size);
// Require ReadWrite access for all chunks
void region_merge (void * region);

/* Region creation / destruction
 */
void * region_create (size_t size);
void region_destroy (void * region); // require ReadWrite access

/* Finer grained stuff
 */
enum AccessMode region_available_mode (void * region); // check cached version mode
void region_require_access (void * region, enum AccessMode mode);
void region_wait_access (void * region, enum AccessMode mode);

/* ---------------- CPP one ----------- */

//////////////////////////////////

struct Gas {
	int node_id_;
	int nb_threads_;

	/***** Interface *******/

	/* Region creation/destruction (equivalent to malloc / posix_memalign / free)
	 * Take a thread_id parameter, will be initialised if -1
	 */
	void * region_create (size_t size, int & tid);                           // local
	void * region_create_aligned (size_t size, size_t alignment, int & tid); // local
	void region_destroy (void * ptr, int & tid);                             // may call gas if remote copies

	/* Split/merge
	 * always take the pointer to the block itself
	 */
	void split (void * ptr, size_t nb_piece); // will notify remotes
	void merge (void * ptr);                  // will notify remotes

	/* Extended interface: "lazy_create"
	 * Will map to normal create for small blocks.
	 * Delayed malloc for bigger ones (multi superpages)
	 */
	void * region_create_noalloc (size_t size, int & tid); // local
	void region_delayed_alloc (void * ptr);                //

	/* Threaded model coherence interface
	 * Assumes DRF accesses
	 */
	enum AccessMode { NoAccess, ReadOnly, ReadWrite };
	void region_ensure_accesses (void ** regions, AccessMode * modes, int nb_region);

	/***** Internals ******/

	// RegionHeader * region_header (void * ptr);
};

////////////////////////////////
