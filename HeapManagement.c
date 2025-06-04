#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEAP_SIZE 1836311903  
#define MAX_FIB_COUNT 50  

// Global Fibonacci table and count
size_t fib_numbers[MAX_FIB_COUNT];
int fib_count = 0;

// Global pointer to heap that is the basee adress
void *heap_start = NULL;
// The usable simulated heap size (set to HEAP_SIZE)
size_t simulated_heap_size = 0;
typedef struct BlockHeader {
    size_t size;          // Total size 
    size_t req_size;      // The original size asked
    int fib_index;        // Index in the Fibonacci table
    int is_free;          // 1 if free, 0 if not 
    struct BlockHeader *next; 
    struct BlockHeader *prev; 
} BlockHeader;

// The free doubly linked list that is maintained by the memory address
BlockHeader *free_list_head = NULL;

// Function prototypes 
void init_fib_table();
void init_heap();
void insert_free_block(BlockHeader *block);
void remove_free_block(BlockHeader *block);
void print_free_list();
void *simulate_malloc(size_t size);
void simulate_free(void *ptr);
void try_merge(BlockHeader *block);

void init_fib_table() {
    fib_numbers[0] = 1;
    fib_numbers[1] = 2;
    fib_count = 2;
    while (1) {
        size_t next = fib_numbers[fib_count - 1] + fib_numbers[fib_count - 2];
        if (next > HEAP_SIZE)
            break;
        fib_numbers[fib_count] = next;
        fib_count++;
        if (fib_count >= MAX_FIB_COUNT)
            break;
    }
    
    printf("Fibonacci table: ");
    for (int i = 0; i < fib_count; i++) {
        printf("%zu ", fib_numbers[i]);
    }
    printf("\n");
}

void init_heap() {
    heap_start = malloc(HEAP_SIZE);
    if (!heap_start) {
        printf("Failed to allocate simulated heap.\n");
        exit(1);
    }
    // Find the largest Fibonacci number <= HEAP_SIZE.
    int largest_index = fib_count - 1;
    while (largest_index >= 0 && fib_numbers[largest_index] > HEAP_SIZE) {
        largest_index--;
    }
    if (largest_index < 0) {
        printf("No suitable Fibonacci block found for heap size.\n");
        exit(1);
    }
    simulated_heap_size = fib_numbers[largest_index];
    
    // Create one free block covering the simulated heap.
    BlockHeader *initial_block = (BlockHeader *)heap_start;
    initial_block->size = simulated_heap_size;
    initial_block->req_size = 0;
    initial_block->fib_index = largest_index;  // fibonacci number of largest index is the  simulated_heap_size
    initial_block->is_free = 1;
    initial_block->next = initial_block->prev = NULL;
    free_list_head = initial_block;
    
    printf("Initialized heap with block at %p, size: %zu (Fib index: %d)\n", 
           (void *)initial_block, initial_block->size, initial_block->fib_index);
}

void insert_free_block(BlockHeader *block) {
    block->is_free = 1;
    block->next = block->prev = NULL;
    if (free_list_head == NULL) {
        free_list_head = block;
        return;
    }
    BlockHeader *current = free_list_head;
    BlockHeader *prev = NULL;
    // check the list until we find the correct position of address 
    while (current && current < block) {
        prev = current;
        current = current->next;
    }
    block->next = current;
    block->prev = prev;
    if (prev)
        prev->next = block;
    else
        free_list_head = block;
    if (current)
        current->prev = block;
}

void remove_free_block(BlockHeader *block) {
    if (block->prev)
        block->prev->next = block->next;
    else
        free_list_head = block->next;
    if (block->next)
        block->next->prev = block->prev;
    block->next = block->prev = NULL;
}

void print_free_list() {
    printf("Free list:\n");
    BlockHeader *current = free_list_head;
    while (current) {
        printf("  Block at %p, size: %zu, Fib index: %d\n", 
               (void *)current, current->size, current->fib_index);
        current = current->next;
    }
}
//     If a free block larger than needed is found, remove it and
//     split it into two Fibonacci blocks using the identity Fn = Fn-1 + Fn-2
//     The left block Fn-1 remains at the original address,
//     and the right block Fn-2 immediately follows.

