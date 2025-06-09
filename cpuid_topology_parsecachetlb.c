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
 *  Internal Prototype APIs
 */
unsigned int ParseCache_Internal_FindMatchingCacheEntry(PCPUID_CACHE_INFO pCacheInfo, unsigned int NumberOfCaches, unsigned int CacheId, PCPUID_REGISTERS pCpuidRegisters);
unsigned int ParseCache_Internal_AddCacheEntry(PCPUID_CACHE_INFO pCacheInfo, unsigned int *pNumberOfCaches, unsigned int CacheId, unsigned int CacheMask, PCPUID_REGISTERS pCpuidRegisters);
unsigned int ParseTlb_Internal_FindMatchingTlbEntry(PCPUID_TLB_INFO pTlbInfo, unsigned int NumberOfTlbs, unsigned int TlbId, PCPUID_REGISTERS pCpuidRegisters);
unsigned int ParseTlb_Internal_AddTlbEntry(PCPUID_TLB_INFO pTlbInfo, unsigned int *pNumberOfTlbs, unsigned int TlbId, unsigned int TlbMask, PCPUID_REGISTERS pCpuidRegisters);        

/*
 * ParseCache_CpuidCacheExample
 *
 *    This is an example of how to parse the CPUID Caching Information
 *    via CPUID Leaf 4.
 * 
 * Arguments:
 *     None
 *     
 * Return:
 *     None
 */
