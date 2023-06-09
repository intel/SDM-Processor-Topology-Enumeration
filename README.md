# Processor Topology Enumeration


## License
BSD Zero Clause License

Copyright (C) 2023 Intel Corporation

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. 
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, 
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.


This sample code is to be paired with the "Intel® 64 Architecture Processor Topology Enumeration Technical Paper" documnetation found on Intel.com.  The purpose of this code is to demonstrate how to enumerate the various CPU topologies using CPUID examples.

## Source List
### OS-Agnostic Files
 - **cpuid_topology.h** - This is the common header file included across all soruce files for sharing structures and shared functions.
 - **cpuid_topology.c** - The OS Agnostic entry point for this example which will parse and dispatch the command line options.
 - **cpuid_topology_display.c** - The OS Agnostic display APIs for presenting the topology details to the console display.
 - **cpuid_topology_file.c** - The OS Agnostic file APIs for saving/loading CPUID information for use across machines.
 - **cpuid_topology_parsecachetlb.c** - The OS Agnostic TLB and Cache topology parsing APIs.
 - **cpuid_topology_parsecpu.c** - The OS Agnostic processor topology APIs.
 - **cpuid_topology_tools.c** - The OS Agnostic set of support APIs which may funnel into OS-dependent APIs.

### OS-Dependent Files

The following files are the OS-Dependent APIs which only need to be included for the specific OS.

 - **linux_os_util.c** - The OS-Dependent APIs for Linux.
 - **win_os_util.c** - The OS-Dependent APIs for Windows.

## How to Compile

### Linux

You may use any compiler for linux that you wish to use.  The following example shows how to compile the sources for GCC using the command line.

```
        gcc -g -c -Wall linux_os_util.c
        gcc -g -c -Wall cpuid_topology_display.c
        gcc -g -c -Wall cpuid_topology_file.c
        gcc -g -c -Wall cpuid_topology_parsecachetlb.c
        gcc -g -c -Wall cpuid_topology_parsecpu.c
        gcc -g -c -Wall cpuid_topology_tools.c
        gcc -g  cpuid_topology.c -Wall -o cpu_topology64.out linux_os_util.o cpuid_topology_display.o cpuid_topology_file.o cpuid_topology_parsecachetlb.o  cpuid_topology_parsecpu.o cpuid_topology_tools.o
```

### Windows

You are free to use any compiler you need to use for Windows OS, including Visual Studio.  Included in the project is a SOURCES file that will work with versions of the WDK such as WDK 7.

 - **SOURCES** - OS File for the Windows WDK (Verified in WDK 7)

### Other Operating Systems

This project can easily be moved to another OS that supports a standard C runtime compiler.  The following APIs would need to be implemented for OS-Specific implemenation and compiled into the sources.

 - **void Os_DisplayTopology(void)**

 -- This API is simply a mechanism to demonstrate displaying the topology using an OS-Specific implementation for application purposes.

 - **void Os_Platform_Read_Cpuid(unsigned int Leaf, unsigned int Subleaf, PCPUID_REGISTERS pCpuidRegisters)**

 -- This implementation requests to execute the CPUID x86 instruction, it is in the OS-Specific section more for being a compiler-specific assembly implementation.

 - **unsigned int Os_GetNumberOfProcessors(void)**

 -- This requests to get the number of logical processors that are active for this application through the OS mechanisms. 

 - **void Os_SetAffinity(unsigned int ProcessorNumber)**
 - 
 -- This API requests to set affinity to a specific processor given an ordered processor number in the platform. 

## How to use the application

      
The following is the default help output from the compiled binary:

```
    Processor Topology Example.
       Command Line Options:

          H                  - Display this message
          S [File]           - Saves raw CPUID to a file.
          L [File] [COMMAND] - Loads raw CPUID from a file and perform a numbered COMMAND.
          C [COMMAND]        - Execute the numbered command from below.

       List of commands
          0 - Display the topology via OS APIs (Not valid with File Load)
          1 - Display the topology via CPUID
          2 - Display CPUID Leaf values one processor
          3 - Display CPUID Leaf values all processors
          4 - Display APIC ID layout
          5 - Display TLB Information
          6 - Display Cache Information
```

The usage is as follows, to run any of the commands 0 to 6 on the local system CPUID, you would use the following commands:

```
    CPUIDTOPOLOGY C 0
    CPUIDTOPOLOGY C 1
    CPUIDTOPOLOGY C 2
    CPUIDTOPOLOGY C 3
    CPUIDTOPOLOGY C 4
    CPUIDTOPOLOGY C 5
    CPUIDTOPOLOGY C 6
```
To save the current system CPUID into a file to view elsewhere, you can use the following:

```
    CPUIDTOPOLOGY S MyMachine.DAT
```

To load that file on any other system to view it with different commands, you can use the following commands:

```
    CPUIDTOPOLOGY L MyMachine.DAT 1
    CPUIDTOPOLOGY L MyMachine.DAT 2
    CPUIDTOPOLOGY L MyMachine.DAT 3
    CPUIDTOPOLOGY L MyMachine.DAT 4
    CPUIDTOPOLOGY L MyMachine.DAT 5
    CPUIDTOPOLOGY L MyMachine.DAT 6
```

A Simple Example:

```
    > CPUIDTOPOLOGY C 2
    Displaying CPUID Leafs 0, 1, 4, 0Bh, 018h, 01Fh if they exist
    *******************************
    Processor: 0
    Leaf 00000000 Subleaf 0 EAX: 00000016 EBX; 756e6547 ECX: 6c65746e EDX; 49656e69
    Leaf 00000001 Subleaf 0 EAX: 00050654 EBX; 00200800 ECX: fedaf387 EDX; bfebfbff
    Leaf 00000004 Subleaf 0 EAX: 3c004121 EBX; 01c0003f ECX: 0000003f EDX; 00000000
    Leaf 00000004 Subleaf 1 EAX: 3c004122 EBX; 01c0003f ECX: 0000003f EDX; 00000000
    Leaf 00000004 Subleaf 2 EAX: 3c004143 EBX; 03c0003f ECX: 000003ff EDX; 00000000
    Leaf 00000004 Subleaf 3 EAX: 3c07c163 EBX; 0280003f ECX: 00006fff EDX; 00000000
    Leaf 00000004 Subleaf 4 EAX: 00000000 EBX; 00000000 ECX: 00000000 EDX; 00000000
    Leaf 0000000b Subleaf 0 EAX: 00000001 EBX; 00000002 ECX: 00000100 EDX; 00000000
    Leaf 0000000b Subleaf 1 EAX: 00000005 EBX; 0000001c ECX: 00000201 EDX; 00000000
    Leaf 0000000b Subleaf 2 EAX: 00000000 EBX; 00000000 ECX: 00000002 EDX; 00000000
```






