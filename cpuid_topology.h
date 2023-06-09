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


#ifndef TOPOLOGY_H__
#define TOPOLOGY_H__


/*
 * The following constants are used to simplify the code and in 
 * many places is used for static array creation as opposed to dynamically 
 * at runtime determining the size. 
 * 
 */
#define MAX_PROCESSORS          1024
#define MAX_SIMULATED_LEAFS     0x20
#define MAX_SIMULATED_SUBLEAFS  10
#define MAX_CACHE_PER_LP        10
#define MAX_TLB_PER_LP          25
#define INVALID_CACHE_INDEX     ((unsigned int)-1)
#define INVALID_TLB_INDEX       ((unsigned int)-1)
 
/*
 * The maximum number of enumerated domains, since X2APIC is 32 bits there 
 * really can't be more than 32 domains enumerated. 
 */
#define MAXIMUM_DOMAINS          32

/*
 * Constants for cache / tlb size enumerations
 */
#define BYTES_IN_KB  (1024)
#define BYTES_IN_MB  (1048576)




/*
 * CPUID structure for using the return data of CPUID 
 * in the application. 
 */
typedef struct _CPUID_REGISTERS {

    union {
        unsigned int Registers[4];
        struct {
            unsigned int Eax;
            unsigned int Ebx;
            unsigned int Ecx;
            unsigned int Edx;
        } Register;
    } x;

} CPUID_REGISTERS, *PCPUID_REGISTERS;

/*
 * Create a platform independnet boolean type
 */
typedef enum _BOOL_TYPE {
    BOOL_FALSE = 0,
    BOOL_TRUE
} BOOL_TYPE, *PBOOL_TYPE;


/*
 * Caching Structure
 */
typedef struct _CPUID_CACHE_INFO 
{
    /* 
     * The Cache Type (Data, Instruction) and then the Level (L1, L2,...)
     */
    unsigned int CacheType;
    unsigned int CacheLevel;

    /*
     * The Cache Id that identifies a particular cache and the 
     * APIC ID mask used to determine the Cache Id. 
     */
    unsigned int CacheId;
    unsigned int CacheMask;

    /*
     * Physical description of the cache.
     */
    unsigned int CacheWays;
    unsigned int CachePartitions;
    unsigned int CacheLineSize;
    unsigned int CacheSets;

    unsigned int CacheSizeInBytes;

    /*
     * Attributes of the cache.
     */
    BOOL_TYPE SelfInitializing;
    BOOL_TYPE CacheIsFullyAssociative;       
    BOOL_TYPE WbinvdFlushsLowerLevelsSharing;
    BOOL_TYPE CacheIsInclusive;              
    BOOL_TYPE CacheIsDirectMapped;           
    BOOL_TYPE CacheIsComplex;                

    /*
     * The list of shared APIC IDs sharing this cache.
     */
    unsigned int NumberOfLPsSharingThisCache;
    unsigned int ListOfApicIDsSharingThisCache[MAX_PROCESSORS];

    /*
     * The CPUID description of this cache.
     */
    CPUID_REGISTERS CachedCpuidRegisters;

} CPUID_CACHE_INFO, *PCPUID_CACHE_INFO;


/*
 * TLB Structure
 */
typedef struct _CPUID_TLB_INFO 
{
    /* 
     * The TLB Type (Data, Instruction) and then the Level (L1, L2,...)
     */
    unsigned int TlbType;
    unsigned int TlbLevel;

    /*
     * The TLB Id that identifies a particular TLB and the 
     * APIC ID mask used to determine the TLB Id. 
     */
    unsigned int TlbId;
    unsigned int TlbMask;

    /*
     * Physical description of the TLB.
     */
    unsigned int TlbWays;
    unsigned int TlbParitioning;
    unsigned int TlbSets;

    /*  
     *  TLB Supported Page Sizes
     */
    BOOL_TYPE _4K_PageSizeEntries;
    BOOL_TYPE _2MB_PageSizeEntries;
    BOOL_TYPE _4MB_PageSizeEntries;
    BOOL_TYPE _1GB_PageSizeEntries;

    /*
     * Attributes of the TLB.
     */
    BOOL_TYPE FullyAssociative;

    /*
     * The list of shared APIC IDs sharing this TLB.
     */
    unsigned int NumberOfLPsSharingThisTlb;
    unsigned int ListOfApicIDsSharingThisTlb[MAX_PROCESSORS];

    /*
     * The CPUID description of this TLB.
     */
    CPUID_REGISTERS CachedCpuidRegisters;

} CPUID_TLB_INFO, *PCPUID_TLB_INFO;






/*
 * The enumeration of domain identifiers and these need to each match 
 * the value as specified by CPUID.1F and CPUID.B documentation. 
 */
typedef enum  _CPU_DOMAIN {
    InvalidDomain = 0,
    LogicalProcessorDomain,
    CoreDomain,
    ModuleDomain,
    TileDomain,
    DieDomain,
    DieGrpDomain
} CPU_DOMAIN, *PCPU_DOMAIN;


/*
 * Create an enumeration of cache types.
 */
typedef enum _CACHE_TYPE {
    CacheType_NoMoreCaches = 0,
    CacheType_DataCache,
    CacheType_InstructionCache,
    CacheType_UnifiedCache
} CACHE_TYPE, *PCACHE_TYPE;


/*
 * Create an enumeration of tlb types.
 */
typedef enum _TLB_TYPE {
    TlbType_InvalidSubleaf = 0,
    TlbType_Data,
    TlbType_Instruction,
    TlbType_Unified,
    TlbType_LoadOnly,
    TlbType_StoreOnly
} TLB_TYPE, *PTLB_TYPE;




