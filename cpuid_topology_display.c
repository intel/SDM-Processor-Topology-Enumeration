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
extern GLOBAL_DATA g_GlobalData;


/*
 * Internal Display APIs
 */
void Display_Internal_DisplaySubLeafs(unsigned int Leaf);



/*
 * Display_DisplayParameters
 *
 * Display the commandline parameters
 *
 * Arguments:
 *     None
 *     
 * Return:
 *     None
 */
void Display_DisplayParameters(void)
{
    printf("Processor Topology Example.\n");
    printf("   Command Line Options:\n\n");
    printf("      H                  - Display this message\n");
    printf("      S [File]           - Saves raw CPUID to a file.\n");
    printf("      L [File] [COMMAND] - Loads raw CPUID from a file and perform a numbered COMMAND.\n");
    printf("      C [COMMAND]        - Execute the numbered command from below.\n\n");
    printf("   List of commands\n");
    printf("      0 - Display the topology via OS APIs (Not valid with File Load)\n");
    printf("      1 - Display the topology via CPUID\n");
    printf("      2 - Display CPUID Leaf values one processor\n");
    printf("      3 - Display CPUID Leaf values all processors\n");
    printf("      4 - Display APIC ID layout\n");
    printf("      5 - Display TLB Information\n");
    printf("      6 - Display Cache Information\n");
    printf("\n");
}


/*
 * Display_DisplayProcessorLeafs
 *
 * Display the raw CPUID leafs for the processors.
 *
 * Arguments:
 *     Number of Processors
 *     
 * Return:
 *     None
 */
void Display_DisplayProcessorLeafs(unsigned int NumberOfProcessors)
{
    unsigned int ProcessorIndex;

    printf("Displaying CPUID Leafs 0, 1, 4, 0Bh, 018h, 01Fh if they exist\n");

    for (ProcessorIndex = 0; ProcessorIndex < NumberOfProcessors; ProcessorIndex++) 
    {
         Tools_SetAffinity(ProcessorIndex);
         printf("*******************************\n");
         printf("Processor: %i\n", ProcessorIndex);
         Display_Internal_DisplaySubLeafs(0);
         Display_Internal_DisplaySubLeafs(1);
         Display_Internal_DisplaySubLeafs(4);
         Display_Internal_DisplaySubLeafs(0xB);
         Display_Internal_DisplaySubLeafs(0x18);
         Display_Internal_DisplaySubLeafs(0x1F);
         printf("\n");
    }
}

/*
 * Display_Internal_DisplaySubLeafs
 *
 * This displays the raw CPUID leaf information and subleafs.
 * There is special handling to determine how to enumerate subleafs
 * on each of the leafs.  This is only ment for a few leafs needed
 * for topology as called by Display_DisplayProcessorLeafs()
 *
 * Arguments:
 *     Leaf Number
 *     
 * Return:
 *     None
 */
void Display_Internal_DisplaySubLeafs(unsigned int Leaf)
{
    CPUID_REGISTERS OriginalCpuidRegisters;
    CPUID_REGISTERS CpuidRegisters;
    PCPUID_REGISTERS pCpuidReadValues;
    unsigned int CurrentSubleaf;
    BOOL_TYPE NextSubleaf;

    CurrentSubleaf = 0;
    pCpuidReadValues = &OriginalCpuidRegisters;

    Tools_ReadCpuid(0, 0, pCpuidReadValues);

    if (pCpuidReadValues->x.Register.Eax >= Leaf) 
    {
        do {

            Tools_ReadCpuid(Leaf, CurrentSubleaf, pCpuidReadValues);

            printf("Leaf %08x Subleaf %u EAX: %08x EBX; %08x ECX: %08x EDX; %08x\n", Leaf, CurrentSubleaf, pCpuidReadValues->x.Register.Eax, pCpuidReadValues->x.Register.Ebx, pCpuidReadValues->x.Register.Ecx, pCpuidReadValues->x.Register.Edx);
            CurrentSubleaf++;

            switch (Leaf) 
            {
                case 4:
                   NextSubleaf = ((pCpuidReadValues->x.Register.Eax & 0x1F) != 0) ? BOOL_TRUE : BOOL_FALSE;
                   break;

                case 0x18:
                   NextSubleaf = (CurrentSubleaf <= OriginalCpuidRegisters.x.Register.Eax) ? BOOL_TRUE : BOOL_FALSE;
                   break;

                case 0xB:
                case 0x1F:
                   NextSubleaf = (pCpuidReadValues->x.Register.Ebx != 0) ? BOOL_TRUE : BOOL_FALSE;
                   break;

                default:
                    NextSubleaf = BOOL_FALSE;
            }

            pCpuidReadValues = &CpuidRegisters;

        } while(NextSubleaf);
    }

}