void ParseCache_CpuidCacheExample(void)
{
    PCPUID_CACHE_INFO pCacheInfo;
    unsigned int CacheIndex;
    unsigned int NumberOfCaches;
    unsigned int SubleafIndex;
    CPUID_REGISTERS CpuidRegisters;
    unsigned int NumberOfProcessors;
    unsigned int ApicIdList[MAX_PROCESSORS];
    unsigned int ProcessorIndex;
    unsigned int MaxAddressibleIdSharingCache;
    unsigned int CacheShift;
    unsigned int CacheMask;
    unsigned int CacheId;
    CACHE_TYPE CacheType;

    /*
     *  Note that the SDM reccomends to look at CPUID.4 if CPUID.2 contains 0FFh.
     *  
     *  However, if there is no intention of decoding CPUID Leaf 2 descriptors you can
     *  just check if CPUID.4 exists and has information.  Only processors that are likely
     *  older than ~2005-2006 are likely to not have this Leaf.
     *  
     */

    Tools_ReadCpuid(0, 0, &CpuidRegisters);

    if (CpuidRegisters.x.Register.Eax >= 4) 
    {
        /*
         * This is an example and the simplest thing is dynamically hardcode to 10 caches per logical processor 
         * for simplicity. 
         */

        NumberOfProcessors = Tools_GatherPlatformApicIds(ApicIdList, MAX_PROCESSORS);
        NumberOfCaches = 0;

        pCacheInfo = (PCPUID_CACHE_INFO)calloc(NumberOfProcessors*MAX_CACHE_PER_LP, sizeof(CPUID_CACHE_INFO));

        if (pCacheInfo) 
        {

            /*
             * This will enumerate through every logical processor and check each one's cache.  It will then 
             * determine if that logical processor is expressing a new cache or an existing cache. 
             * 
             * This can be done by checking the entire CPUID Subleaf contents and using the generated Cache ID. 
             *  
             * Here we cannot assume anything about a subleaf number relationship between the enumerated cache and the 
             * other logical processors on the system which may be sharing it. 
             *  
             * This is going to be done the very long way.  There is an assumption that an enumerated cache and logical 
             * processors that are included in that ID should all have the same cache information reported.  However, as 
             * in the manual, we can verify by ensuring the entire subleaf is identical along with the generated Cache ID. 
             *  
             *  
             */

            for (ProcessorIndex = 0; ProcessorIndex < NumberOfProcessors; ProcessorIndex++) 
            {
                Tools_SetAffinity(ProcessorIndex);
                
                SubleafIndex = 0;
                Tools_ReadCpuid(4, SubleafIndex, &CpuidRegisters);
                CacheType = (CpuidRegisters.x.Register.Eax & 0x1F);

                /*
                 * Enumerate subleafs until no more caches Cache Type is retuned.
                 */
                while (CacheType != CacheType_NoMoreCaches) 
                {
                    /*
                     * Compute the Cache ID for this cache; this is called 
                     *  
                     *     Maximum Addressible IDs sharing this cache since not all APIC IDs
                     *     may be assigned to a logical processor.
                     *  
                     */
                    MaxAddressibleIdSharingCache = (((CpuidRegisters.x.Register.Eax)>>14) & 0xFFF)+1;


                    /*
                     * We round to a power of 2 and create a CacheMask that can be used with 
                     * APIC IDs to generate a Cache ID. 
                     *  
                     */
                    CacheShift = Tools_CreateTopologyShift(MaxAddressibleIdSharingCache);
                    CacheMask = ~((1<<CacheShift)-1);

                    /*
                     * This mask can be used on other logical processor's APIC IDs to find 
                     * processors sharing this cache.  We do that by using this mask on this processor's APIC ID, 
                     * then we use the mask on other logical processors. 
                     * Logical processors that match this APIC ID are then sharing the cache. 
                     *  
                     *  This would be the following, checking every APIC ID to see which ones generate the same Cache ID. 
                     *  
                     *  CacheId = ApicIdList[ProcessorIndex] & CacheMask; 
                     *  
                     *  for(RemoteProcessorIndex = 0; RemoteProcessorIndex < NumberOfApicIds; RemoteProcessorIndex++)
                     *  {
                     *     if(RemoteProcessorIndex != ProcessorIndex)
                     *     {
                     *        if(ApicIdList[RemoteProcessorIndex] & CacheMask) == CacheId)
                     *        {
                     *           // Remote Processor shares this cache
                     *        }
                     *     }
                     *  }    
                     *  
                     * This code is going to take another way, which is to instead find a matching cache id that has 
                     * the exact same CPUID Leaf 4 details.  So if the Cache ID matches and CPUID Leaf 4 matches the same 
                     * details of the cache, then it is the same cache since the CPUID details contain all the information 
                     * that identifies the cache level, type, etc. they must match if it is the same cache.  We need to check 
                     * the cache ID though, because many caches can have the same details but generate a different Cache ID 
                     * from their APIC ID and the same cache mask.  Both need to be checked in this case. 
                     *  
                     * If we do not find one, then this will be added.
                     *  
                     */
                    CacheId = ApicIdList[ProcessorIndex] & CacheMask;

                    /*
                     * Find if this cache already exists in the cache list.
                     */
                    CacheIndex = ParseCache_Internal_FindMatchingCacheEntry(pCacheInfo, NumberOfCaches, CacheId, &CpuidRegisters);

                    if(CacheIndex == INVALID_CACHE_INDEX) 
                    {
                        /*
                         * This cache does not exist, so add this cache.
                         */
                        CacheIndex = ParseCache_Internal_AddCacheEntry(pCacheInfo, &NumberOfCaches, CacheId, CacheMask, &CpuidRegisters);
                    }


                    /*
                     * Add this logical processor to the cache index, unless we had an error 
                     * and we do not have any cache to add. 
                     */
                    if(CacheIndex != INVALID_CACHE_INDEX) 
                    {
                        pCacheInfo[CacheIndex].ListOfApicIDsSharingThisCache[pCacheInfo[CacheIndex].NumberOfLPsSharingThisCache] = ApicIdList[ProcessorIndex];
                        pCacheInfo[CacheIndex].NumberOfLPsSharingThisCache++;
                    }
                    else
                    {
                        /*
                         * Should never reach here unless there were a lot of caches since 
                         * we statically select a number. 
                         */

                    }

                    SubleafIndex++;
                    Tools_ReadCpuid(4, SubleafIndex, &CpuidRegisters);
                    CacheType = (CACHE_TYPE)(CpuidRegisters.x.Register.Eax & 0x1F);
                }
            }

            Display_DisplayProcessorCaches(pCacheInfo, NumberOfCaches);

            free(pCacheInfo);
            pCacheInfo = NULL;
        }
        else
        {
            /* 
             * Memory Allocation Failure
             */
        }

    }
    else
    {
        /*
         * Does not support CPUID.4 
         */
    }
}



