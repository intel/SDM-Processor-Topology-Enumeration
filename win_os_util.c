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


#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "cpuid_topology.h"

/*
 * Constants, values for local use
 */
#define BYTES_IN_KB  (1024)
#define BYTES_IN_MB  (1048576)

void __cpuidex(unsigned int *cpuinfo,int function_id,int subfunction_id);
#pragma intrinsic(__cpuidex)


#define CACHE_INVALID_INDEX 4

unsigned char *g_pszCacheTypeString[] = {
    "Unified",
    "Instruction",
    "Data",
    "Trace",
    "Invalid or Unknown Enumeration"
};

/* 
 * Prototypes
 */
void WinOs_EnumerateAndDisplayTopology(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX pSystemLogicalProcInfoEx, DWORD BufferSize);
void WinOs_DisplayGroupAffinity(WORD GroupCount, GROUP_AFFINITY *pGroupAffinityArray);
unsigned char *WinOs_GetCacheTypeString(PROCESSOR_CACHE_TYPE  CacheType);

/*
 * Os_DisplayTopology
 *
 * Display the OS using Windows APIs.
 *
 * Arguments:
 *     None
 *     
 * Return:
 *     None
 */
void Os_DisplayTopology(void)
{
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX pSystemLogicalProcInfoEx;
    DWORD BufferSize = 0;

    GetLogicalProcessorInformationEx(RelationAll, NULL, &BufferSize);

    pSystemLogicalProcInfoEx = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)malloc(BufferSize);
    
    if(pSystemLogicalProcInfoEx)
    {
        printf("********************************************************\n");
        printf("****  Windows OS GetLogicalProcessorInformationEx() ****\n\n");

        memset(pSystemLogicalProcInfoEx, 0, BufferSize);

        if (GetLogicalProcessorInformationEx(RelationAll, pSystemLogicalProcInfoEx, &BufferSize) != FALSE)
        {
            WinOs_EnumerateAndDisplayTopology(pSystemLogicalProcInfoEx, BufferSize);
        }
        else
        {
            printf("Error; failed to display GetLogicalProcessorInformationEx() GLE: %i\n\n", GetLastError());
        }

        free(pSystemLogicalProcInfoEx);

        printf("\n\n");
    }
    else
    {
        printf("Error; failed to display GetLogicalProcessorInformationEx() GLE: %i\n\n", GetLastError());
    }
}



/*
 * WinOs_EnumerateAndDisplayTopology
 *
 *    Itterates through the OS supplied topology information.
 *
 * Arguments:
 *     System Logical Processor Buffer, Size of the Buffer
 *     
 * Return:
 *     None
 */
void WinOs_EnumerateAndDisplayTopology(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX pSystemLogicalProcInfoEx, DWORD BufferSize)
{
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX ptr;
    DWORD Index;
    PCACHE_RELATIONSHIP Cache;
    PGROUP_AFFINITY pGroupAffinity;
    PPROCESSOR_GROUP_INFO pProcessorGroupInfo;
    DWORD CurrentSize;
    DWORD NumberOfPackages = 0;
    WORD GroupCount;

    ptr = pSystemLogicalProcInfoEx;
    CurrentSize = 0;

    /*
     * Example implementation that displays the data, however for applications it could cache the data and use it for any purpose
     */

    while (CurrentSize < BufferSize) 
    {
        switch (ptr->Relationship) 
        {
            case RelationProcessorCore:
                 printf(" - Processor Core\n");
                 WinOs_DisplayGroupAffinity(ptr->Processor.GroupCount, &ptr->Processor.GroupMask[0]);
                 printf("+++++\n");
                 break;

            case RelationCache:
                 printf(" - Cache Type: %s\n", WinOs_GetCacheTypeString(ptr->Cache.Type));
                 printf("    CacheLevel: L%i\n", ptr->Cache.Level);
                 printf("    CacheSize: %i Bytes (%i Kilobytes) (%i Megabytes)\n", ptr->Cache.CacheSize, ptr->Cache.CacheSize / BYTES_IN_KB, ptr->Cache.CacheSize / BYTES_IN_MB);
                 printf("    LineSize: %i\n", ptr->Cache.LineSize);
                 if (ptr->Cache.Associativity == CACHE_FULLY_ASSOCIATIVE) 
                 {
                     printf("    Fully Associative\n\n");
                 }
                 else
                 {
                     printf("    Associativity: %i\n\n", ptr->Cache.Associativity);
                 }
                 printf("    Cache Processor Masks\n");

                 /*
                  * Newer versions of Windows have added "GroupCount" here where it was 0 and reserved on older Versions, 
                  * Convert it to 1 if it is 0. 
                  */
                 GroupCount = *((WORD *)(&ptr->Cache.Reserved[18]));
                 if (GroupCount == 0) 
                 {
                     GroupCount = 1;
                 }
                 WinOs_DisplayGroupAffinity(GroupCount, &ptr->Cache.GroupMask);
                 printf("+++++\n");
                 break;

            case RelationProcessorPackage:
                 printf(" - Package: %i\n", NumberOfPackages);
                 WinOs_DisplayGroupAffinity(ptr->Processor.GroupCount, &ptr->Processor.GroupMask[0]);
                 NumberOfPackages++;
                 printf("+++++\n");
                 break;
            default:
                 /* 
                  * Silently ignore new enumerations that are unknown.
                  */
                 break;
        }

        ptr = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)(((char *)ptr) + ptr->Size);
        CurrentSize = (DWORD)(((ULONG64)ptr) - ((ULONG64)pSystemLogicalProcInfoEx));
    }
}