void *simulate_malloc(size_t size) {
    // Calculate total required size (include header overhead)
    size_t total_req = size + sizeof(BlockHeader);
    
    // Find the smallest Fibonacci number >= total_req.
    int target_index = -1;
    for (int i = 0; i < fib_count; i++) {
        if (fib_numbers[i] >= total_req) {
            target_index = i;
            break;
        }
    }
    if (target_index == -1) {
        printf("Requested size too large.\n");
        return NULL;
    }
    
    // Look for a free block with the exact Fibonacci index.
    BlockHeader *block = NULL;
    BlockHeader *current = free_list_head;
    while (current) {
        if (current->fib_index == target_index) {
            block = current;
            break;
        }
        current = current->next;
    }
    
    // If not found, split a larger block until one becomes available.
    while (block == NULL) {
        // Find a free block with a larger Fibonacci index.
        current = free_list_head;
        BlockHeader *larger = NULL;
        while (current) {
            if (current->fib_index > target_index) {
                larger = current;
                break;
            }
            current = current->next;
        }
        if (larger == NULL) {
            // No suitable block available for splitting.
            printf("Allocation failed: no suitable block available.\n");
            return NULL;
        }
        // Remove the larger block from the free list.
        remove_free_block(larger);
        
        // Can only split if the block's Fibonacci index is at least 2.
        if (larger->fib_index < 2) {
            printf("Error: block too small to split further.\n");
            return NULL;
        }
        
        // Left block: size = fib_numbers[n-1], fib_index = n-1
        // Right block: size = fib_numbers[n-2], fib_index = n-2
        int left_index = larger->fib_index - 1;
        int right_index = larger->fib_index - 2;
        
        // Left block remains at the same address.
        BlockHeader *left_block = larger;
        left_block->size = fib_numbers[left_index];
        left_block->fib_index = left_index;
        left_block->is_free = 1;
        left_block->req_size = 0;
        left_block->next = left_block->prev = NULL;
        
        // Right block begins immediately after the left block.
        BlockHeader *right_block = (BlockHeader *)((char *)larger + fib_numbers[left_index]);
        right_block->size = fib_numbers[right_index];
        right_block->fib_index = right_index;
        right_block->is_free = 1;
        right_block->req_size = 0;
        right_block->next = right_block->prev = NULL;
        
        // Insert the two new blocks into the free list.
        insert_free_block(left_block);
        insert_free_block(right_block);
        
        // Try again to find a block with the exact target Fibonacci index.
        current = free_list_head;
        while (current) {
            if (current->fib_index == target_index) {
                block = current;
                break;
            }
            current = current->next;
        }
    }
    
    // Remove the found block from the free list and mark it as allocated.
    remove_free_block(block);
    block->is_free = 0;
    block->req_size = size;
    printf("Allocated block at %p, total size: %zu (Fib index: %d) for request: %zu\n",
           (void *)block, block->size, block->fib_index, size);
    
    // Return a pointer to the payload (immediately after the header).
    return (void *)(block + 1);
}

//  Simulates deallocation. Given a pointer returned by simulate_malloc,
//  retrieves its block header, marks it as free, inserts it into the free list,
//  and then attempts to merge it with any adjacent free blocks.

void simulate_free(void *ptr) {
    if (ptr == NULL)
        return;
    // Get the block header.
    BlockHeader *block = ((BlockHeader *)ptr) - 1;
    block->is_free = 1;
    printf("Freeing block at %p, size: %zu (Fib index: %d)\n",
           (void *)block, block->size, block->fib_index);
    
    // Insert this block into the free list.
    insert_free_block(block);
    
    // Attempt to merge with adjacent free blocks.
    try_merge(block);
}

// void try_merge(BlockHeader *block) {
//     BlockHeader *next = block->next;

//     // Try merging with the next block (right buddy)
//     if (next && next->is_free) {
//         if ((char *)block + block->size == (char *)next) {
//             // Ensure they are Fibonacci buddies (indices differ by exactly 1)
//             if (block->fib_index == next->fib_index + 1) {
//                 printf("Merging blocks at %p and %p\n", (void *)block, (void *)next);
                
//                 // Remove next block from the free list
//                 remove_free_block(next);
                
//                 // Merge into a single block
//                 block->size += next->size;
//                 block->fib_index = next->fib_index + 1;
//                 block->req_size = 0;  // Reset since it's a new free block
                
//                 // Attempt further merging
//                 try_merge(block);
//                 return;
//             }
//         }
//     }

//     // Try merging with the previous block (left buddy)
//     BlockHeader *prev = block->prev;
//     if (prev && prev->is_free) {
//         if ((char *)prev + prev->size == (char *)block) {
//             if (prev->fib_index == block->fib_index + 1) {
//                 printf("Merging blocks at %p and %p\n", (void *)prev, (void *)block);

//                 // Remove current block from free list
//                 remove_free_block(block);
                
//                 // Merge into a single block
//                 prev->size += block->size;
//                 prev->fib_index = block->fib_index + 1;
//                 prev->req_size = 0;  // Reset since it's a new free block
                