/*
 * ParseCache_Internal_FindMatchingCacheEntry
 *
 *    Searches the array to find a matching CacheID based on APIC ID parsing.
 * 
 *    Then there is a full CPUID Subleaf verification to determine this exactly
 *    matches the CPUID.4.n details.    
 * 
 * Arguments:
 *     Cache Array, Array Size, Cache ID to find, matching subleaf information for verification
 *     
 * Return:
 *     Index if found
 */
unsigned int ParseCache_Internal_FindMatchingCacheEntry(PCPUID_CACHE_INFO pCacheInfo, unsigned int NumberOfCaches, unsigned int CacheId, PCPUID_REGISTERS pCpuidRegisters)
{
    unsigned int CacheIndex;
    unsigned int CacheFoundIndex;

    CacheFoundIndex = INVALID_CACHE_INDEX;

    for (CacheIndex = 0; CacheIndex < NumberOfCaches && CacheFoundIndex == INVALID_CACHE_INDEX; CacheIndex++) 
    {
        if (pCacheInfo[CacheIndex].CacheId == CacheId) 
        {
            /*
             * These Cache IDs were generated independently, using possibly different cache masks.  So we need to further 
             * verify that the CPUID Leaf that generated the cache ID is identical, then we know it is the same cache and that 
             * the same cache mask was then used to generate both Cache IDs. 
             */
            if (memcmp(&pCacheInfo[CacheIndex].CachedCpuidRegisters, pCpuidRegisters, sizeof(CPUID_REGISTERS)) == 0) 
            {
                CacheFoundIndex = CacheIndex;
            }
        }
    }

    return CacheFoundIndex;
}


/*
 * ParseCache_Internal_AddCacheEntry
 *
 *    Adds a new cache entry based on the CPUID Input.
 * 
 * Arguments:
 *     Cache Array, Array Size, Cache ID to add, CPUID subleaf information describing this cache
 *     
 * Return:
 *     New Index
 */
unsigned int ParseCache_Internal_AddCacheEntry(PCPUID_CACHE_INFO pCacheInfo, unsigned int *pNumberOfCaches, unsigned int CacheId, unsigned int CacheMask, PCPUID_REGISTERS pCpuidRegisters)
{
    unsigned int NewCacheIndex;

    NewCacheIndex = *pNumberOfCaches;
    *pNumberOfCaches = *pNumberOfCaches + 1;

    /*
     * Populate the cache base information from the CPUID descriptoin
     */
    pCacheInfo[NewCacheIndex].CacheType  = (pCpuidRegisters->x.Register.Eax & 0x1F);;      /* EAX[4:0] */
    pCacheInfo[NewCacheIndex].CacheLevel = ((pCpuidRegisters->x.Register.Eax>>5) & 0x7);   /* EAX[7:5] Cache Level Starts at 1 */
    pCacheInfo[NewCacheIndex].CacheId    = CacheId;
    pCacheInfo[NewCacheIndex].CacheMask  = CacheMask;

    /*
     * Populate the cache details and calculate the size of the cache
     */
    pCacheInfo[NewCacheIndex].CacheWays       = (pCpuidRegisters->x.Register.Ebx>>22) + 1;             /* EBX[31:22] + 1 */
    pCacheInfo[NewCacheIndex].CachePartitions = ((pCpuidRegisters->x.Register.Ebx>>12) & 0x3FF) + 1;   /* EBX[21:12] + 1 */
    pCacheInfo[NewCacheIndex].CacheLineSize   = (pCpuidRegisters->x.Register.Ebx & 0xFFF) + 1;         /* EBX[11:0]  + 1 */
    pCacheInfo[NewCacheIndex].CacheSets       = pCpuidRegisters->x.Register.Ecx + 1;                   /* ECX + 1        */

    /*
     * The number of Cache Ways is multiplied by the number of Sets. 
     *  
     * This is then expanded by the number of partitions and then the cache size to get the complete number of bytes 
     * for ths size of this cache. 
     *  
     */
    pCacheInfo[NewCacheIndex].CacheSizeInBytes = pCacheInfo[NewCacheIndex].CacheWays * pCacheInfo[NewCacheIndex].CacheLineSize *
                                                 pCacheInfo[NewCacheIndex].CachePartitions * pCacheInfo[NewCacheIndex].CacheSets;

    /*
     * Populate the extra attributes of the cache.
     */
    pCacheInfo[NewCacheIndex].SelfInitializing                = (pCpuidRegisters->x.Register.Eax & (1<<8)) ? BOOL_TRUE : BOOL_FALSE;
    pCacheInfo[NewCacheIndex].CacheIsFullyAssociative         = (pCpuidRegisters->x.Register.Eax & (1<<9)) ? BOOL_TRUE : BOOL_FALSE;
    pCacheInfo[NewCacheIndex].WbinvdFlushsLowerLevelsSharing  = (pCpuidRegisters->x.Register.Edx & 1) ? BOOL_FALSE : BOOL_TRUE;
    pCacheInfo[NewCacheIndex].CacheIsInclusive                = (pCpuidRegisters->x.Register.Edx & 2) ? BOOL_TRUE : BOOL_FALSE;
    pCacheInfo[NewCacheIndex].CacheIsComplex                  = (pCpuidRegisters->x.Register.Edx & 4) ? BOOL_TRUE : BOOL_FALSE;
    pCacheInfo[NewCacheIndex].CacheIsDirectMapped             = pCacheInfo[NewCacheIndex].CacheIsComplex ? BOOL_FALSE : BOOL_TRUE;

    pCacheInfo[NewCacheIndex].NumberOfLPsSharingThisCache = 0;
    memcpy(&pCacheInfo[NewCacheIndex].CachedCpuidRegisters, pCpuidRegisters, sizeof(CPUID_REGISTERS));

    return NewCacheIndex;
}





