// stax-vm
// implemented here

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

/*
  What is StaxVM?
  
      a virtual machine format that is designed to be fast and eco-friendly.
      refer to the README.org for more information.
*/

// --- Headers --- 
#define INFO_HDR 0xAB

// --- Instructions ---
#define INT   0xEF        // INT [id] - calls an interrupt, a blocking execution of
                          // a certain functionality in the system. Interrupts have
                          // full access to the stack, and results can be pushed
                          // into the stack.

#define PUSH  0xAD        // PUSH [n] - pushes N into the stack.
#define POP   0xAC        // POP - pops an item from the stack and releases it from
                          // memory.
#define MAGIC_STOP 0xEFB  // To end the bytecode.

#define IVT_SIZE 199
#define MAX_EXCEPT 200

typedef struct CPU CPU;
typedef int (*ivtfn32)(CPU*);
typedef int byte;

typedef struct RollocNode {
  void * ptr;                 // the pointer to the memory
  int size;                   // size of the memory
  bool reusable;              // can this block of memory be reused?
  struct RollocNode * next;   // the next memory node
} RollocNode;

typedef struct RollocFreeList {
  RollocNode * root;
} RollocFreeList;

RollocFreeList * r_new_free_list() {
  RollocFreeList * list = malloc(sizeof(RollocFreeList));
  assert(list);
  list->root = NULL;

  return list;
}

RollocNode * r_node(int size) {
  assert(size > 0);
  RollocNode* n = malloc(sizeof(RollocNode));
  assert(n);

  n->ptr = malloc(size);
  n->size = size;
  n->reusable = false;
  n->next = NULL;
  
  return n;
}

RollocNode * r_new_chunk(RollocFreeList* freelist, size_t size) {
  assert(freelist);

  RollocNode* tmp = freelist->root;
  
  if (freelist->root == NULL) {
    tmp = r_node(size);
    freelist->root = tmp;
    
    return freelist->root;
  } else {
    while (tmp->next) {
      tmp = tmp->next;
    }
    tmp->next = r_node(size);
    tmp = tmp->next;

    return tmp;
  }
}

// O(n)
RollocNode * r_find_first_reusable(RollocFreeList* freelist) {
  // iterate over the list to find a reusable node

  RollocNode* tmp = freelist->root;

  while (tmp) {
    if (tmp->reusable) return tmp;
    tmp = tmp->next;
  }

  return NULL;
}

/* wrapper allocator function */
void* r_alloc(RollocFreeList* freelist, size_t size, bool usable) {
  RollocNode * f = r_find_first_reusable(freelist);

  if (f) {
    if (f->size >= size) {
      memset(f->ptr, 0, f->size);
      f->reusable = usable;
      return f->ptr;
    }
  }
  else {
    RollocNode* n = r_new_chunk(freelist, size);
    assert(n);
    n->reusable = usable;
    return n->ptr;
  }
  return NULL;
  // pointer -> as it's already added to the freelist.
}

/* reallocate 
 * check if there's a reusable chunk of a similar size.
 * otherwise reallocate the pointer. O(n) */
void* r_realloc(RollocFreeList* freelist, void* ptr, size_t newsize) {
  assert(freelist);

  RollocNode *tmp = freelist->root;

  while (tmp) {
    if (tmp->ptr == ptr) {
      tmp->ptr = realloc(tmp->ptr, newsize);
      assert(tmp->ptr);
      return tmp->ptr;
    }
  }

  return NULL;
}

/* Iterate through each node in the free list and free it. */
void r_free_list(RollocFreeList* freelist) {
  assert(freelist);

  RollocNode* tmp = freelist->root;
  RollocNode* two = NULL;

  while (tmp) {
    two = tmp->next;
    free(tmp->ptr);
    free(tmp);
    tmp = two;
  } 

  free(freelist);
}

// simple function to hash a string into a number
int cpu_hash(const char* N, size_t m) {
  int r = 1;

  while (*N) {
    printf("%d\n", *N);
    r = (r * (*N)) % m;
    (void)*N ++;
  }

  printf("%d\n", r);
  return r % m;
}

// An interrupt table, contains IVT_SIZE amount of interrupt hash addresses.
// See 2.21 "CPU Interrupts" for more information.
typedef struct vivt32 {
  ivtfn32 * ivt;      // the function table
  size_t ivt_size;    // size of the function table.
} vivt32;

// Creates a vector table.
vivt32 * createvt (size_t ivt_s) {
  vivt32 * vt = malloc(sizeof(vivt32));
  assert(vt);
  
  vt->ivt = calloc(ivt_s, sizeof(ivtfn32));
  vt->ivt_size = ivt_s;
  
  assert(vt->ivt);
  
  memset(vt->ivt, 0, vt->ivt_size * sizeof(ivtfn32));

  return vt;
}

