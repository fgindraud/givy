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