/*
 * ParseTlb_CpuidTlbExample
 *
 *    This is an example of how to parse the CPUID TLB Information
 *    via CPUID Leaf 18H
 * 
 * Arguments:
 *     None
 *     
 * Return:
 *     None
 */
void ParseTlb_CpuidTlbExample(void)
{
    PCPUID_TLB_INFO pTlbInfo;
    unsigned int TlbIndex;
    unsigned int NumberOfTlbs;
    unsigned int SubleafIndex;
    unsigned int MaxSubleaf;
    CPUID_REGISTERS CpuidRegisters;
    unsigned int NumberOfProcessors;
    unsigned int ApicIdList[MAX_PROCESSORS];
    unsigned int ProcessorIndex;
    unsigned int MaxAddressibleIdSharingTlb;
    unsigned int TlbShift;
    unsigned int TlbMask;
    unsigned int TlbId;
    TLB_TYPE TlbType;

    Tools_ReadCpuid(0, 0, &CpuidRegisters);

    /*
     *  Note that the SDM reccomends to look at CPUID.18 if CPUID.2 contains 0FEh.
     *  
     *  However, if there is no intention of decoding CPUID Leaf 2 descriptors you can
     *  just check if CPUID.18 exists and has information. Over time, all processors will
     *  transition to returning Leaf 18H.
     *  
     */

    if (CpuidRegisters.x.Register.Eax >= 0x18) 
    {
        /*
         * This is an example and the simplest thing is dynamically hardcode to 10 TLBs per logical processor 
         * for simplicity. 
         */

        NumberOfProcessors = Tools_GatherPlatformApicIds(ApicIdList, MAX_PROCESSORS);
        NumberOfTlbs = 0;

        pTlbInfo = (PCPUID_TLB_INFO)calloc(NumberOfProcessors*MAX_TLB_PER_LP, sizeof(CPUID_TLB_INFO));

        if (pTlbInfo) 
        {

            /*
             * This will enumerate through every logical processor and check each one's TLB.  It will then 
             * determine if that logical processor is expressing a new TLB or an existing TLB. 
             * 
             * This can be done by checking the entire CPUID Subleaf contents and using the generated TLB ID. 
             *  
             * Here we cannot assume anything about a subleaf number relationship between the enumerated TLB and the 
             * other logical processors on the system which may be sharing it. 
             *  
             * This is going to be done the very long way.  There is an assumption that an enumerated TLB and logical 
             * processors that are included in that ID should all have the same TLB information reported.  However, as 
             * in the manual, we can verify by ensuring the entire subleaf is identical along with the generated TLB ID. 
             *  
             *  
             */

            for (ProcessorIndex = 0; ProcessorIndex < NumberOfProcessors; ProcessorIndex++) 
            {
                Tools_SetAffinity(ProcessorIndex);
                
                MaxSubleaf = 0;

                for (SubleafIndex = 0; SubleafIndex <= MaxSubleaf; SubleafIndex++) 
                {

                    Tools_ReadCpuid(0x18, SubleafIndex, &CpuidRegisters);

                    if (SubleafIndex == 0) 
                    {
                        /*
                         *  Subleaf 0 contains data in EBX, ECX, EDX and EAX is the maximum subleaf.
                         *  Subsequent subleafs do not use EAX.
                         */
                        MaxSubleaf = (CpuidRegisters.x.Register.Eax);
                    }

                    /*
                     * Clear EAX for all subleafs for direct compare to work easier.
                     */
                    CpuidRegisters.x.Register.Eax = 0;

                    TlbType = (CpuidRegisters.x.Register.Edx & 0x1F);  /* EDX[4:0] */

                    if (TlbType != TlbType_InvalidSubleaf) 
                    {

                        /*
                         * Compute the Tlb ID for this tlb; this is called 
                         *  
                         *     Maximum Addressible IDs sharing this translation cache since not all APIC IDs
                         *     may be assigned to a logical processor.
                         *  
                         */
                        MaxAddressibleIdSharingTlb = (((CpuidRegisters.x.Register.Edx)>>14) & 0xFFF)+1;


                        /*
                         * We round to a power of 2 and create a TlbMask that can be used with 
                         * APIC IDs to generate a Tlb ID. 
                         *  
                         */
                        TlbShift = Tools_CreateTopologyShift(MaxAddressibleIdSharingTlb);
                        TlbMask = ~((1<<TlbShift)-1);

                        /*
                         * This mask can be used on other logical processor's APIC IDs to find 
                         * processors sharing this TLB.  This code is going to instead check our 
                         * Tlb array and see if it is already there.  If not, then create a new 
                         * Tlb in the array.  If it is there, then add this APIC ID to that Tlb 
                         * and continue on. 
                         *  
                         */
                        TlbId = ApicIdList[ProcessorIndex] & TlbMask;

                        /*
                         * Find if this TLB already exists in the TLB list.
                         */
                        TlbIndex = ParseTlb_Internal_FindMatchingTlbEntry(pTlbInfo, NumberOfTlbs, TlbId, &CpuidRegisters);

                        if(TlbIndex == INVALID_TLB_INDEX) 
                        {
                            /*
                             * This TLB does not exist, so add this TLB.
                             */
                            TlbIndex = ParseTlb_Internal_AddTlbEntry(pTlbInfo, &NumberOfTlbs, TlbId, TlbMask, &CpuidRegisters);
                        }


                        /*
                         * Add this logical processor to the TLB index, unless we had an error 
                         * and we do not have any TLB to add. 
                         */
                        if(TlbIndex != INVALID_TLB_INDEX) 
                        {
                            pTlbInfo[TlbIndex].ListOfApicIDsSharingThisTlb[pTlbInfo[TlbIndex].NumberOfLPsSharingThisTlb] = ApicIdList[ProcessorIndex];
                            pTlbInfo[TlbIndex].NumberOfLPsSharingThisTlb++;
                        }
                        else
                        {
                            /*
                             * Should never reach here unless there were a lot of TLBs since 
                             * we statically select a number. 
                             */

                        }
                    }
                }
            }

            Display_DisplayProcessorTlbs(pTlbInfo, NumberOfTlbs);

            free(pTlbInfo);
            pTlbInfo = NULL;
        }
        else
        {
            /* 
             * Memory Allocation Failure
             */
        }

    }
    else
    {
        /*
         * Does not support CPUID.18H
         */
    }
}