//                 // Attempt further merging
//                 try_merge(prev);
//             }
//         }
//     }
// }
void try_merge(BlockHeader *block) {
    // Flag to track if any merges occurred
    int merged = 1;
    
    // Continue merging as long as we find mergeable blocks
    while (merged) {
        merged = 0;
        
        // Try to merge with right neighbor
        BlockHeader *right = NULL;
        char *right_addr = (char *)block + block->size;
        
        // Find if the address corresponds to a block in the free list
        BlockHeader *current = free_list_head;
        while (current) {
            if ((char *)current == right_addr && current->is_free) {
                right = current;
                break;
            }
            current = current->next;
        }
        
        // Check if they form a Fibonacci buddy pair
        if (right && block->fib_index > 0 && right->fib_index > 0) {
            // For Fibonacci buddies, verify that:
            // 1. block->size = fib_numbers[n]
            // 2. right->size = fib_numbers[n-1]
            // 3. After merging, the size should be fib_numbers[n+1]
            int left_idx = block->fib_index;
            int right_idx = right->fib_index;
            
            // Check if they are consecutive Fibonacci buddies
            if (left_idx == right_idx + 1 && 
                block->size == fib_numbers[left_idx] && 
                right->size == fib_numbers[right_idx]) {
                
                printf("Merging blocks at %p and %p\n", (void *)block, (void *)right);
                
                // Remove right block from the free list
                remove_free_block(right);
                
                // Update block size and Fibonacci index
                block->size = fib_numbers[left_idx + 1];  // fib_numbers[n+1]
                block->fib_index = left_idx + 1;
                block->req_size = 0;  // Reset as it's now a free block
                
                // Reinsert the merged block to maintain address order
                remove_free_block(block);
                insert_free_block(block);
                
                merged = 1;
                continue;  // Skip to next iteration to check for more merges
            }
        }
        
        // Try to merge with left neighbor
        BlockHeader *left = NULL;
        
        // Find the potential left buddy (block immediately before current block)
        current = free_list_head;
        while (current) {
            if ((char *)current + current->size == (char *)block && current->is_free) {
                left = current;
                break;
            }
            current = current->next;
        }
        
        // Check if they form a Fibonacci buddy pair
        if (left && left->fib_index > 0 && block->fib_index > 0) {
            int left_idx = left->fib_index;
            int right_idx = block->fib_index;
            
            // Check if they are consecutive Fibonacci buddies
            if (left_idx == right_idx + 1 && 
                left->size == fib_numbers[left_idx] && 
                block->size == fib_numbers[right_idx]) {
                
                printf("Merging blocks at %p and %p\n", (void *)left, (void *)block);
                
                // Remove block from the free list
                remove_free_block(block);
                
                // Update left block size and Fibonacci index
                left->size = fib_numbers[left_idx + 1];  // fib_numbers[n+1]
                left->fib_index = left_idx + 1;
                left->req_size = 0;  // Reset as it's now a free block
                
                // Reinsert the merged block to maintain address order
                remove_free_block(left);
                insert_free_block(left);
                
                // Continue with the left block for further merges
                block = left;
                merged = 1;
            }
        }
    }
}
int main() {
    init_fib_table();
    init_heap();

    void *a, *b, *c, *d, *e;
    size_t size;

    printf("Enter size for allocation a: ");
    scanf("%zu", &size);
    a = simulate_malloc(size);
    printf("Memory allocated at: %p\n", a);

    printf("Enter size for allocation b: ");
    scanf("%zu", &size);
    b = simulate_malloc(size);
    printf("Memory allocated at: %p\n", b);

    printf("Enter size for allocation c: ");
    scanf("%zu", &size);
    c = simulate_malloc(size);
    printf("Memory allocated at: %p\n", c);

    printf("Enter size for allocation d: ");
    scanf("%zu", &size);
    d = simulate_malloc(size);
    printf("Memory allocated at: %p\n", d);

    printf("Enter size for allocation e: ");
    scanf("%zu", &size);
    e = simulate_malloc(size);
    printf("Memory allocated at: %p\n", e);

    print_free_list(); // Show free list after allocations

    // Free all allocated memory
    printf("\nFreeing all allocations...\n");
    simulate_free(a);
    printf("Freed memory at: %p\n", a);
    simulate_free(b);
    printf("Freed memory at: %p\n", b);
    simulate_free(c);
    printf("Freed memory at: %p\n", c);
    simulate_free(d);
    printf("Freed memory at: %p\n", d);
    simulate_free(e);
    printf("Freed memory at: %p\n", e);

    print_free_list(); // Show free list after deallocations

    // Free heap memory
    free(heap_start);
    return 0;
}