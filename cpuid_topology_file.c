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
 * Context structures to maintain state during parsing of the 
 * CPUID files for Read and write. 
 */
typedef struct _FILE_READ_CONTEXT
{
    FILE *CpuidFile;

    /*
     * Maintains the current leaf state machine for reading subsequent subleafs.
     */
    unsigned int CurrentLeaf;

    /*
     * Maintains the processor relation index of the current CPUID.4 reads
     */
    unsigned int Leaf4Index;

    /*
     * Maintains the processor relation index of the current CPUID.18 reads
     */ 
    unsigned int Leaf18Index;

} FILE_READ_CONTEXT, *PFILE_READ_CONTEXT;

typedef struct _FILE_WRITE_CONTEXT
{
    FILE *CpuidFile;
    unsigned int NumberOfProcessors;

} FILE_WRITE_CONTEXT, *PFILE_WRITE_CONTEXT;

/*
 *  Internal Prototypes
 */
BOOL_TYPE File_Internal_DispatchReadLeaf(PFILE_READ_CONTEXT pFileContext, unsigned int LeafNumber);
BOOL_TYPE File_Internal_DispatchReadSubleaf(PFILE_READ_CONTEXT pFileContext, unsigned int SubleafNumber);
BOOL_TYPE File_Internal_DispatchReadApicId(PFILE_READ_CONTEXT pFileContext, unsigned int ApicIdNumber);
BOOL_TYPE File_Internal_WriteLeafToFile(PFILE_WRITE_CONTEXT pFileContext, unsigned int LeafNumber);
BOOL_TYPE File_Internal_WriteApicIdsToFile(PFILE_WRITE_CONTEXT pFileContext);


/*
 * File_ReadCpuidFromFile
 *
 * This function will read a file that contains
 * CPUID data.  This will populate the global
 * with the fake CPUID data to be used with the CPUID
 * algorithms.
 * 
 *
 * Arguments:
 *     File Name
 *     
 * Return:
 *     Returns true if successful
 */
BOOL_TYPE File_ReadCpuidFromFile(char *pszFileName)
{
    FILE_READ_CONTEXT FileContext;
    char Character;
    unsigned int FirstData;
    BOOL_TYPE FileReadStatus;

    FileReadStatus = BOOL_FALSE;

    memset(&FileContext, 0, sizeof(FILE_READ_CONTEXT));

    /* 
     * This is a simple file format for reading in data from a file and creating 
     * a CPUID thunk from that data. 
     *  
     *   
     * The File Format Legend 
     *  
     *    The Leaf encoding specifies a line starting with "L" and then a space with a number for that Leaf.
     *    This is then followed by a new line.  The leaf is in decimal value.
     *  
     *      L [Leaf Number]
     *  
     *    After a leaf encoding, subsequent values following it must be a series of lines starting with "S" to identify
     *    this is a subleaf.  This is then followed by the subleaf number and the register values in decimal.  These
     *    subleafs are then associated with the specified preceeding L directive line.
     *  
     *       S [Subleaf Number] [EAX] [EBX] [ECX] [EDX]
     *  
     *    This simulation is very simple and only expects one entry for each Leaf except for Leaf 4 and Leaf 18H.
     *    Each subsequent description of a new Leaf 4 or Leaf 18H will for that leaf associate it with an incremental
     *    processor number thus creating an association between the list of APIC IDs and that leaf as tied to a specific
     *    processor.
     *  
     *    The APIC IDs for the logical processors are represented by a line that starts with "A" followed by a space and
     *    then the APIC ID value in decimal.  Each subsequent APIC ID will be associated with the next numerical logical processor.
     *  
     *       A [APIC ID Value]
     *  
     *    These values must be capital.
     *  
     *    Leaves which do not enumerate multiple subleafs simply enumerate a single subleaf of 0.
     *  
     *      For example:
     *      L 1
     *      S 0 1 2 3 4
     *      A 10
     *  
     *      
     *    This code is simple and does not do any extenisve level of error checking.  
     *  
     */ 

    FileContext.CpuidFile = fopen(pszFileName, "r");

    /*
    * Always switch to Virtual CPUID; if the file does not contain CPUID information 
    * then it is invalid anyway. 
    */
    g_GlobalData.UseNativeCpuid = BOOL_FALSE;  


    if(FileContext.CpuidFile) 
    {

        while (!feof(FileContext.CpuidFile))
        {
            if (fscanf(FileContext.CpuidFile, "%c %i\n", &Character, &FirstData) != 0) 
            {
                switch (Character) 
                {
                    case 'L':
                        FileReadStatus = File_Internal_DispatchReadLeaf(&FileContext, FirstData);
                        break;

                    case 'S':
                        FileReadStatus = File_Internal_DispatchReadSubleaf(&FileContext, FirstData);
                        break;

                    case 'A':
                        FileReadStatus = File_Internal_DispatchReadApicId(&FileContext, FirstData);
                        break;

                   default:
                        FileReadStatus = BOOL_FALSE;
                }

            }
        }

        fclose(FileContext.CpuidFile);
        FileContext.CpuidFile = NULL;
    }

    return FileReadStatus;
}