/*
 * ParseTlb_Internal_FindMatchingTlbEntry
 *
 *    Searches the array to find a matching TlbID based on APIC ID parsing.
 * 
 *    Then there is a full CPUID Subleaf verification to determine this exactly
 *    matches the CPUID.18.n details.    
 * 
 * Arguments:
 *     Tlb Array, Array Size, Tlb ID to find, matching subleaf information for verification
 *     
 * Return:
 *     Index if found
 */
unsigned int ParseTlb_Internal_FindMatchingTlbEntry(PCPUID_TLB_INFO pTlbInfo, unsigned int NumberOfTlbs, unsigned int TlbId, PCPUID_REGISTERS pCpuidRegisters)
{
    unsigned int TlbIndex;
    unsigned int TlbFoundIndex;

    TlbFoundIndex = INVALID_TLB_INDEX;

    for (TlbIndex = 0; TlbIndex < NumberOfTlbs && TlbFoundIndex == INVALID_TLB_INDEX; TlbIndex++) 
    {
        if (pTlbInfo[TlbIndex].TlbId == TlbId) 
        {
            /*
             * Further verification this is the same by checking the complete subleaf matches, we do not care 
             * what the subleaf level was of the entry that created this cache or any that have been added, 
             * only that they are a complete same description. 
             *  
             */
            if (memcmp(&pTlbInfo[TlbIndex].CachedCpuidRegisters, pCpuidRegisters, sizeof(CPUID_REGISTERS)) == 0) 
            {
                TlbFoundIndex = TlbIndex;
            }
        }
    }

    return TlbFoundIndex;
}







