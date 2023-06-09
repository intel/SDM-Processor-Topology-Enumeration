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
void ParseCpu_Internal_TopologyBitsFromLeaf(unsigned int Leaf);
void ParseCpu_Internal_LegacyTopologyBits(void);
void ParseCpu_Internal_CreateDomainMaskMatrix(PAPICID_BIT_LAYOUT_CTX pApicidBitLayoutCtx);


/*
 * ParseCpu_CpuidLegacyExample
 *
 *    The legacy example of CPUID.1 and CPUID.4. Software should
 *    not use this method on modern systems, they should check for
 *    Leaf 1Fh and then Leaf Bh.
 *
 * Arguments:
 *     None
 *     
 * Return:
 *     None
 */
void ParseCpu_CpuidLegacyExample(void)
{
    unsigned int MaximumAddressibleIdsPhysicalPackage;
    unsigned int MaximumAddressibleIdsCores;
    unsigned int LogicalProcessorsPerCore;
    unsigned int LogicalProcessorsPerPackage;
    unsigned int PackageShift;
    unsigned int LogicalProcessorShift;
    CPUID_REGISTERS CpuidRegisters;

    /*  MaximumAddressibleIdsPhysicalPackage
     * 
     *      CPUID.1.EBX[23:16]
     *      Maximum number of addressable IDs for logical processors in this physical package
     *
     *  This is the legacy value for determining the package mask and has been superceded by Leaf 0Bh and Leaf 01Fh.
     *  Since this is a byte, processors are already exceeding 256 addressible IDs either due to topology domains or
     *  simply having more processors in a package.  
     *
     *
     *      CPUID.1.EDX[28].HTT
     *      The Maximum number of addressable IDs for logical processor in this package is valid when set to 1.
     *
     */

    MaximumAddressibleIdsPhysicalPackage = 1;

    Tools_ReadCpuid(1, 0, &CpuidRegisters);

    /*
     * Determine that CPUID.1.EDX[28].HTT == 1, if this is not set it would be a very old platform.
     */
    if (CpuidRegisters.x.Register.Edx & ((unsigned int)1<<28)) 
    {
        
        MaximumAddressibleIdsPhysicalPackage = (unsigned int)((CpuidRegisters.x.Register.Ebx>>16)&0xFF);

        /*
         * This would be a 20+ year old platform to not support CPUID.4
         */
        Tools_ReadCpuid(0, 0, &CpuidRegisters);

        if (CpuidRegisters.x.Register.Eax >= 4) 
        {
                /* MaximumAddressibleIdsCores
                * 
                *      CPUID.4.0.EAX[31:26]
                *       Maximum number of addressable IDs for processor cores in the physical Package
                *
                *  This is the legacy value for determining the core/SMT mask and has been superceded by Leaf 0Bh and Leaf 01Fh.
                *  Since this is 6 bits, processors are already exceeding this value addressible IDs either due to topology domains or
                *  simply having more processors in a package.  
                */
                Tools_ReadCpuid(4, 0, &CpuidRegisters);
                MaximumAddressibleIdsCores = (unsigned int)(CpuidRegisters.x.Register.Eax>>26) + 1;

                /*
                 * Determine the number of LogicalProcessors per core.
                */
                LogicalProcessorsPerCore = MaximumAddressibleIdsPhysicalPackage / MaximumAddressibleIdsCores;
                LogicalProcessorsPerPackage = MaximumAddressibleIdsPhysicalPackage;

                LogicalProcessorShift = Tools_CreateTopologyShift(LogicalProcessorsPerCore);
                PackageShift = Tools_CreateTopologyShift(LogicalProcessorsPerPackage);
        }
        else
        {
            /* 
             * You cannot report Cores here, a Package == Core and so this only reports SMT within a Package.
             */
            LogicalProcessorsPerCore = MaximumAddressibleIdsPhysicalPackage;
            LogicalProcessorsPerPackage = MaximumAddressibleIdsPhysicalPackage;

            PackageShift = Tools_CreateTopologyShift(MaximumAddressibleIdsPhysicalPackage);
            LogicalProcessorShift = PackageShift;
        }
    }
    else
    {
        /* 
         * You do not report Cores or SMT here.  It's always 1 Logical Processor.
         */
        PackageShift = Tools_CreateTopologyShift(MaximumAddressibleIdsPhysicalPackage);
        LogicalProcessorShift = PackageShift;
    }

    Display_ThreeDomainDisplay(1, PackageShift, LogicalProcessorShift);
}