/*
 * Display_ApicIdBitLayout
 *
 * This is a general function that will display the APIC ID bit
 * layout and a message about it from the calling function.  The bit layout
 * is encoded into the structure passed in from legacy topolgy, three level topology
 * or many levels of topology.
 *
 * Arguments:
 *     APIC Bit Layout Context
 *     
 * Return:
 *     None
 */
void Display_ApicIdBitLayout(PAPICID_BIT_LAYOUT_CTX pApicidBitLayoutCtx)
{
    unsigned int LowBit;
    unsigned int HighBit;
    unsigned int DomainIndex;
    unsigned int DomainTextIndex;
    const unsigned int UnknownTextIndex = 7;
    char *pszTopologyNames[] = { "Invalid", "Logical Processor", "Core", "Module", "Tile", "Die", "DieGrp", "Unknown"};

    printf("%s\n", pApicidBitLayoutCtx->szDescription);
    
    LowBit = 0;

    for(DomainIndex = 0; DomainIndex < pApicidBitLayoutCtx->PackageDomainIndex; DomainIndex++) 
    {
        /*
         *  
         * The first index will be logical processor and the second will be Core.  We need to handle the case 
         * where zero is returned.
         * 
         */
        if(pApicidBitLayoutCtx->ShiftValues[DomainIndex] != 0)
        {
            HighBit = pApicidBitLayoutCtx->ShiftValues[DomainIndex] - 1;

            /*
             * We are short cutting this here since domains and indexes are equal, note that this may not be the case always depending on what future domains are being handled.
             */
            DomainTextIndex = (Tools_IsDomainKnownEnumeration(pApicidBitLayoutCtx->ShiftValueDomain[DomainIndex]) == BOOL_TRUE) ? pApicidBitLayoutCtx->ShiftValueDomain[DomainIndex] : UnknownTextIndex;

            printf("%20s[%i:%i] (Domain Type Value: %i)\n", pszTopologyNames[DomainTextIndex], HighBit, LowBit, pApicidBitLayoutCtx->ShiftValueDomain[DomainIndex]);
            LowBit = pApicidBitLayoutCtx->ShiftValues[DomainIndex];
        }
    }

    printf("%20s[%i:%i]\n\n", "Package", pApicidBitLayoutCtx->NumberOfApicIdBits-1, LowBit);
}



/*
 * Display_ThreeDomainDisplay
 *
 *    This will specifically display a three domain
 *    topology using a simple output format.  This is for
 *    easy display.
 *
 * Arguments:
 *     Leaf, CoreShift, LogicalProcessorShift
 *     
 * Return:
 *     None
 */
void Display_ThreeDomainDisplay(unsigned int Leaf, unsigned int PackageShift, unsigned int LogicalProcessorShift)
{
    unsigned int ProcessorIndex;
    unsigned int PackageMask;
    unsigned int LogicalProcessorPackageMask;
    unsigned int CorePackageMask;
    unsigned int LogicalProcessorMask;
    unsigned int ApicIdArray[MAX_PROCESSORS];
    unsigned int NumberOfLogicalProcessors;

    printf("\n**************************\n");

    if (Leaf == 1) 
    {
        printf("Three Level Topology using CPUID.1/CPUID.4.\n");
        printf("On modern platforms this may not be accurate since these are only 8 bit APIC IDs and they are subject to overflow.\n");
    }
    else
    {
        printf("Topology from CPUID Leaf %Xh\n\n", Leaf);
    }

    LogicalProcessorPackageMask = (1<<LogicalProcessorShift) - 1;
    CorePackageMask             = ((1<<PackageShift) - 1) ^ ((1<<LogicalProcessorShift)-1);
    PackageMask                 = ~((1<<PackageShift) - 1);
    LogicalProcessorMask        = (1<<LogicalProcessorShift)-1;

    printf("**Package Mask: 0x%08x\n", PackageMask);
    printf("**Core Mask:    0x%08x\n", CorePackageMask);
    printf("**Package Logical Processor Mask: 0x%08x\n\n", LogicalProcessorPackageMask);

    /*
     * Optionally could dynamically allocate this based on the number of logical processors.
     */
    NumberOfLogicalProcessors = Tools_GatherPlatformApicIds(ApicIdArray, MAX_PROCESSORS);
    
    for (ProcessorIndex = 0; ProcessorIndex < NumberOfLogicalProcessors; ProcessorIndex++) 
    {
        printf(" - Processor %i APIC ID(0x%x)  PKG_ID(%i)  CORE_ID(%i)  LP_ID(%i)\n", ProcessorIndex, ApicIdArray[ProcessorIndex],
                                                                                    (ApicIdArray[ProcessorIndex] & PackageMask)>>PackageShift,
                                                                                    (ApicIdArray[ProcessorIndex] & CorePackageMask)>>LogicalProcessorShift,
                                                                                    ApicIdArray[ProcessorIndex] & LogicalProcessorMask);
    }

}