/*
 * ParseTlb_Internal_AddTlbEntry
 *
 *    Adds a new Tlb entry based on the CPUID Input.
 * 
 * Arguments:
 *     Tlb Array, Array Size, Tlb ID to add, CPUID subleaf information describing this Tlb
 *     
 * Return:
 *     New Index
 */
unsigned int ParseTlb_Internal_AddTlbEntry(PCPUID_TLB_INFO pTlbInfo, unsigned int *pNumberOfTlbs, unsigned int TlbId, unsigned int TlbMask, PCPUID_REGISTERS pCpuidRegisters)
{
    unsigned int NewTlbIndex;

    NewTlbIndex = *pNumberOfTlbs;
    *pNumberOfTlbs = *pNumberOfTlbs + 1;

    /*
     * Populate the TLB base information from the CPUID descriptoin
     */
    pTlbInfo[NewTlbIndex].TlbType  = (pCpuidRegisters->x.Register.Edx & 0x1F);      /* EDX[4:0] */
    pTlbInfo[NewTlbIndex].TlbLevel = ((pCpuidRegisters->x.Register.Edx>>5) & 0x7);   /* EDX[7:5] Tlb Level Starts at 1 */
    pTlbInfo[NewTlbIndex].TlbId    = TlbId;
    pTlbInfo[NewTlbIndex].TlbMask  = TlbMask;

    /*
     * Supported page sizes  EBX[3:0]
     */
    pTlbInfo[NewTlbIndex]._4K_PageSizeEntries   = (pCpuidRegisters->x.Register.Ebx & 1) ? BOOL_TRUE : BOOL_FALSE;
    pTlbInfo[NewTlbIndex]._2MB_PageSizeEntries  = (pCpuidRegisters->x.Register.Ebx & 2) ? BOOL_TRUE : BOOL_FALSE;
    pTlbInfo[NewTlbIndex]._4MB_PageSizeEntries  = (pCpuidRegisters->x.Register.Ebx & 4) ? BOOL_TRUE : BOOL_FALSE;
    pTlbInfo[NewTlbIndex]._1GB_PageSizeEntries  = (pCpuidRegisters->x.Register.Ebx & 8) ? BOOL_TRUE : BOOL_FALSE;

    /*
     * TLB Information
     */ 
    pTlbInfo[NewTlbIndex].TlbParitioning = ((pCpuidRegisters->x.Register.Ebx>>8)&0x7);   /* EBX[10:8]  */
    pTlbInfo[NewTlbIndex].TlbWays        = ((pCpuidRegisters->x.Register.Ebx>>16)&0xFF); /* EBX[31:16] */
    pTlbInfo[NewTlbIndex].TlbSets        = (pCpuidRegisters->x.Register.Ecx);            /* ECX[31:0]  */
 
    /*
     *  TLB Attributes
     */
    pTlbInfo[NewTlbIndex].FullyAssociative = (pCpuidRegisters->x.Register.Edx & (1<<8)) ? BOOL_TRUE : BOOL_FALSE;


    pTlbInfo[NewTlbIndex].NumberOfLPsSharingThisTlb = 0;
    memcpy(&pTlbInfo[NewTlbIndex].CachedCpuidRegisters, pCpuidRegisters, sizeof(CPUID_REGISTERS));

    return NewTlbIndex;
}














   


