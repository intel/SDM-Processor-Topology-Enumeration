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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpuid_topology.h"


/*
 * Global application data variable
 */
GLOBAL_DATA g_GlobalData;


/*
 * Tools_ReadCpuid
 *
 *    CPUID API used to be able to read native CPUID or be thunked and
 *    simulate or display other CPUID values from other platforms
 *    or testing out topology values.
 *
 * Arguments:
 *     Leaf, Subleaf, CPUID Data Structure
 *     
 * Return:
 *     None
 */
void Tools_ReadCpuid(unsigned int Leaf, unsigned int Subleaf, PCPUID_REGISTERS pCpuidRegisters)
{
    if (g_GlobalData.UseNativeCpuid) 
    {
        Os_Platform_Read_Cpuid(Leaf, Subleaf, pCpuidRegisters);
    }
    else
    {
        memset(pCpuidRegisters, 0, sizeof(CPUID_REGISTERS));

        if (Leaf < MAX_SIMULATED_LEAFS) 
        {
            if (Subleaf < MAX_SIMULATED_SUBLEAFS) 
            {
                /*
                 *  The file stores a single copy of each CPUID except for CPUID.4 and CPUID.18.  Although the others
                 *  have some asymmetric aspects in CPUID.1F and CPUID.B; they are not important to this sample code. However, to
                 *  ensure we reserve asymmetric topology enumeration we save all of CPUID.4 and CPUID.18 values and so we
                 *  have to dispatch those seperately and other leafs we rebuild just the APIC IDs (we do not rebuild EBX in extended
                 *  topology leaf, which can also be asymmetric but it's only for reporting purposes and not used in this sample).
                 */
                if (Leaf == 0x4) 
                {
                    pCpuidRegisters->x.Registers[0] = g_GlobalData.SimulatedCpuidLeaf4[g_GlobalData.CurrentProcessorAffinity][Subleaf][0];
                    pCpuidRegisters->x.Registers[1] = g_GlobalData.SimulatedCpuidLeaf4[g_GlobalData.CurrentProcessorAffinity][Subleaf][1];
                    pCpuidRegisters->x.Registers[2] = g_GlobalData.SimulatedCpuidLeaf4[g_GlobalData.CurrentProcessorAffinity][Subleaf][2];
                    pCpuidRegisters->x.Registers[3] = g_GlobalData.SimulatedCpuidLeaf4[g_GlobalData.CurrentProcessorAffinity][Subleaf][3];
                }
                else
                {
                    if (Leaf == 0x18) 
                    {
                        pCpuidRegisters->x.Registers[0] = g_GlobalData.SimulatedCpuidLeaf18[g_GlobalData.CurrentProcessorAffinity][Subleaf][0];
                        pCpuidRegisters->x.Registers[1] = g_GlobalData.SimulatedCpuidLeaf18[g_GlobalData.CurrentProcessorAffinity][Subleaf][1];
                        pCpuidRegisters->x.Registers[2] = g_GlobalData.SimulatedCpuidLeaf18[g_GlobalData.CurrentProcessorAffinity][Subleaf][2];
                        pCpuidRegisters->x.Registers[3] = g_GlobalData.SimulatedCpuidLeaf18[g_GlobalData.CurrentProcessorAffinity][Subleaf][3];
                    }
                    else
                    {
                        pCpuidRegisters->x.Registers[0] = g_GlobalData.SimulatedCpuid[Leaf][Subleaf][0];
                        pCpuidRegisters->x.Registers[1] = g_GlobalData.SimulatedCpuid[Leaf][Subleaf][1];
                        pCpuidRegisters->x.Registers[2] = g_GlobalData.SimulatedCpuid[Leaf][Subleaf][2];
                        pCpuidRegisters->x.Registers[3] = g_GlobalData.SimulatedCpuid[Leaf][Subleaf][3];

                        if ((Leaf == 0xB || Leaf == 0x1F) && pCpuidRegisters->x.Registers[1] != 0)
                        {
                            pCpuidRegisters->x.Registers[3] = g_GlobalData.SimulatedApicIds[g_GlobalData.CurrentProcessorAffinity];
                        }

                        if (Leaf == 0x1)
                        {
                            pCpuidRegisters->x.Registers[2] = pCpuidRegisters->x.Registers[2] & (~(0xFF<<24));
                            pCpuidRegisters->x.Registers[2] = pCpuidRegisters->x.Registers[2] | (g_GlobalData.SimulatedApicIds[g_GlobalData.CurrentProcessorAffinity]<<24);
                        }
                    }
                }
            }
        }
     }
}






/*
 * Tools_CreateTopologyShift
 *
 *    Creates a power of 2 mask inclusive of the count.
 *
 * Arguments:
 *     None
 *     
 * Return:
 *     None
 */
unsigned int Tools_CreateTopologyShift(unsigned int count)
{
    unsigned int Index;
    unsigned int Shift;

    Shift = 31;

    count = (count*2) - 1;

    for (Index = (1<<Shift); Index; Index >>=1, Shift--) 
    {
        if (count & Index) 
        {
            break;
        }
    }

    return Shift;
}




