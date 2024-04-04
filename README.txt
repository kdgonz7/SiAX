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
  instructions.


3.0 Instructions
=====
  - `00c0` : ALLOCH - Allocates a block on the memory chain.
  - `0046` : PUT    - Puts a byte into a memory chain block.

3.1 Instruction Permissions
=====
  System instructions are ran at the CPU VK level, meaning that the permissions
  these instructions possess are virtually endless, with the primary permission
  being that 