// Maps FUNCTION in TABLE.
// Function must follow @ivtfn32 format.
// HASHED by INSTRUCTION_NAME
// DV - prints the address of the HASHED
//      instruction.
void ivt_map(vivt32 * table, ivtfn32 function, const char* instruction_name, bool dv) {
  assert(table);
  assert(table->ivt_size > 0);
  assert(table->ivt);

  int hash_id =  cpu_hash(instruction_name, table->ivt_size);

  if (dv) {
    printf("stax: [IVT]: hashed instruction '%s': %04x\n", instruction_name, hash_id);
  }

  // if the hashed id is greater than the tables actual size
  if (hash_id > table->ivt_size) {
    fprintf(stderr, "stax: [IVT]: table overflow, abort\n");
    abort(); // abort because we only need a limited amount of instructions.
  }

  assert(! table->ivt[hash_id]); // new instruction only
  table->ivt[hash_id] = function;
}

// the structure for managing bytes.
// holds a CPU as a reference to know where to go in the address.
//
// e.g [1,2,3,4]      order
//      0 1 2 3       pc    (CPU)
typedef struct {
  CPU * ref;
  byte * data;
  size_t data_size;
} Order;

// allocates a new ordering.
Order* ordering(CPU * cpu) {
  Order* od = malloc(sizeof(Order));
  assert(od);

  od->ref = cpu;
  od->data = NULL;
  od->data_size = 0;

  return od;
}

void ord_append(Order* order, byte * data, int size) {
  assert(order);

  int p = 0;

  // make data size bigger
  if (! order->data) {
    order->data = calloc(size, sizeof(byte));
    order->data_size = size;
  } else {
    order->data = realloc(order->data, order->data_size + size);
    order->data_size += size;
    p = order->data_size;
  }

  // offset order->data by p in case we are appending data.
  memcpy(order->data + p, data, size);
}



typedef enum cpu_state_t {
  OFF,
  WAITING,
  ON
} cpu_state_t;

// The CPU
//
// Contains methods to change its state, pc, and its IVT
struct CPU {
  cpu_state_t state;    // the state of the CPU
  int pc;               // the program counter
  bool executing;       // are we currently executing a binary?
  bool memory_enabled;  // is this CPU memory powered?
  bool verbose;         // should this CPU message any findings?

  vivt32 *ivt;           // hashes that hold certain actions, these are blocking

  RollocFreeList * memory_chain;  // contiguous list of memory allocated by the CPU as requested.

  int * cpes;                     // The exception Stack, you can view the last exception code, etc.
  int cpes_size;
  int cpes_cap;

  Order* internal;                // internal management.
};

// where the PC is currently.
byte ord_cur(Order* order) {
  assert(order);

  if (order->ref->pc > order->data_size) {
    return -1;
  }

  return order->data[order->ref->pc];
}




typedef int (*ivtfn32)(CPU*);

struct cpu_settings_t {
  bool allow_memory_allocation;     // Can additional memory be allocated?
  int max_memory_allocation_pool;   // The max amount of memory that can be allocated from anonymous requests. (Note: -1 disables this)
  bool silent;
};

// initializes a virtual CPU with settings `settings`
CPU * vcpu(struct cpu_settings_t settings) {
  CPU * cpu = malloc(sizeof(CPU));
  if (! cpu) {
    if (! settings.silent) {
      printf("stax: CPU could not be created.\n");
    }
    abort();
  }

  cpu->state = OFF;
  cpu->verbose = ! settings.silent;
  cpu->pc = 0;
  cpu->ivt = createvt(IVT_SIZE);
  cpu->memory_enabled = settings.allow_memory_allocation;

  cpu->cpes = calloc(MAX_EXCEPT, sizeof(int)); 
  assert(cpu->cpes);

  cpu->cpes_size = 0;
  cpu->cpes_cap = MAX_EXCEPT;

  cpu->internal = ordering(cpu);
  
  assert(cpu->internal);

  if (cpu->memory_enabled) {
    cpu->memory_chain = r_new_free_list();
    assert(cpu->memory_chain);

    if (cpu->verbose) {
      printf("stax: [CPU]: loaded volatile memory table\n");
    }
  }

  return cpu; // TODO 
}

// binding of ord_append
void cpu_exe(CPU* vcpu, byte* info, size_t size) {
  assert(vcpu);
  assert(info);

  ord_append(vcpu->internal, info, size);
}

// raises a CPU exception.
//
// This function is not throwable.
void cpu_raise(CPU* vcpu, int code) {
  assert(vcpu);

  // double the capacity
  if (vcpu->cpes_size >= vcpu->cpes_cap) 
  {
    vcpu->cpes_cap *= 2;
    vcpu->cpes = realloc(vcpu->cpes, vcpu->cpes_cap);
  }

  // set the last member to the code
  // move to next member
  vcpu->cpes[vcpu->cpes_size ++] = code;
}

