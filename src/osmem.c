// SPDX-License-Identifier: BSD-3-Clause

#include "osmem.h"
#include "helpers.h"


struct block_meta *heap_start = NULL;
size_t flag = MMAP_THRESHOLD;

/* Finds the best free space for a given size if it exists */
void *find_best_space(size_t size) {
	struct block_meta *ptr = heap_start;
	struct block_meta *best = NULL;

	size_t padding_size;
	if (size % 8 == 0){
		padding_size = size;
	} else {
		padding_size = (size/8) * 8 + 8;
	}

	while (ptr != NULL) {
		if (ptr->status == STATUS_FREE && (padding_size <= ptr->size)) { // If there is enough space in the block
			if (best != NULL) { // if there is already a space, verify if it is smaller than that
				if (best->size > ptr->size) {
					// A new best is found
					best = ptr;
				}
			} else {
				best = ptr;
			}
		} // Verify if the last pointer is free in order to extend it and not to create another one
		else if (ptr->next == NULL && ptr->status == STATUS_FREE ) { 
			sbrk(padding_size - ptr->size);
			best = ptr;
			best->size = padding_size;
			return ptr;
		}
		ptr = ptr->next;
	} 
	return best;
}

/* Adding the heap and adding the new block to the end of the list */
struct block_meta *add_Space(size_t size) {
	size_t padding_size;
	if (size % 8 == 0){
		padding_size = size;
	} else {
		padding_size = (size/8) * 8 + 8;
	}
	struct block_meta *block = sbrk(padding_size + METADATA_SIZE);
	DIE(block == (void*) -1, "sbrk");

	block->size = padding_size;
	block->status = STATUS_ALLOC;
	block->next = NULL;

	struct block_meta *p = heap_start;
	while (p->next != NULL){
		p = p->next;
	}
	p->next = block;
	
	return block;
}

/* Parse the list and coalesce any blocks that are possible */
void coalesce() {
	if (heap_start == NULL)
		return;
	
	struct block_meta *p = heap_start, *next;
	while (p->next != NULL)
	{
		next = p->next;
		// Verify if 2 consecutive nods are adjent and if they can be merged
		if ( p->status == STATUS_FREE && next->status == STATUS_FREE &&
			(next == (struct block_meta*)((char*)p + p->size + METADATA_SIZE) )) {
			p->next = next->next;
			p->size += next->size + METADATA_SIZE;
		} else {
			p = p->next;
		}
	}
}