/*
 * Tools_IsNative
 *
 * Determines if CPUID is in NATIVE or Virtual Mode
 *
 * Arguments:
 *     None
 *     
 * Return:
 *     TRUE for Native and FALSE for Virtual
 */
BOOL_TYPE Tools_IsNative(void)
{
    return g_GlobalData.UseNativeCpuid;
}


/*
 * Tools_SetAffinity
 *
 *    Sets affinity to the specified processor.
 * 
 *    Ignored if processor does not exist.
 *
 * Arguments:
 *     Processor to set affinity
 *     
 * Return:
 *     None
 */
void Tools_SetAffinity(unsigned int ProcessorNumber)
{
    if (g_GlobalData.UseNativeCpuid)
    {
        Os_SetAffinity(ProcessorNumber);
    }
    else
    {
        if (ProcessorNumber < g_GlobalData.NumberOfSimulatedProcessors) 
        {
            g_GlobalData.CurrentProcessorAffinity = ProcessorNumber;
        }
    }
}




/*
 * Tools_GetNumberOfProcessors
 *
 *    Returns the number of processors on this system
 *
 * Arguments:
 *     None
 *     
 * Return:
 *     The number of processors
 */
unsigned int Tools_GetNumberOfProcessors(void)
{
    unsigned int NumberOfProcessors;

    if (g_GlobalData.UseNativeCpuid)
    {
        NumberOfProcessors = Os_GetNumberOfProcessors();
    }
    else
    {
        NumberOfProcessors = g_GlobalData.NumberOfSimulatedProcessors;
    }

    return NumberOfProcessors;
}


/*
 * Tools_IsDomainKnownEnumeration
 *
 *    Determines if the enumeration value is known.
 *
 * Arguments:
 *     Domain
 *     
 * Return:
 *     Returns BOOL_TRUE if the enumeration is a known value.
 */
BOOL_TYPE Tools_IsDomainKnownEnumeration(unsigned int Domain)
{
    BOOL_TYPE KnownEnumeration;

    KnownEnumeration = BOOL_FALSE;

    switch (Domain) 
    {
        case InvalidDomain:  /* This is a valid known enumeration; code should check directly for invalid.
                              *  This API doesn't mean that unknown domains are invalid so doesn't make sense
                              * to call it not a known enumeration.
                              */
        case LogicalProcessorDomain:
        case CoreDomain:
        case ModuleDomain:
        case TileDomain:
        case DieDomain:
        case DieGrpDomain:
            KnownEnumeration = BOOL_TRUE;
            break;
    }

    return KnownEnumeration;
}


/*
 * Tools_GatherPlatformApicIds
 *
 *    Creates and returns a cache of the APIC IDs.
 *    
 *
 * Arguments:
 *     APIC ID array, Size of the array
 *     
 * Return:
 *     Number of APIC IDs populated
 */
unsigned int Tools_GatherPlatformApicIds(unsigned int *pApicIdArray, unsigned int ArraySize)
{
    unsigned int NumberOfProcessors;
    unsigned int Index;
    unsigned int ApicId;
    BOOL_TYPE bApicIdFound;
    CPUID_REGISTERS CpuidRegisters;
    CPUID_REGISTERS CpuidRegistersApicid;

    NumberOfProcessors = Tools_GetNumberOfProcessors();

    if (NumberOfProcessors > MAX_PROCESSORS) 
    {
        NumberOfProcessors = MAX_PROCESSORS;
    }

    /*
     * Determine X2APIC ID or fall back to APIC ID.
     */
    Tools_ReadCpuid(0, 0, &CpuidRegisters);

    for (Index = 0; Index < NumberOfProcessors && Index < ArraySize; Index++) 
    {
        ApicId = 0;
        bApicIdFound = BOOL_FALSE;

        Tools_SetAffinity(Index);
        if (CpuidRegisters.x.Register.Eax >= 0x1F) 
        {
            Tools_ReadCpuid(0x1F, 0, &CpuidRegistersApicid);

            if (CpuidRegistersApicid.x.Register.Ebx != 0) 
            {
                bApicIdFound = BOOL_TRUE;
                ApicId = CpuidRegistersApicid.x.Register.Edx;
            }

        }

        if (bApicIdFound == BOOL_FALSE) 
        {
            if (CpuidRegisters.x.Register.Eax >= 0xB)
            {
                Tools_ReadCpuid(0xB, 0, &CpuidRegistersApicid);
                if (CpuidRegistersApicid.x.Register.Ebx != 0) 
                {
                    ApicId = CpuidRegistersApicid.x.Register.Edx;
                    bApicIdFound = BOOL_TRUE;
                }
            }

            if (bApicIdFound == BOOL_FALSE) 
            {
                /*
                 *  Fall back to Legacy 8 bit APIC ID.
                 */
                Tools_ReadCpuid(1, 0, &CpuidRegistersApicid);
                ApicId = (CpuidRegistersApicid.x.Register.Ebx >> 24);
                bApicIdFound = BOOL_TRUE;
            }
        }

        pApicIdArray[Index] = ApicId;
    }

    return Index;
}