/* 
 * This data structure is used to communicate CPU Topology structure 
 * values and bits as derived from CPUID.B or CPUID.1F.  It can also 
 * be used for legacy as hand coded for the APIs that use it. 
 */

typedef struct _APICID_BIT_LAYOUT_CTX {

    /*
     * To support this as legacy APIC, the structure will contain the number 
     * of bits that represent an APIC ID, which has been 4, 8 and 32(Today). 
     *  
     * This code only will set it to 8 or 32. 
     */
    unsigned int NumberOfApicIdBits;

    /*
     * These are a cache of CPUID Topology as returned from CPUID.1F or CPUID.B. 
     *  
     * The usage beyond mirroring the values in a simple structure is that these 
     * values can contain a collapsed version from Unknown Domains to a list of 
     * all known domains or other number of levels. 
     * 
     */
    unsigned int ShiftValues[MAXIMUM_DOMAINS];
    unsigned int ShiftValueDomain[MAXIMUM_DOMAINS];


    /*
     * This is a domain relative where the index is based on the domain 
     * level index.  The second index determines the relative to the current 
     * domain mask.  The index where both entries are the current domain represents 
     * a global mask to ID this domain level globally. 
     *  
     * The indexes then move to the next higher domian creating a relative mask from the 
     * current domain relative to the second domain level index. 
     * 
     */
    unsigned int DomainRelativeMasks[MAXIMUM_DOMAINS][MAXIMUM_DOMAINS];

    /*
     * The top index in the above matrix that contains the package domain.
     */
    unsigned int PackageDomainIndex;

    /*
     * This is a string that allows a description to be passed from the parsing function 
     * to the general display function for context. 
     */
    char szDescription[256];

} APICID_BIT_LAYOUT_CTX, *PAPICID_BIT_LAYOUT_CTX;



/*
 * Internal Structures and types
 */
typedef struct _GLOBAL_DATA {
    
    /*
     *  This determines if the Virtual CPUID or the Native CPUID should be used.
     */
    BOOL_TYPE UseNativeCpuid;

    /*
     *  The Virtual CPUID simulation values.
     */
    unsigned int SimulatedCpuid[MAX_SIMULATED_LEAFS][MAX_SIMULATED_SUBLEAFS][4];

    /*
     *  The Virtual CPUID simulation value for CPUID.4; since it is asymmetric.
     */
    unsigned int SimulatedCpuidLeaf4[MAX_PROCESSORS][MAX_SIMULATED_SUBLEAFS][4];

    /*
     *  The Virtual CPUID simulation value for CPUID.18; since it is asymmetric.
     */
    unsigned int SimulatedCpuidLeaf18[MAX_PROCESSORS][MAX_SIMULATED_SUBLEAFS][4];

    /*
     * The list of the Simulated APIC IDs.
     */
    unsigned int SimulatedApicIds[MAX_PROCESSORS];

    /*
     * The number of simulated processors .
     */
    unsigned int NumberOfSimulatedProcessors;

    /*
     * The main execution thread's Processor Affinity Number.
     */
    unsigned int CurrentProcessorAffinity;


} GLOBAL_DATA, *PGLOBAL_DATA;



/*
 *  Parse CPUID Topology Information APIs
 */
void ParseCpu_ApicIdTopologyLayout(void);
void ParseCpu_CpuidLegacyExample(void);
void ParseCpu_CpuidThreeDomainExample(unsigned int Leaf);
void ParseCpu_CpuidManyDomainExample(unsigned int Leaf);



/*
 *  Parse CPUID Cache and Tlbs APIs
 */
void ParseCache_CpuidCacheExample(void);
void ParseTlb_CpuidTlbExample(void);

/*
 *  Display APIs
 */
void Display_DisplayParameters(void);
void Display_ApicIdBitLayout(PAPICID_BIT_LAYOUT_CTX pApicidBitLayoutCtx);
void Display_ThreeDomainDisplay(unsigned int Leaf, unsigned int PackageShift, unsigned int LogicalProcessorShift);
void Display_DisplayProcessorLeafs(unsigned int NumberOfProcessors);
void Display_ManyDomainExample(unsigned int Leaf, PAPICID_BIT_LAYOUT_CTX pApicidBitLayoutCtx);
void Display_DisplayProcessorCaches(PCPUID_CACHE_INFO pCacheInfo, unsigned int NumberOfCaches);
void Display_DisplayProcessorTlbs(PCPUID_TLB_INFO pTlbInfo, unsigned int NumberOfTlbs);

/*
 * Common Support Tools and Initialization APIs
 */
void Tools_ReadCpuid(unsigned int Leaf, unsigned int Subleaf, PCPUID_REGISTERS pCpuidRegisters);
unsigned int Tools_CreateTopologyShift(unsigned int count);
void Tools_SetAffinity(unsigned int ProcessorNumber);
unsigned int Tools_GetNumberOfProcessors(void);
BOOL_TYPE Tools_IsNative(void);
BOOL_TYPE Tools_IsDomainKnownEnumeration(unsigned int Domain);
unsigned int Tools_GatherPlatformApicIds(unsigned int *pApicIdArray, unsigned int ArraySize);


/*
 *  File Read/Write CPUID APIs
 */
BOOL_TYPE File_ReadCpuidFromFile(char *pszFileName);
BOOL_TYPE File_WriteCpuidToFile(char *pszFileName);

/*
 *  OS-Specific Implementation APIs
 */
void Os_DisplayTopology(void);
void Os_Platform_Read_Cpuid(unsigned int Leaf, unsigned int Subleaf, PCPUID_REGISTERS pCpuidRegisters);
unsigned int Os_GetNumberOfProcessors(void);
void Os_SetAffinity(unsigned int ProcessorNumber);


#endif