void *os_malloc(size_t size)
{
	/* TODO: Implement os_malloc */
	if (size <= 0) {
		return NULL;
	}

	size_t padding_size;
	if (size % 8 == 0) {
		padding_size = size;
	} else {
		padding_size = (size/8) * 8 + 8;
	}

	// If the size is smaller than 128kB (page size for calloc) alloc it using brk
	// or store it in an existing block
	if ( padding_size + METADATA_SIZE < flag ) {
		flag = MMAP_THRESHOLD;
		if (heap_start == NULL) {
			// Prealloc 128kB
			heap_start = sbrk(MMAP_THRESHOLD);
			DIE(heap_start == (void*) -1, "sbrk");

			// Set the used space with padding
			heap_start->size = padding_size;
			heap_start->status = STATUS_ALLOC;
			size_t total_size = heap_start->size + sizeof(struct block_meta);
		
			if (MMAP_THRESHOLD - total_size >= 32) {
				// Alloc another block with the remaining space of the preallocation if there is enough space left
				struct block_meta *block = (struct block_meta*)((char*)heap_start + heap_start->size + sizeof(struct block_meta));
				block->size = MMAP_THRESHOLD - sizeof(struct block_meta) - heap_start->size;
				block->status = STATUS_FREE;
				block->next = NULL;
				heap_start->next = block;


			} else { // else the first block takes all the space left
				heap_start->size = MMAP_THRESHOLD - METADATA_SIZE;
			}
			return (heap_start + 1); 
		}
		// Try to merge any adjent free blocks, than searches for the best block to allocate
		coalesce();
		struct block_meta *best = find_best_space(size);
		if (best != NULL) { // A free split block is found
			size_t remaining_size = best->size;
			best->size = padding_size;
			best->status = STATUS_ALLOC;
			
			// Verify if there is still space left for another block
			if (remaining_size - padding_size >= 32) {
				struct block_meta *block = (struct block_meta*)((char*)best + best->size + METADATA_SIZE);
				block->size = remaining_size - best->size - METADATA_SIZE;
				block->status = STATUS_FREE;
				block->next = best->next;
				best->next = block;
			} else {
				best->size = remaining_size;
			}
			return (best + 1);
		} else {
			struct block_meta *block = add_Space(size);
			return (block + 1);
		}

	} else { // Allocate it with mmap
		flag = MMAP_THRESHOLD;
		void *p = mmap(NULL,padding_size + sizeof(struct block_meta),PROT_READ | PROT_WRITE,MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		DIE(p == MAP_FAILED, "mmap");
		struct block_meta *aux = p;
		aux->size = padding_size;
		aux->next = NULL;
		aux->status = STATUS_MAPPED;
		return (aux + 1); 
	}
	return NULL;
}

void os_free(void *ptr)
{
	/* TODO: Implement os_free */
	if (ptr == NULL)
		return;
	
	struct block_meta *block = (struct block_meta*)(ptr - sizeof(struct block_meta));
	if (block->status == STATUS_MAPPED) {
		munmap(ptr - sizeof(struct block_meta), block->size + sizeof(struct block_meta));
	} 
	else {
		block->status = STATUS_FREE;
	}
}

void *os_calloc(size_t nmemb, size_t size)
{
	/* TODO: Implement os_calloc */
	if (nmemb <= 0 || size <= 0)
		return NULL;

	size_t page_size = getpagesize();
	flag = page_size;
	void *ptr = os_malloc(nmemb * size);
	memset(ptr,0,size);
	return ptr;
}

void *os_realloc(void *ptr, size_t size)
{
	/* TODO: Implement os_realloc */
	if (ptr == NULL){
		ptr = os_malloc(size);
		return ptr;
	}
	if (size == 0) {
		os_free(ptr);
		return NULL;
	}
	struct block_meta *block = (struct block_meta*)(ptr - METADATA_SIZE);
	if (block->status == STATUS_FREE) // Avoid undefined behavior
		return NULL;

	size_t padding_size = size;
	if (size % 8 == 0)
		padding_size = size;
	else
		padding_size = (size/8) * 8 + 8;

	// Size is smaller than original size
	if (padding_size <= block->size && padding_size < MMAP_THRESHOLD && block->size < MMAP_THRESHOLD) {
		if (block->size - padding_size >= 32) {
			// Create the new split block and populate its fields

			struct block_meta *split = (struct block_meta*)((char*)block + padding_size + METADATA_SIZE);
			split->size = block->size - padding_size - METADATA_SIZE;
			split->status = STATUS_FREE;
			split->next = block->next;

			// Modify the existing block
			block->size = padding_size;
			block->next = split;
			block->status = STATUS_ALLOC;
		} else {
		}
		return (block + 1);
	}
	// Coalesce all the possible blocks and verify if the block can be expanded or it should be reallocated
	coalesce();
	if( block->next != NULL && block->next->status == STATUS_FREE &&
	 (block->next == (struct block_meta*)((char*)block + block->size + METADATA_SIZE)) && padding_size <= (block->next->size + METADATA_SIZE + block->size)) {
		size_t initial_size = block->size;
		size_t free_size = block->next->size;
		block->next = block->next->next;

		// Split the block if it is the case
		if (initial_size + free_size + METADATA_SIZE - padding_size >= 32) {
			struct block_meta *split = (struct block_meta*)((char*)block + padding_size + METADATA_SIZE);
			split->size = initial_size + free_size - padding_size;
			split->status = STATUS_FREE;
			split->next = block->next;
			
			block->next = split;
			block->size = padding_size;
		} else {
			block->size = initial_size + free_size + METADATA_SIZE;
		}

		return (block + 1);
	}

	// If it reaches here, block cannot be expanded,
	// so it allocates another mem block and copies its information and free the previous one 
	size_t copy_size;
	if (size < block->size){
		copy_size = size;
	} else {
		copy_size = block->size;
	}
	void *new_ptr = os_malloc(size);
	if (new_ptr == NULL)
		return NULL;
	memcpy(new_ptr,ptr,copy_size);
	os_free(ptr);
	return new_ptr;
}

int main(void)
{
	void *ptr, *prealloc_ptr;

	prealloc_ptr = mock_preallocate();
	ptr = os_malloc(inc_sz_sm[0]);
	taint(ptr, inc_sz_sm[0]);

	/* Expand last block */
	for (int i = 0; i < NUM_SZ_SM; i++) {
		ptr = os_realloc_checked(ptr, inc_sz_sm[i]);
		taint(ptr, inc_sz_sm[i]);
	}

	/* Cleanup */
	os_realloc_checked(ptr, 0);
	os_realloc_checked(prealloc_ptr, 0);

	return 0;
}
