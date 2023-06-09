/*
 * Copyright (C) 2023 by Intel Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#define _GNU_SOURCE
#include <sched.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "cpuid_topology.h"


/*
 * Constants, values for local use
 */
#define BYTES_IN_KB  (1024)
#define BYTES_IN_MB  (1048576)




/*
 * Os_DisplayTopology
 *
 * Display the topology using OS related information
 *
 * Arguments:
 *     None
 *     
 * Return:
 *     None
 */
void Os_DisplayTopology(void)
{
    /*
     * One example, applications may parse the output.
    */
    printf("*********************************\n");
    printf("****  Linux OS /proc/cpuinfo ****\n\n");

    system("cat /proc/cpuinfo\n");

    printf("\n\n");
}




/*
 * Os_Platform_Read_Cpuid
 *
 *    OS/Compiler Specific implementation of reading CPUID.
 *
 * Arguments:
 *     Leaf, Subleaf, CPUID Data Structure
 *     
 * Return:
 *     None
 */
void Os_Platform_Read_Cpuid(unsigned int Leaf, unsigned int Subleaf, PCPUID_REGISTERS pCpuidRegisters)
{
    unsigned int ReturnEax;
    unsigned int ReturnEbx;
    unsigned int ReturnEcx;
    unsigned int ReturnEdx;
    
    asm ("movl %4, %%eax\n"
         "movl %5, %%ecx\n"
         "CPUID\n"
         "movl %%eax, %0\n"
         "movl %%ebx, %1\n"
         "movl %%ecx, %2\n"
         "movl %%edx, %3\n"
          : "=r" (ReturnEax), "=r" (ReturnEbx), "=r" (ReturnEcx), "=r" (ReturnEdx)
          : "r" (Leaf), "r" (Subleaf)
          : "%eax", "%ebx", "%ecx", "%edx"
    );

     pCpuidRegisters->x.Register.Eax  = ReturnEax;
     pCpuidRegisters->x.Register.Ebx  = ReturnEbx;
     pCpuidRegisters->x.Register.Ecx  = ReturnEcx;
     pCpuidRegisters->x.Register.Edx  = ReturnEdx;
}

/*
 * Os_GetNumberOfProcessors
 *
 *    Get the number of processors
 *
 * Arguments:
 *     None
 *     
 * Return:
 *     Number of processors
 */
unsigned int Os_GetNumberOfProcessors(void)
{
    unsigned int NumberOfProcessors;

    NumberOfProcessors = (unsigned int)get_nprocs();

    return NumberOfProcessors;
}


/*
 * Os_SetAffinity
 *
 *    Set the current thread affinity
 *
 * Arguments:
 *     Processor Number
 *     
 * Return:
 *     None
 */
void Os_SetAffinity(unsigned int ProcessorNumber)
{  
    cpu_set_t *cpu_set;
    unsigned int NumberOfProcessors;
    unsigned int SetSize;

    /*
     * Get the size of the maximum number of configured processors.
    */
    NumberOfProcessors = get_nprocs_conf();

    /*
     * Something larger comes in we will just go with it.
     */
    if (ProcessorNumber > NumberOfProcessors) 
    {
        NumberOfProcessors = ProcessorNumber;
    }

    cpu_set = CPU_ALLOC(NumberOfProcessors);

    if (cpu_set) 
    {
        SetSize = CPU_ALLOC_SIZE(NumberOfProcessors);
        CPU_ZERO_S(SetSize, cpu_set);
        CPU_SET_S(ProcessorNumber, SetSize, cpu_set);

        sched_setaffinity(getpid(),SetSize, cpu_set);

        CPU_FREE(cpu_set);
    }
}