/*
 * Display_ManyDomainExample
 *
 *    This API takes in the APIC ID bit layout context and will
 *    display any level of domain layout.
 *
 * Arguments:
 *     Leaf, APIC Bit Layout Context
 *     
 * Return:
 *     None
 */
void Display_ManyDomainExample(unsigned int Leaf, PAPICID_BIT_LAYOUT_CTX pApicidBitLayoutCtx)
{
    char *pszTopologyNames[] = { "Invalid", "Logical Processor", "Core", "Module", "Tile", "Die", "DieGrp" };  /* Assume the input was filtered only to known domains. */
    unsigned int DomainIndex;
    unsigned int TopDomainIndex;
    unsigned int ProcessorIndex;
    unsigned int DomainShift;
    unsigned int ApicIdArray[MAX_PROCESSORS];
    unsigned int NumberOfLogicalProcessors;

    printf("***********************************\n");
    printf("CPUID Leaf %i - Parse all known domains\n\n", Leaf);

    for (DomainIndex = 0; DomainIndex < pApicidBitLayoutCtx->PackageDomainIndex; DomainIndex++)
    {
        if (pApicidBitLayoutCtx->ShiftValues[DomainIndex] != 0) 
        {
            printf("  %20s Mask:  0x%08x\n", pszTopologyNames[pApicidBitLayoutCtx->ShiftValueDomain[DomainIndex]], pApicidBitLayoutCtx->DomainRelativeMasks[DomainIndex][DomainIndex]);
        }
    }

    printf("  %20s Mask:  0x%08x\n\n", "Package", pApicidBitLayoutCtx->DomainRelativeMasks[DomainIndex][DomainIndex]);

    for (DomainIndex = 0; DomainIndex < pApicidBitLayoutCtx->PackageDomainIndex; DomainIndex++) 
    {
        if (pApicidBitLayoutCtx->ShiftValues[DomainIndex] != 0) 
        {
            for (TopDomainIndex = DomainIndex+1; TopDomainIndex < pApicidBitLayoutCtx->PackageDomainIndex; TopDomainIndex++) 
            {
                printf("  %s Domain ID Mask Relative to %s Domain  0x%08x\n", pszTopologyNames[pApicidBitLayoutCtx->ShiftValueDomain[DomainIndex]], pszTopologyNames[pApicidBitLayoutCtx->ShiftValueDomain[TopDomainIndex]], pApicidBitLayoutCtx->DomainRelativeMasks[DomainIndex][TopDomainIndex]);
            }

            printf("  %s Domain ID Mask Relative to Package  0x%08x\n\n", pszTopologyNames[pApicidBitLayoutCtx->ShiftValueDomain[DomainIndex]], pApicidBitLayoutCtx->DomainRelativeMasks[DomainIndex][TopDomainIndex]);
        }
    }

    printf("\n Enumerating Processors\n");

    /*
     * Optionally could dynamically allocate this based on the number of logical processors.
     */
    NumberOfLogicalProcessors = Tools_GatherPlatformApicIds(ApicIdArray, MAX_PROCESSORS);

    for (ProcessorIndex = 0; ProcessorIndex < NumberOfLogicalProcessors; ProcessorIndex++) 
    {
        printf("\n - Processor %i APIC ID(0x%x)\n", ProcessorIndex, ApicIdArray[ProcessorIndex]);
        printf("   + Package ID:  0x%08x\n", (pApicidBitLayoutCtx->DomainRelativeMasks[pApicidBitLayoutCtx->PackageDomainIndex][pApicidBitLayoutCtx->PackageDomainIndex] & ApicIdArray[ProcessorIndex])>>pApicidBitLayoutCtx->ShiftValues[pApicidBitLayoutCtx->PackageDomainIndex-1]);

        for (DomainIndex = 0, DomainShift = 0; DomainIndex < pApicidBitLayoutCtx->PackageDomainIndex; DomainIndex++) 
        {
            if (pApicidBitLayoutCtx->ShiftValues[DomainIndex] != 0) 
            {
                for (TopDomainIndex = DomainIndex+1; TopDomainIndex < pApicidBitLayoutCtx->PackageDomainIndex; TopDomainIndex++) 
                {
                    printf("   + %s Relative to %s ID:  0x%08x\n", pszTopologyNames[pApicidBitLayoutCtx->ShiftValueDomain[DomainIndex]], pszTopologyNames[pApicidBitLayoutCtx->ShiftValueDomain[TopDomainIndex]], (pApicidBitLayoutCtx->DomainRelativeMasks[DomainIndex][TopDomainIndex] & ApicIdArray[ProcessorIndex])>>DomainShift);
                }

                printf("   + %s Relative to Package ID:  0x%08x\n", pszTopologyNames[pApicidBitLayoutCtx->ShiftValueDomain[DomainIndex]], (pApicidBitLayoutCtx->DomainRelativeMasks[DomainIndex][TopDomainIndex] & ApicIdArray[ProcessorIndex])>>DomainShift);
            }
            DomainShift = pApicidBitLayoutCtx->ShiftValues[DomainIndex];
        }

    }


    printf("***********************************\n");
}