/*
 * File_Internal_DispatchReadLeaf
 *
 * This function will update the current leaf number.  There is no error checking
 * for valid leaf or reusing an already processed leaf number.
 *
 * Arguments:
 *     File Context, Leaf Number
 *     
 * Return:
 *     Returns true if successful
 */
BOOL_TYPE File_Internal_DispatchReadLeaf(PFILE_READ_CONTEXT pFileContext, unsigned int LeafNumber)
{
    pFileContext->CurrentLeaf = LeafNumber;

    switch (LeafNumber) 
    {
        case 4:
             pFileContext->Leaf4Index++;
             break;

        case 0x18:
             pFileContext->Leaf18Index++;
             break;
    }

    return BOOL_TRUE;
}


/*
 * File_Internal_DispatchReadSubleaf
 *
 * This function will update the subleaf for the current leaf number.
 * 
 * There is no error checking valid subleaf or rewriting the same subleaf.
 *
 * Arguments:
 *     File Context, Subleaf Number
 *     
 * Return:
 *     Returns true if successful
 */
BOOL_TYPE File_Internal_DispatchReadSubleaf(PFILE_READ_CONTEXT pFileContext, unsigned int SubleafNumber)
{
    BOOL_TYPE SubleafSuccess;
    unsigned int Eax;
    unsigned int Ebx;
    unsigned int Ecx;
    unsigned int Edx;

    SubleafSuccess = BOOL_FALSE;

    if (fscanf(pFileContext->CpuidFile, "%u %u %u %u", &Eax, &Ebx, &Ecx, &Edx) != 0) 
    {
        SubleafSuccess = BOOL_TRUE;

        switch (pFileContext->CurrentLeaf) 
        {
            case 4:
                 g_GlobalData.SimulatedCpuidLeaf4[pFileContext->Leaf4Index-1][SubleafNumber][0] = Eax;
                 g_GlobalData.SimulatedCpuidLeaf4[pFileContext->Leaf4Index-1][SubleafNumber][1] = Ebx;
                 g_GlobalData.SimulatedCpuidLeaf4[pFileContext->Leaf4Index-1][SubleafNumber][2] = Ecx;
                 g_GlobalData.SimulatedCpuidLeaf4[pFileContext->Leaf4Index-1][SubleafNumber][3] = Edx;
                 printf("Proc %i Leaf %08x Subleaf %u EAX: %08x EBX; %08x ECX: %08x EDX; %08x\n", pFileContext->Leaf4Index-1, pFileContext->CurrentLeaf, SubleafNumber, Eax, Ebx, Ecx, Edx);
                 break;

           case 0x18:
                 g_GlobalData.SimulatedCpuidLeaf18[pFileContext->Leaf18Index-1][SubleafNumber][0] = Eax;
                 g_GlobalData.SimulatedCpuidLeaf18[pFileContext->Leaf18Index-1][SubleafNumber][1] = Ebx;
                 g_GlobalData.SimulatedCpuidLeaf18[pFileContext->Leaf18Index-1][SubleafNumber][2] = Ecx;
                 g_GlobalData.SimulatedCpuidLeaf18[pFileContext->Leaf18Index-1][SubleafNumber][3] = Edx;
                 printf("Proc %i Leaf %08x Subleaf %u EAX: %08x EBX; %08x ECX: %08x EDX; %08x\n", pFileContext->Leaf18Index-1, pFileContext->CurrentLeaf, SubleafNumber, Eax, Ebx, Ecx, Edx);
                 break;

            default:
                 if (pFileContext->CurrentLeaf < MAX_SIMULATED_LEAFS && SubleafNumber < MAX_SIMULATED_SUBLEAFS)
                 {
                     g_GlobalData.SimulatedCpuid[pFileContext->CurrentLeaf][SubleafNumber][0] = Eax;
                     g_GlobalData.SimulatedCpuid[pFileContext->CurrentLeaf][SubleafNumber][1] = Ebx;
                     g_GlobalData.SimulatedCpuid[pFileContext->CurrentLeaf][SubleafNumber][2] = Ecx;
                     g_GlobalData.SimulatedCpuid[pFileContext->CurrentLeaf][SubleafNumber][3] = Edx;
                     printf("Leaf %08x Subleaf %u EAX: %08x EBX; %08x ECX: %08x EDX; %08x\n", pFileContext->CurrentLeaf, SubleafNumber, Eax, Ebx, Ecx, Edx);
                 }
                 else
                 {
                     printf("Skipping entry beyond supported maximum leaf/subleafs 0x%x, %i\n", pFileContext->CurrentLeaf, SubleafNumber);
                 }
        }
    }

    return SubleafSuccess;
}



