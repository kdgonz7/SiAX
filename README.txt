1.0 What is STAX?
======
  Stax is a VM designed to be strict and memory safe.

1.1 Exceptions
======
  A list of exceptions that STAX provides.

    - 758 - This code can only be returned by the `cpu_n0` function 
            which occurs when the CPU does not exist.

    - 102 - Memory Permission Denied. When an allocation/action 
            is made on a CPU that is not memory enabled 
            (does not have a memory chain)

    - 744 - PUT_PAST_RANGE, when you attempt to use the PUT instruction
            in a block that can't go to the location.

    - 399 - EOB (End of Bytecode)

1.2 CPU State
======
  The CPU states are ON, OFF, and WAITING, when the CPU is *waiting*, this means that an instruction has 
  been called and is about to be called. This state can only be seen if the program is ran piece by piece,
  however, a majority of the time you will only see the ON and OFF states
  (depending on how fast your computer can process kernel-level instruction
  sequences)

1.3 CPU Structure
=====
  The CPU is organized with a standard reader to process sequencial
  instructions. It also contains an internal memory chain which isn't allocated
  if memory_enabled is disabled.

  Raw memory can be allocated by the CPU, however, certain sizes of objects may
  need to be considered, depending on the alignments needed.

  The ALLOC instruction allocates memory into a new block. And as for the CPU
  block structure the first block (0) is always reserved for data/information.
  It is NOT reusable. This is simply a block of memory which contains around 500
  * SIZEOF int bytes for storage. And should only be used with LOADSTR
  instructions, when you need to quickly access and load memory.

  The UNLOAD instruction unloads memory from the data block.

        >     LOADSTR 5 65 66 67 68 69      ; 'ABCDE' loaded into volatile block
        >     ...                           ; actions w/ volatile data
        >     UNLOAD                        ; remove all memory (reset)

3.0 Instructions
=====
  - `00c0` : ALLOCH - Allocates a block on the memory chain.
  - `0046` : PUT    - Puts a byte into a memory chain block.

3.1 Instruction Permissions
=====
  System instructions are ran at the CPU VK level, meaning that the permissions
  these instructions possess are virtually endless, with the primary permission
  being that 

4.0 SYSTEM-dependent instructions
=====
  WIP.