/*
 * WinOs_DisplayGroupAffinity
 *
 *    Displays the Group Affinity.
 *
 * Arguments:
 *     Number Of Groups, Group Affinity Array
 *     
 * Return:
 *     None
 */
void WinOs_DisplayGroupAffinity(WORD GroupCount, GROUP_AFFINITY *pGroupAffinityArray)
{
    WORD Index;
                 
    for(Index = 0; Index < GroupCount; Index++) 
    {
       printf("     Group: %i, Affinity: 0x%016I64x\n", pGroupAffinityArray[Index].Group, pGroupAffinityArray[Index].Mask);
    }
}


/*
 * WinOs_GetCacheTypeString
 *
 *    Returns a string describing the cache type.
 *
 * Arguments:
 *     Cache Type
 *     
 * Return:
 *     Pointer from global string array of cache type
 */
unsigned char *WinOs_GetCacheTypeString(PROCESSOR_CACHE_TYPE  CacheType)
{
    if ((CacheType >= CacheUnified && CacheType <= CacheTrace) == 0) 
    {
        CacheType = CACHE_INVALID_INDEX;
    }

    return g_pszCacheTypeString[CacheType];
}


/*
 * Os_Platform_Read_Cpuid
 *
 *    OS/Compiler Specific intrinsic of reading CPUID.
 *
 * Arguments:
 *     Leaf, Subleaf, CPUID Data Structure
 *     
 * Return:
 *     None
 */
void Os_Platform_Read_Cpuid(unsigned int Leaf, unsigned int Subleaf, PCPUID_REGISTERS pCpuidRegisters)
{
    __cpuidex(&pCpuidRegisters->x.Registers[0], Leaf, Subleaf);
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

    NumberOfProcessors = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);

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
    GROUP_AFFINITY GroupAffinity;
    unsigned short GroupIndex;
    unsigned short MaxGroups;
    unsigned int NumberOfGroupProcessors;

    /*
     * Assume the active groups are going to be contiguous.
     */
    MaxGroups = GetActiveProcessorGroupCount();

    for (GroupIndex = 0; GroupIndex < MaxGroups; GroupIndex++) 
    {
        NumberOfGroupProcessors = GetActiveProcessorCount(GroupIndex);

        if (ProcessorNumber < NumberOfGroupProcessors) 
        {
            memset(&GroupAffinity, 0, sizeof(GroupAffinity));
            GroupAffinity.Group = GroupIndex;
            GroupAffinity.Mask = (KAFFINITY)((ULONG64)1<<(ULONG64)ProcessorNumber);
            SetThreadGroupAffinity(GetCurrentThread(), &GroupAffinity, NULL);
            break;
        }
        else
        {
            ProcessorNumber = ProcessorNumber - NumberOfGroupProcessors;
        }
    }
}