/*
 * File_Internal_DispatchReadApicId
 *
 * This function will update the APIC ID of the next processor
 * 
 * There is no error checking, same APIC ID could be submitted twice.
 *
 * Arguments:
 *     File Context, APIC ID Number
 *     
 * Return:
 *     Returns true if successful
 */
BOOL_TYPE File_Internal_DispatchReadApicId(PFILE_READ_CONTEXT pFileContext, unsigned int ApicIdNumber)
{
     if (g_GlobalData.NumberOfSimulatedProcessors < MAX_PROCESSORS) 
     {
        g_GlobalData.SimulatedApicIds[g_GlobalData.NumberOfSimulatedProcessors] = ApicIdNumber;
        printf("Processor %i  - ApicID - %08x\n", g_GlobalData.NumberOfSimulatedProcessors, ApicIdNumber);
        g_GlobalData.NumberOfSimulatedProcessors++;
     }
     else
     {
         printf("Too many processors in file, skipping APICID %0x\n", ApicIdNumber);
     }

     return BOOL_TRUE;
}




/*
 * File_WriteCpuidToFile
 *
 * Write the CPUID values to a file.
 *
 * Arguments:
 *     File Name
 *     
 * Return:
 *     Returns true if successful
 */
BOOL_TYPE File_WriteCpuidToFile(char *pszFileName)
{
    FILE_WRITE_CONTEXT FileWriteContext;
    CPUID_REGISTERS CpuidRegisters;
    unsigned int MaximumLeaf;
    unsigned int Index;
    unsigned int ApicId;
    BOOL_TYPE FileWritten;

    FileWritten = BOOL_FALSE;

    memset(&FileWriteContext, 0, sizeof(FILE_WRITE_CONTEXT));

    /* 
     * This is a simple file format for reading in data from a file and creating 
     * a CPUID thunk from that data. 
     *  
     *   
     * The File Format Legend 
     *  
     *    The Leaf encoding specifies a line starting with "L" and then a space with a number for that Leaf.
     *    This is then followed by a new line.  The leaf is in decimal value.
     *  
     *      L [Leaf Number]
     *  
     *    After a leaf encoding, subsequent values following it must be a series of lines starting with "S" to identify
     *    this is a subleaf.  This is then followed by the subleaf number and the register values in decimal.  These
     *    subleafs are then associated with the specified preceeding L directive line.
     *  
     *       S [Subleaf Number] [EAX] [EBX] [ECX] [EDX]
     *  
     *    This simulation is very simple and only expects one entry for each Leaf except for Leaf 4 and Leaf 18H.
     *    Each subsequent description of a new Leaf 4 or Leaf 18H will for that leaf associate it with an incremental
     *    processor number thus creating an association between the list of APIC IDs and that leaf as tied to a specific
     *    processor.
     *  
     *    The APIC IDs for the logical processors are represented by a line that starts with "A" followed by a space and
     *    then the APIC ID value in decimal.  Each subsequent APIC ID will be associated with the next numerical logical processor.
     *  
     *       A [APIC ID Value]
     *  
     *    These values must be capital.
     *  
     *    Leaves which do not enumerate multiple subleafs simply enumerate a single subleaf of 0.
     *  
     *      For example:
     *      L 1
     *      S 0 1 2 3 4
     *      A 10
     *  
     *      
     *    This code is simple and does not do any extenisve level of error checking.  
     *  
     */
    FileWriteContext.CpuidFile = fopen(pszFileName, "w");

    if(FileWriteContext.CpuidFile) 
    {
        FileWritten = BOOL_TRUE;

        Tools_ReadCpuid(0, 0, &CpuidRegisters);
        MaximumLeaf = CpuidRegisters.x.Register.Eax;
        FileWriteContext.NumberOfProcessors = Tools_GetNumberOfProcessors();


        File_Internal_WriteLeafToFile(&FileWriteContext, 0);

        if (MaximumLeaf >= 1)
        {
            File_Internal_WriteLeafToFile(&FileWriteContext, 1);
        }

        if (MaximumLeaf >= 0x4)
        {
            for (Index = 0; Index < FileWriteContext.NumberOfProcessors; Index++) 
            {
                printf("* Processor %i\n", Index);
                Tools_SetAffinity(Index);
                File_Internal_WriteLeafToFile(&FileWriteContext, 0x4);
            }
        }

        if (MaximumLeaf >= 0xB)
        {
            File_Internal_WriteLeafToFile(&FileWriteContext, 0xB);
        }

        if (MaximumLeaf >= 0x18)
        {
            for (Index = 0; Index < FileWriteContext.NumberOfProcessors; Index++) 
            {
                printf("* Processor %i\n", Index);
                Tools_SetAffinity(Index);
                File_Internal_WriteLeafToFile(&FileWriteContext, 0x18);
            }
        }

        if (MaximumLeaf >= 0x1F)
        {
            File_Internal_WriteLeafToFile(&FileWriteContext, 0x1F);
        }


        for (Index = 0; Index < FileWriteContext.NumberOfProcessors; Index++) 
        {
            Tools_SetAffinity(Index);

            if (CpuidRegisters.x.Register.Eax >= 0xB)
            {
                Tools_ReadCpuid(0xB, 0, &CpuidRegisters);
                ApicId = CpuidRegisters.x.Register.Edx;
            }
            else
            {
                Tools_ReadCpuid(1, 0, &CpuidRegisters);
                ApicId = (CpuidRegisters.x.Register.Ebx >> 24);
            }
            fprintf(FileWriteContext.CpuidFile, "A %i\n", ApicId);
            printf("A %i\n", ApicId);
        }

        fclose(FileWriteContext.CpuidFile);
    }

    return FileWritten;
}