/*
 * ParseCpu_CpuidThreeDomainExample
 *
 *    Performs the topology enumeration for 3 levels of
 *    topology (Logical Processor, Core, Package).
 * 
 *    The valid leaf input as of today is 0xB or 0x1F.
 *
 * Arguments:
 *     Leaf
 *     
 * Return:
 *     None
 */
void ParseCpu_CpuidThreeDomainExample(unsigned int Leaf)
{
    unsigned int LogicalProcessorShift;
    unsigned int PackageShift;
    unsigned int DomainType;
    unsigned int DomainShift;
    unsigned int Subleaf;
    CPUID_REGISTERS CpuidRegisters;

    LogicalProcessorShift = 0;
    PackageShift = 0;
    Subleaf = 0;

    Tools_ReadCpuid(Leaf, Subleaf, &CpuidRegisters);

    while (CpuidRegisters.x.Register.Ebx != 0) 
    {
        /*
        *  CPUID.B or 1F.x.ECX[15:8] = Level Type / Domain Type
        */  
        DomainType = (CpuidRegisters.x.Register.Ecx>>8) & 0xFF;

        /*
        *  CPUID.B or 1F.x.EAX[4:0] = Level Shift / Domain Shift
        */  
        DomainShift = CpuidRegisters.x.Register.Eax & 0x1F;

        /*                                                                      
         * Determine if we have enumerated Logical Processor Domain, this will 
         * always be CPUID.x.0 so can also do an ordered verification. 
         *  
         * Also determine that nothing has an invalid Domain Type. 
         *  
         */                                                                      
        switch(DomainType) 
        {
             case InvalidDomain:
                  /* Optionally log an error */
                  break;

             case LogicalProcessorDomain:
                  LogicalProcessorShift = DomainShift;
                  break;
        }

       /*
        * In three Domain topology, we do not care what the last Domain is.  Whatever
        * it is this is the mask for the package and it is also the mask for the core
        * since we are only recognizing three Domains.  It is always the core relationship
        * to the package.
        * 
        * It is incorrect to check for core id (2) because then if there was another Domain
        * above core id, you would then mistake it as the package identifier.  
        */
        PackageShift = DomainShift;

        Subleaf++;
        Tools_ReadCpuid(Leaf, Subleaf, &CpuidRegisters);
    }

    Display_ThreeDomainDisplay(Leaf, PackageShift, LogicalProcessorShift);
}




/*
 * ParseCpu_CpuidManyDomainExample
 *
 *    This algorithm will perform >3 Levels of
 *    Domains but only for known domains.  You could modify this
 *    to also enumerate unknown domains but current expectations are
 *    that software is only written to perform actions on known domains.
 * 
 *    This will collapse domains to known domains.
 * 
 *    Valid input is 0xB or 0x1F; only 0x1F can enumerate
 *    more than 3 domains of topology.
 * 
 * Arguments:
 *     Leaf
 *     
 * Return:
 *     None
 */
