1.0 What is STAX?
======
  Stax is a VM designed to be strict and memory safe.

1.1 Exceptions
======
  A list of exceptions that STAX provides.

    - 758 - This code can only be returned by the `cpu_n0` function which occurs when the CPU does not exist.
    - 102 - Memory Permission Denied. When an allocation is made on a CPU that is not memory enabled (does not have a memory chain)
    - 399 - EOB (End of Bytecode)

1.2 CPU State
======
  The CPU states are ON, OFF, and WAITING, when the CPU is *waiting*, this means that an instruction has been called and is
  about to be called. This state can only be seen if the program is ran piece by piece, however, a majority of the time you will
  only see the ON and OFF states.

2.2
======