/*
 * Display_DisplayProcessorCaches
 *
 *    Displays the enumerated Caches on the platform, the processors
 *    associated with them and their details.
 *
 * Arguments:
 *     List of Caches, number of Caches
 *     
 * Return:
 *     None
 */
void Display_DisplayProcessorCaches(PCPUID_CACHE_INFO pCacheInfo, unsigned int NumberOfCaches)
{
    unsigned int CacheIndex;
    unsigned int CacheProcessorIndex;
    char *pszCacheType[] = { "Data Cache", "Instruction Cache", "Unified Cache" };

    for (CacheIndex = 0; CacheIndex < NumberOfCaches; CacheIndex++) 
    {
        printf("\n*************************************\n");
        printf("   Cache Level: %i\n", pCacheInfo[CacheIndex].CacheLevel);
        printf("    Cache Type: %i ", pCacheInfo[CacheIndex].CacheType);

        switch (pCacheInfo[CacheIndex].CacheType) 
        {
            case CacheType_DataCache:      
            case CacheType_InstructionCache:
            case CacheType_UnifiedCache:   
                 printf("(%s)", pszCacheType[pCacheInfo[CacheIndex].CacheType - 1]);
        }

        printf("\n       CacheId: %i\n", pCacheInfo[CacheIndex].CacheId);
        printf("    Cache Mask: 0x%08x\n\n", pCacheInfo[CacheIndex].CacheMask);
        
        printf(" Processors sharing this cache: %i", pCacheInfo[CacheIndex].NumberOfLPsSharingThisCache);

        for (CacheProcessorIndex = 0; CacheProcessorIndex < pCacheInfo[CacheIndex].NumberOfLPsSharingThisCache; CacheProcessorIndex++) 
        {
            if ((CacheProcessorIndex % 6) == 0) 
            {
                printf("\n     ");
            }
            else
            {
                printf(", ");
            }

            printf("0x%03x", pCacheInfo[CacheIndex].ListOfApicIDsSharingThisCache[CacheProcessorIndex]);
        }

        printf("\n\n");

        printf(" Number of Ways: %i\n Partitions: %i\n Cache Line Size: %i Bytes\n Number of Sets: %i\n Cache Size: %i Bytes, %1.2f Kb, %1.2f MB\n", pCacheInfo[CacheIndex].CacheWays, pCacheInfo[CacheIndex].CachePartitions, 
                                                                                                                                                      pCacheInfo[CacheIndex].CacheLineSize, pCacheInfo[CacheIndex].CacheSets, pCacheInfo[CacheIndex].CacheSizeInBytes,
                                                                                                                                                      (float)pCacheInfo[CacheIndex].CacheSizeInBytes/1024.0, ((float)pCacheInfo[CacheIndex].CacheSizeInBytes/1024.0)/1024.0);

        printf("\n Cache Level is Self Initializing: %s\n", (pCacheInfo[CacheIndex].SelfInitializing == BOOL_FALSE) ? "FALSE" : "TRUE");
        printf(" Cache is Fully Associative:       %s\n", (pCacheInfo[CacheIndex].CacheIsFullyAssociative == BOOL_FALSE) ? "FALSE" : "TRUE");
        printf(" Cache is Inclusive:               %s\n", (pCacheInfo[CacheIndex].CacheIsInclusive == BOOL_FALSE) ? "FALSE" : "TRUE");
        printf(" Cache is Direct Mapped:           %s\n", (pCacheInfo[CacheIndex].CacheIsDirectMapped == BOOL_FALSE) ? "FALSE" : "TRUE");
        printf(" Cache is Complex:                 %s\n\n", (pCacheInfo[CacheIndex].CacheIsComplex == BOOL_FALSE) ? "FALSE" : "TRUE");
                                                                                         
        printf(" WBINVD will flush lower levels sharing this cache:       %s\n", (pCacheInfo[CacheIndex].WbinvdFlushsLowerLevelsSharing == BOOL_FALSE) ? "FALSE" : "TRUE");

        printf("\n");
    }
}