void ParseCpu_CpuidManyDomainExample(unsigned int Leaf)
{
    CPUID_REGISTERS CpuidRegisters;
    unsigned int Subleaf;
    unsigned int DomainType;
    unsigned int DomainShift;
    APICID_BIT_LAYOUT_CTX ApicidBitLayoutCtx;

    memset(&ApicidBitLayoutCtx, 0, sizeof(APICID_BIT_LAYOUT_CTX));
    ApicidBitLayoutCtx.NumberOfApicIdBits = 32;

    Subleaf = 0;

    Tools_ReadCpuid(Leaf, Subleaf, &CpuidRegisters);

    while (CpuidRegisters.x.Register.Ebx != 0) 
    {
        /*
         *  CPUID.B or 1F.x.ECX[15:8] = Level Type / Domain Type
         */  
        DomainType = (CpuidRegisters.x.Register.Ecx>>8) & 0xFF;

        /*
         *  CPUID.B or 1F.x.EAX[4:0] = Level Shift / Domain Shift
         */  
        DomainShift = CpuidRegisters.x.Register.Eax & 0x1F;


        /*
         * Best to check for known domains explicity since the ones you use 
         * may not be in sequential ordering. 
         */
        switch(DomainType)
        {
            case InvalidDomain:  
                 /*  This would be an error, could log it. */
            case LogicalProcessorDomain:
            case CoreDomain:
            case ModuleDomain:
            case TileDomain:
            case DieDomain:
            case DieGrpDomain:
                    ApicidBitLayoutCtx.ShiftValues[ApicidBitLayoutCtx.PackageDomainIndex] = DomainShift;
                    ApicidBitLayoutCtx.ShiftValueDomain[ApicidBitLayoutCtx.PackageDomainIndex] = DomainType;
                    ApicidBitLayoutCtx.PackageDomainIndex++;
                    break;

            default: 
                    /*
                     * First Domain is always Logical Processor, so we will always have a valid previous.
                     */
                    ApicidBitLayoutCtx.ShiftValues[ApicidBitLayoutCtx.PackageDomainIndex-1] = DomainShift;
        }

        Subleaf++;
        Tools_ReadCpuid(Leaf, Subleaf, &CpuidRegisters);
    }

    ParseCpu_Internal_CreateDomainMaskMatrix(&ApicidBitLayoutCtx);
    Display_ManyDomainExample(Leaf, &ApicidBitLayoutCtx);
}


/*
 * ParseCpu_Internal_CreateDomainMaskMatrix
 *
 *   There are many ways to create masks to identify a logical processor or domain.  There are masks
 *   that will allow unquie identity of that domain across the entire system. There are also masks that
 *   will create an ID for a domain relative to other higher domains below the entire system.  This routine
 *   generates these domains.
 * 
 * Arguments:
 *     APIC Bit Layout Context
 *     
 * Return:
 *     None
 */
void ParseCpu_Internal_CreateDomainMaskMatrix(PAPICID_BIT_LAYOUT_CTX pApicidBitLayoutCtx)
{
    unsigned int DomainIndex;
    unsigned int NextDomainIndex;
    unsigned int PreviousBit;

    PreviousBit = 0;

    /*
     * Create globally identifiable masks for each domain.
     */
    for (DomainIndex = 0; DomainIndex <= pApicidBitLayoutCtx->PackageDomainIndex; DomainIndex++) 
    {
        pApicidBitLayoutCtx->DomainRelativeMasks[DomainIndex][DomainIndex] = ~((1<<PreviousBit)-1);
        PreviousBit = pApicidBitLayoutCtx->ShiftValues[DomainIndex];
    }

    /*
     * Create a relative identifier for each Domain to another higher level Domain
     */
    for (DomainIndex = 0; DomainIndex < pApicidBitLayoutCtx->PackageDomainIndex; DomainIndex++) 
    {
        /*
         * Start to create relative IDs to the next level above the current. 
         *  
         *     A relative ID is taking the global ID mask and removing the previous mask (which is already done) and then removing the mask of
         *     the higher level domain, so for example:
         *  
         *     A global Logical processor mask would be 0xFFFFFFFF since all logical processors are the lowest identifier so the entire APIC ID is needed.
         *  
         *     A global Core mask could be:  0xFFFFFFFE  meaning the Core ID doesn't include the lower Logical Processor IDs.  This will identify the 2 Logical processors as
         *     a core globally.
         *  
         *     A global Package mask could be:  0xFFFFFFF8  Meaning we can identify this package among other packages and this package has 8 logical processors.
         *  
         *  
         *     To then create a Mask to create an ID relative to Package, we would do   ~(0xFFFFFFF8) & 0xFFFFFFFE  = 0x00000006  Essentially, you remove the ID mask for the upper domain
         *     from the global mask ID for the core.  To create the full ID though you also need to use the low bit's shift value.
         *  
         *     (APIC ID & 0x6)>>1 = CORE_ID for the Package.
         *  
         */
        for (NextDomainIndex = DomainIndex+1; NextDomainIndex <= pApicidBitLayoutCtx->PackageDomainIndex; NextDomainIndex++) 
        {
            pApicidBitLayoutCtx->DomainRelativeMasks[DomainIndex][NextDomainIndex] =  (~pApicidBitLayoutCtx->DomainRelativeMasks[NextDomainIndex][NextDomainIndex]) & (pApicidBitLayoutCtx->DomainRelativeMasks[DomainIndex][DomainIndex]);
        }
    }
}





