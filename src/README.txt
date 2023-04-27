// Toma Bogdan-Nicolae
// 333CB

/*
            MALLOC:
    os_malloc verifies if the size provided is greater than 0, then apply padding to it
(it needs to be a multiple of 8). There are 2 ways of allocating the memmory:
- with mmap (when its greater than MMAP_THRESHOLD) and will not be linked with other blocks
of memmory
- with brk, in which case the blocks of memmory are linked to each other and can be reused,
merged or splited.

    When using brk, we prealloc the heap with 128kB and when the first call of malloc happens,
we split the 128kB into the sized that is requested and the difference stored as STATUS_FREE.
The preallocation is the head of the "heap", other blocks (or splited) will be stored using a linked
list (struct block_meta, which address will be right before the start of malloc memmory). If a size is
requested that is bigger then the rest of the prealloc heap, then we search with
"find_best_space" function. Also, before calling the function we try to merge all the adjent free spaces
in order to optimize and make less brk calls (coalesce function).

find_best_space - goes through the heap list to find the minimum free space greater than the size
                  provided. If the function returns null than there is no space left and we allocate
                  it with brk (or if the last block is free, we increment the space of the last block
                  instead of creating another one).
                - If it does return a block, we verify if the remaining size can create another block.
                  For this to happen, remaining size should be >= than 32 (24B for structureand + 1B + 7(padding) B) 

coalesce - searches through the list to find nodes that are have adjent addresses and are both free to
           truncate them and form one bigger block

            FREE:
    Checks if the pointer is null and it verifies its STATUS by accessing the block meta with ptr - sizeof struct:
STATUS_ALLOC - it is a reusable block allocated with brk and only changes the status as STATUS_FREE
STATUS_MAPPED - it is non reusable block allocated with mmap, that is freed using munmap and its size.

            CALLOC:
    Verifies if number of elements and size parameters are valid, than get the page size and calls malloc
with number of el * size, than sets all the memmory with 0.


            REALLOC:
    Checks if the pointer is null (calls simple malloc), size is greater than 0 (if not calls free) and if
status != STATUS_FREE (returns null cause its undefined behavior).
    After that, first it checks if the size is smaller than the previous allocated one. In this case we either
split the block if the size is smaller enough, or we leave the size field unmodified.
    If the size is bigger, then first checks if the current block can be expanded to a adjent free block.
Else, it is the basic case where we use the previous implemented malloc to create a new pointer with the new
size, and copies its information using memcpy, than frees old pointer and returns the new one.     
*/  