// binding of ord_cur
byte cpu_cur(CPU* vcpu) {
  assert(vcpu);
  assert(vcpu->internal);

  if (vcpu->pc == vcpu->internal->data_size) {
    return -1;
  }
  
  return ord_cur(vcpu->internal);
}

// return current byte and move to the next one (move PC)
byte cpu_next1(CPU* vcpu) {
  assert(vcpu);
  assert(vcpu->internal);

  if (vcpu->pc > vcpu->internal->data_size) {
    if (vcpu->verbose) {
      printf("stax: [CPU]: EOB(399): end of bytecode\n");
    }
    cpu_raise(vcpu, 399);
    return 0;
  }

  byte n = cpu_cur(vcpu);

  vcpu->pc ++;

  return n;
}

// access the last CPU exception
//
// This function is not throwable.
int cpu_n0(CPU* vcpu) {
  if (! vcpu) return 758;
  return vcpu->cpes[vcpu->cpes_size - 1];
}

// allocate memory in the internal CPU memory chain.
// access will be DENIED if 
void* cpu_alloc(CPU* vcpu, size_t size) {
  if (! vcpu->memory_enabled) {
    if (vcpu->verbose) {
      fprintf(stderr, "stax: [CPU]: permission denied\n");
    }
    cpu_raise(vcpu, 102); // 102 - MPDENIED
    return NULL;
  }

  RollocNode* chunk = r_new_chunk(vcpu->memory_chain, size);
  assert(chunk);

  return (chunk);
}

// Turns CPU either ON or OFF
void cpu_toggle(CPU* vcpu) {
  assert(vcpu);

  vcpu->state = (vcpu->state == ON) ? OFF : ON;
}

// runs the loaded in bytecode based on the CPU's current settings. 
// Keeps all data in place, so added data can be reused. 
// The PC does not change.
// This function runs based on the IVT.
int cpu_ivtr0(CPU * vcpu) {
  assert(vcpu);
  assert(vcpu->internal);

  if (vcpu->state != ON) {
    return -1;
  }
  
  while (cpu_cur(vcpu) != MAGIC_STOP) {
    byte n = cpu_next1(vcpu);

    if (cpu_n0(vcpu) == 399 || n == -1) {
      if (vcpu->verbose) {
        printf("stax: [CPU]: EOB(399): premature end\n");
      }
      break;
    }

    if (vcpu->ivt->ivt[n] != NULL) {
      vcpu->state = WAITING;
      
      int prepc = vcpu->pc;

      vcpu->ivt->ivt[n](vcpu);

      int postpc = vcpu->pc - prepc;

      if (vcpu->verbose) {
        printf("stax: [CPU]: instruction '0x%.04X' completed; occupied %d bytes\n", n, postpc);
      }

      vcpu->state = ON;
    } else {
      if (vcpu->verbose) {
        printf("stax: [CPU]: note: dead code here (pc=%d)\n", vcpu->pc);
      }
    }
  }

  return 0;
}

int test_reusable_chunks(void) {
  RollocFreeList * list = r_new_free_list();

  RollocNode* chunk = r_new_chunk(list, 1);
  chunk->reusable = true;

  void* ptr2 = r_alloc(list, 1, true);
  void* ptr3 = r_realloc(list, ptr2, 2);
  // void* ptr4 = r_alloc(list, 1, true);

  assert(chunk->ptr);

  r_free_list(list);

  return 0;
}

int test_cpu_instruction_hash(void) {
  printf("hash1: 'DIE': %d\n", cpu_hash("DIE", 101));
  printf("hash1: 'DIE2': %d\n", cpu_hash("DIE2", 101));
  printf("hash1: 'DIE3': %d\n", cpu_hash("DIE3", 101));
  printf("hash1: 'DIE4': %d\n", cpu_hash("DIE4", 101));
  printf("hash1: 'DIE5': %d\n", cpu_hash("DIE5", 101));

  return 0;
}

int test ( CPU * cpu ) {  
  printf("Hello, world!\n");
  return 0;
}


int test_cpu_make(void) {
  struct cpu_settings_t settings;
  settings.silent = false;
  settings.allow_memory_allocation = true;
  settings.max_memory_allocation_pool = 1000;

  CPU * vcp = vcpu(settings);
  ivt_map(vcp->ivt, test, "TEST", true);

  assert(vcp->verbose);

  vcp->ivt->ivt[0x00AF](vcp);

  cpu_raise(vcp, 655);

  printf("%d\n", cpu_n0(vcp));

  byte * data = malloc(30 * sizeof (byte));

  data[0] = 0x00AF;
  data[1] = 3;
  data[2] = MAGIC_STOP;

  cpu_exe(vcp, data, 5);

  cpu_toggle(vcp);

  cpu_ivtr0(vcp);

  return 0;
}

// int test_order(void) { }

int main(void) {
  test_reusable_chunks();
  test_cpu_instruction_hash();
  test_cpu_make();
}