/*
 * ParseCpu_ApicIdTopologyLayout
 *
 * Parse the APIC ID Topology Layout in a general
 * fashion using the different leafs.
 *
 * Arguments:
 *     None
 *     
 * Return:
 *     None
 */
void ParseCpu_ApicIdTopologyLayout(void)
{
    CPUID_REGISTERS CpuidRegisters;

    Tools_ReadCpuid(0, 0, &CpuidRegisters);

    if (CpuidRegisters.x.Register.Eax >= 0x1F)
    {
        ParseCpu_Internal_TopologyBitsFromLeaf(0x1F);
    }

    if (CpuidRegisters.x.Register.Eax >= 0xB)
    {
        ParseCpu_Internal_TopologyBitsFromLeaf(0xB);
    }

    ParseCpu_Internal_LegacyTopologyBits();
}

/*
 * ParseCpu_Internal_LegacyTopologyBits
 *
 * This is a legacy example.  Software should only use this
 * as a fallback if Leaf 1F and Leaf B both do not exist.
 * 
 *
 * Arguments:
 *     None
 *     
 * Return:
 *     None
 */
void ParseCpu_Internal_LegacyTopologyBits(void)
{
    CPUID_REGISTERS CpuidRegisters;
    APICID_BIT_LAYOUT_CTX ApicidBitLayoutCtx;
    unsigned int MaximumAddressibleIdsPhysicalPackage;
    unsigned int MaximumAddressibleIdsCores;
    unsigned int PackageShift;
    unsigned int LogicalProcessorsPerCore;
    unsigned int LogicalProcessorShift;

    memset(&ApicidBitLayoutCtx, 0, sizeof(ApicidBitLayoutCtx));

    ApicidBitLayoutCtx.ShiftValueDomain[0] = LogicalProcessorDomain;
    ApicidBitLayoutCtx.ShiftValueDomain[1] = CoreDomain;
    ApicidBitLayoutCtx.PackageDomainIndex  = 2;
    ApicidBitLayoutCtx.NumberOfApicIdBits  = 8;

    MaximumAddressibleIdsPhysicalPackage = 1;
    Tools_ReadCpuid(1, 0, &CpuidRegisters);


    /*
     * Determine that CPUID.1.EDX[28].HTT == 1; the meaning of this bit has changed from 
     * originally being SMT to being about Multi-Core. 
     */
    if (CpuidRegisters.x.Register.Edx & (1<<28)) 
    {
        MaximumAddressibleIdsPhysicalPackage = (unsigned int)((CpuidRegisters.x.Register.Ebx>>16)&0xFF);

        /*
         * This would be a very old platform to not have Leaf 4.
         */
        Tools_ReadCpuid(0, 0, &CpuidRegisters);

        if (CpuidRegisters.x.Register.Eax >= 4) 
        {

                /* MaximumAddressibleIdsCores
                * 
                *      CPUID.4.0.EAX[31:26]
                *       Maximum number of addressable IDs for processor cores in the physical Package
                *
                *  This is the legacy value for determining the core/SMT mask and has been superceded by Leaf 0Bh and Leaf 01Fh.
                *  Since this is 6 bits, processors are already exceeding this value addressible IDs either due to topology domains or
                *  simply having more processors in a package.  
                */
                Tools_ReadCpuid(4, 0, &CpuidRegisters);
                MaximumAddressibleIdsCores = (unsigned int)(CpuidRegisters.x.Register.Eax>>26) + 1;

                /*
                 * Determine the number of LogicalProcessors per core.
                */
                LogicalProcessorsPerCore = MaximumAddressibleIdsPhysicalPackage / MaximumAddressibleIdsCores;

                LogicalProcessorShift = Tools_CreateTopologyShift(LogicalProcessorsPerCore);

                PackageShift = Tools_CreateTopologyShift(MaximumAddressibleIdsPhysicalPackage);

                ApicidBitLayoutCtx.ShiftValues[0] = LogicalProcessorShift;
                ApicidBitLayoutCtx.ShiftValues[1] = PackageShift;

                strncpy(ApicidBitLayoutCtx.szDescription, "Legacy path using CPUID.1 and CPUID.4 (May not be correct if Leaf B or Leaf 1F exist.)", sizeof(ApicidBitLayoutCtx.szDescription)-1);
        }
        else
        {
            /* 
             * This is no Cores, only logical processors in a package (i.e. SMT)
             */
            PackageShift = Tools_CreateTopologyShift(MaximumAddressibleIdsPhysicalPackage);

            ApicidBitLayoutCtx.ShiftValues[0] = PackageShift;
            ApicidBitLayoutCtx.PackageDomainIndex  = 1;
            strncpy(ApicidBitLayoutCtx.szDescription, "Legacy path using CPUID.1 and CPUID.HTT = 1 but no CPUID.4", sizeof(ApicidBitLayoutCtx.szDescription)-1);
        }
    }
    else
    {
        /* 
         * Without any enumeration of CPUID existing, then it's just one logical processor per package.
         */
        PackageShift = Tools_CreateTopologyShift(MaximumAddressibleIdsPhysicalPackage);
        ApicidBitLayoutCtx.ShiftValues[0] = PackageShift;
        ApicidBitLayoutCtx.PackageDomainIndex  = 1;
        strncpy(ApicidBitLayoutCtx.szDescription, "Legacy path where CPUID.HTT = 0", sizeof(ApicidBitLayoutCtx.szDescription)-1);
    }

    Display_ApicIdBitLayout(&ApicidBitLayoutCtx);
}