/*
 * Display_DisplayProcessorTlbs
 *
 *    Displays the enumerated TLBs on the platform, the processors
 *    associated with them and their details.
 *    
 *
 * Arguments:
 *     List of TLBs, Number of TLBs
 *     
 * Return:
 *     None
 */
void Display_DisplayProcessorTlbs(PCPUID_TLB_INFO pTlbInfo, unsigned int NumberOfTlbs)
{
    unsigned int TlbIndex;
    unsigned int TlbProcessorIndex;
    char *pszTlbType[] = { "Data TLB", "Instruction TLB", "Unified TLB", "Load-Only TLB", "Store-Only TLB" };

    for (TlbIndex = 0; TlbIndex < NumberOfTlbs; TlbIndex++) 
    {
        printf("\n*************************************\n");
        printf("   TLB Level: %i\n", pTlbInfo[TlbIndex].TlbLevel);
        printf("    TLB Type: %i ", pTlbInfo[TlbIndex].TlbType);

        switch (pTlbInfo[TlbIndex].TlbType) 
        {
            case TlbType_Data:       
            case TlbType_Instruction:
            case TlbType_Unified:    
            case TlbType_LoadOnly:   
            case TlbType_StoreOnly:   
                 printf("(%s)", pszTlbType[pTlbInfo[TlbIndex].TlbType - 1]);
        }

        printf("\n       TlbId: %i\n", pTlbInfo[TlbIndex].TlbId);
        printf("    Tlb Mask: 0x%08x\n\n", pTlbInfo[TlbIndex].TlbMask);
        
        printf(" Processors sharing this TLB: %i", pTlbInfo[TlbIndex].NumberOfLPsSharingThisTlb);

        for (TlbProcessorIndex = 0; TlbProcessorIndex < pTlbInfo[TlbIndex].NumberOfLPsSharingThisTlb; TlbProcessorIndex++) 
        {
            if ((TlbProcessorIndex % 6) == 0) 
            {
                printf("\n     ");
            }
            else
            {
                printf(", ");
            }

            printf("0x%03x", pTlbInfo[TlbIndex].ListOfApicIDsSharingThisTlb[TlbProcessorIndex]);
        }

        printf("\n\n");

        printf(" Number of Ways: %i\n TLB Paritioning: %i\n Number of Sets: %i\n", pTlbInfo[TlbIndex].TlbWays, pTlbInfo[TlbIndex].TlbParitioning, pTlbInfo[TlbIndex].TlbSets);


        printf("\n  4K Page Size Entries Supported:       %s\n", (pTlbInfo[TlbIndex]._4K_PageSizeEntries  == BOOL_FALSE) ? "FALSE" : "TRUE");
        printf(" 2MB Page Size Entries Supported:       %s\n", (pTlbInfo[TlbIndex]._2MB_PageSizeEntries == BOOL_FALSE) ? "FALSE" : "TRUE");
        printf(" 4MB Page Size Entries Supported:       %s\n", (pTlbInfo[TlbIndex]._4MB_PageSizeEntries == BOOL_FALSE) ? "FALSE" : "TRUE");
        printf(" 1GB Page Size Entries Supported:       %s\n", (pTlbInfo[TlbIndex]._1GB_PageSizeEntries == BOOL_FALSE) ? "FALSE" : "TRUE");


        printf("\n TLB is Fully Associative:              %s\n", (pTlbInfo[TlbIndex].FullyAssociative == BOOL_FALSE) ? "FALSE" : "TRUE");

        printf("\n");
    }
}