/*
 * File_Internal_WriteLeafToFile
 *
 * Write a leaf and subleafs into a file.
 *
 * Arguments:
 *     File Context, Leaf Number
 *     
 * Return:
 *     Return true on success
 */
BOOL_TYPE File_Internal_WriteLeafToFile(PFILE_WRITE_CONTEXT pFileContext, unsigned int LeafNumber)
{
    CPUID_REGISTERS OriginalCpuidRegisters;
    CPUID_REGISTERS CpuidRegisters;
    PCPUID_REGISTERS pCpuidReadValues;
    unsigned int CurrentSubleaf;
    BOOL_TYPE LeafWritten;
    BOOL_TYPE NextSubleaf;

    fprintf(pFileContext->CpuidFile, "L %i\n", LeafNumber);
    printf("L %i\n", LeafNumber);

    CurrentSubleaf = 0;

    LeafWritten = BOOL_TRUE;
    
    pCpuidReadValues = &OriginalCpuidRegisters;

    do {

        Tools_ReadCpuid(LeafNumber, CurrentSubleaf, pCpuidReadValues);

        fprintf(pFileContext->CpuidFile, "S %u %u %u %u %u\n", CurrentSubleaf, pCpuidReadValues->x.Register.Eax, pCpuidReadValues->x.Register.Ebx, pCpuidReadValues->x.Register.Ecx, pCpuidReadValues->x.Register.Edx);
        printf("S %u %u %u %u %u\n", CurrentSubleaf, pCpuidReadValues->x.Register.Eax, pCpuidReadValues->x.Register.Ebx, pCpuidReadValues->x.Register.Ecx, pCpuidReadValues->x.Register.Edx);
        CurrentSubleaf++;

        switch (LeafNumber) 
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

    return LeafWritten;
}