/*
 * ParseCpu_Internal_TopologyBitsFromLeaf
 *
 *    Parse the full topology and display the APIC ID bits.
 *    this will also deal with unknown levels and first
 *    display the full APICID layout and then if any
 *    unknown domains were enumerated it will collapse
 *    them into the previous known domain.
 * 
 *    The input is either 0xB or 0x1F, where 0xB
 *    will only ever return 3 level topology.
 *    
 *
 * Arguments:
 *     CPUID Leaf
 *     
 * Return:
 *     None
 */
void ParseCpu_Internal_TopologyBitsFromLeaf(unsigned int Leaf)
{
    BOOL_TYPE bFoundUnknownDomain;
    unsigned int Subleaf;
    unsigned int DomainShift;
    unsigned int DomainType;
    CPUID_REGISTERS CpuidRegisters;
    APICID_BIT_LAYOUT_CTX ApicidBitLayoutCtx;

    memset(&ApicidBitLayoutCtx, 0, sizeof(ApicidBitLayoutCtx));

    sprintf(ApicidBitLayoutCtx.szDescription, "****  APIC ID Bit Layout CPUID.0x%0x ****\n\n", Leaf);
    ApicidBitLayoutCtx.NumberOfApicIdBits = 32;

    Subleaf = 0;

    Tools_ReadCpuid(Leaf, Subleaf, &CpuidRegisters);

    bFoundUnknownDomain = BOOL_FALSE;


    /*
     * In this first example, the unknown domains will not be collapsed but 
     * shown in the APIC ID layout.  If unknown domains are found, then it will 
     * provide a second example with them collasped. 
     */

    while (CpuidRegisters.x.Register.Ebx != 0) 
    {
        /*
         *  CPUID.B or 1F.x.ECX[15:8] = Level Type / Domain Type
         */  
        DomainType = (CpuidRegisters.x.Register.Ecx>>8) & 0xFF;

        /*
         *  CPUID.B or 1F.x.EAX[4:0] = Level Shift / Domain Shift
         */  
        DomainShift = CpuidRegisters.x.Register.Eax & 0x1F;

        ApicidBitLayoutCtx.ShiftValues[Subleaf] = DomainShift;
        ApicidBitLayoutCtx.ShiftValueDomain[Subleaf] = DomainType;
        ApicidBitLayoutCtx.PackageDomainIndex++;

        /*
         * Best to check for known domains explicity since the ones you use 
         * may not be in sequential ordering. 
         */
        switch(DomainType)
        {
            case InvalidDomain: 
                 /*  This would be an error, could log it. */
            case LogicalProcessorDomain:
            case CoreDomain:
            case ModuleDomain:
            case TileDomain:
            case DieDomain:
            case DieGrpDomain:
                    break;

            default: 
                bFoundUnknownDomain = BOOL_TRUE;
        }

        Subleaf++;
        Tools_ReadCpuid(Leaf, Subleaf, &CpuidRegisters);
    }

    Display_ApicIdBitLayout(&ApicidBitLayoutCtx);


    if (bFoundUnknownDomain) 
    {
            sprintf(ApicidBitLayoutCtx.szDescription, "\nFound Unknown Domains, Consolidated APIC ID\n\n");
            ApicidBitLayoutCtx.PackageDomainIndex = 0;

            Subleaf = 0;
            Tools_ReadCpuid(Leaf, Subleaf, &CpuidRegisters);

            bFoundUnknownDomain = BOOL_FALSE;

            /*
            * Collaspe unknown domains into known domains and display the known APIC ID layout.
            */
            while (CpuidRegisters.x.Register.Ebx != 0) 
            {
                /*
                 *  CPUID.B or 1F.x.ECX[15:8] = Level Type / Domain Type
                 */  
                DomainType = (CpuidRegisters.x.Register.Ecx>>8) & 0xFF;

                /*
                 *  CPUID.B or 1F.x.EAX[4:0] = Level Shift / Domain Shift
                 */  
                DomainShift = CpuidRegisters.x.Register.Eax & 0x1F;

                /*
                 * Best to check for known domains explicity since the ones you use 
                 * may not be in sequential ordering. 
                 */
                switch(DomainType)
                {
                    case InvalidDomain:
                         /*  This would be an error, could log it. */
                    case LogicalProcessorDomain:
                    case CoreDomain:
                    case ModuleDomain:
                    case TileDomain:
                    case DieDomain:
                    case DieGrpDomain:
                            ApicidBitLayoutCtx.ShiftValues[ApicidBitLayoutCtx.PackageDomainIndex] = DomainShift;
                            ApicidBitLayoutCtx.ShiftValueDomain[ApicidBitLayoutCtx.PackageDomainIndex] = DomainType;
                            ApicidBitLayoutCtx.PackageDomainIndex++;
                            break;

                    default: 
                        /*
                         * First Domain is always Logical Processor, so we will always have a valid previous. 
                         * Not doing any error checking here and assumption that hardware reported the correct details. 
                         * If this is reported incorrectly and it is detected then it would be possible to report 
                         * an error, bail out or attempt to work with incorrect information. 
                         */
                        ApicidBitLayoutCtx.ShiftValues[ApicidBitLayoutCtx.PackageDomainIndex-1] = DomainShift;
                }
   
                Subleaf++;
                Tools_ReadCpuid(Leaf, Subleaf, &CpuidRegisters);
            }

            Display_ApicIdBitLayout(&ApicidBitLayoutCtx);
    }
}

