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
 * Function Pointer Definition for Handling command line input for a specific command
 */
typedef void (*PFN_DISPATCHFUNC)(unsigned int NumberOfParameters, char **Parameters);

/*
 * Global application data variable
 */
extern GLOBAL_DATA g_GlobalData;

/*
 * Command Line Dispatch Definition
 */
typedef struct _DISPATCH_COMMAND {
    char LowerCase;
    PFN_DISPATCHFUNC pfnDispatchFunc;
} DISPATCH_COMMAND, *PDISPATCH_COMMAND;

/*
 * Internal Prototypes 
 */
void CpuidTopology_DispatchTaskCommand(unsigned int NumberOfParameters, char **Parameters);
void CpuidTopology_DispatchCommand(unsigned int NumberOfParameters, char **Parameters);
void CpuidTopology_DispatchReadFile(unsigned int NumberOfParameters, char **Parameters);
void CpuidTopology_DispatchWriteFile(unsigned int NumberOfParameters, char **Parameters);
void CpuidTopology_InitGlobal(void);
void CpuidTopology_AllTopologyFromCpuid(void);



/*
 * Global to contain the dispatch function to command line input.
 */
DISPATCH_COMMAND g_DispatchCommand[4] = {
    {'s', CpuidTopology_DispatchWriteFile   },
    {'l', CpuidTopology_DispatchReadFile    },
    {'c', CpuidTopology_DispatchTaskCommand },
    {0,   NULL }
};


/*
 * main
 *
 * Entry Point
 *
 * Arguments:
 *     argc - Number of Arguements
 *     argv - Arguements 
 *     
 * Return:
 *     Zero
 */
int main(int argc, char **argv)
{
    if (argc < 2)
    {
        Display_DisplayParameters();
    }
    else
    {
        CpuidTopology_InitGlobal();
        CpuidTopology_DispatchCommand(argc - 1, &argv[1]);
    }

    return 0;
}



/*
 * CpuidTopology_DispatchCommand
 *
 * Dispatch the command line input to the function handler.
 *
 * Arguments:
 *     Number of Parameters, Paramter List
 *     
 * Return:
 *     None
 */
void CpuidTopology_DispatchCommand(unsigned int NumberOfParameters, char **Parameters)
{
    unsigned int Index;
    BOOL_TYPE CommandDispatched;

    CommandDispatched = BOOL_FALSE;
    Index = 0;

    while(CommandDispatched == BOOL_FALSE && g_DispatchCommand[Index].LowerCase != 0) 
    {
        if (g_DispatchCommand[Index].LowerCase == (*Parameters[0] | ((char)0x20))) 
        {
            g_DispatchCommand[Index].pfnDispatchFunc(NumberOfParameters - 1, Parameters + 1);
            CommandDispatched = BOOL_TRUE;
        }
        else
        {
            Index++;
        }
    }

    if (CommandDispatched == BOOL_FALSE) 
    {
        Display_DisplayParameters();
    }
}


/*
 * CpuidTopology_DispatchReadFile
 *
 * Command line handler to dispatch a Load CPUID from file request.
 *
 * Arguments:
 *     Number of Parameters, Paramter List
 *     
 * Return:
 *     None
 */
void CpuidTopology_DispatchReadFile(unsigned int NumberOfParameters, char **Parameters)
{
    if(NumberOfParameters >= 2)
    {
        if(File_ReadCpuidFromFile(Parameters[0]) == BOOL_TRUE)
        {
            printf("CPUID loaded from %s\n", Parameters[0]);
            CpuidTopology_DispatchTaskCommand(NumberOfParameters - 1, Parameters + 1);
        }
        else
        {
            printf("Failed to load CPUID to %s\n\n", Parameters[0]);
            Display_DisplayParameters();
        }

    }
    else
    {
        printf("No file name to load CPUID or not command to dispatch afterwards.\n\n");
        Display_DisplayParameters();
    }
}




/*
 * CpuidTopology_DispatchWriteFile
 *
 * Command line handler to dispatch a request to write CPUID on this machine to a file.
 *
 * Arguments:
 *     Number of Parameters, Paramter List
 *     
 * Return:
 *     None
 */
void CpuidTopology_DispatchWriteFile(unsigned int NumberOfParameters, char **Parameters)
{

    if(NumberOfParameters >= 1)
    {
        if(File_WriteCpuidToFile(Parameters[0]) != BOOL_FALSE)
        {
            printf("CPUID saved to %s\n", Parameters[0]);
        }
        else
        {
            printf("Failed to save CPUID to %s\n\n", Parameters[0]);
            Display_DisplayParameters();
        }
    }
    else
    {
        printf("No file name to save CPUID.\n\n");
        Display_DisplayParameters();
    }

}




/*
 * CpuidTopology_DispatchTaskCommand
 *
 * Dispatch a specific action request from the command line.
 *
 * Arguments:
 *     Number of Parameters, Parameter List
 *     
 * Return:
 *     None
 */
void CpuidTopology_DispatchTaskCommand(unsigned int NumberOfParameters, char **Parameters)
{
    /*
     * The Command Reference.
     *
     *   0 - Display the topology via OS APIs (Not valid with File Load since we do not have this data.)
     *   1 - Display the topology via CPUID
     *   2 - Display CPUID Leaf values one processor
     *   3 - Display CPUID Leaf Values all processors
     *   4 - Display APIC ID layout
     *   5 - Display TLB Information
     *   6 - Display Cache Information
     *  
     */

    if (NumberOfParameters > 0) 
    {

        if (*Parameters[0] == '0') 
        {
            if (Tools_IsNative()) 
            {
                printf("The following demonstrates OS-provided topology information to applications.\n");
                printf("It is reccomended for applications to utilize OS APIs where possible rather than direct CPUID manipulation.\n\n");
                Os_DisplayTopology();
            }
            else
            {
                Display_DisplayParameters();
            }
        }
        else
        {

            switch (*Parameters[0])
            {
                case '1':
                     CpuidTopology_AllTopologyFromCpuid();
                     break;

                case '2':
                     Display_DisplayProcessorLeafs(1);
                     break;
            
                case '3':
                     Display_DisplayProcessorLeafs(Tools_GetNumberOfProcessors());
                     break;

                case '4': 
                     ParseCpu_ApicIdTopologyLayout();
                     break;

                case '5':
                     ParseTlb_CpuidTlbExample();
                     break;

                case '6':
                     ParseCache_CpuidCacheExample();
                     break;

                default: 
                     Display_DisplayParameters();
            }
        }
    }
    else
    {
        Display_DisplayParameters();
    }

}




/*
 * CpuidTopology_AllTopologyFromCpuid
 *
 *    Demonstrates displaing all topology from CPUID.
 *
 * Arguments:
 *     None
 *     
 * Return:
 *     None
 */
void CpuidTopology_AllTopologyFromCpuid(void)
{
    CPUID_REGISTERS CpuidRegisters;
    CPUID_REGISTERS CpuidRegistersTopologyLeaf;
    unsigned int FoundTopologyInfo;

    FoundTopologyInfo = 0;


    Tools_ReadCpuid(0, 0, &CpuidRegisters);

    if (CpuidRegisters.x.Register.Eax >= 0x1F)
    {
        Tools_ReadCpuid(0x1F, 0, &CpuidRegistersTopologyLeaf);
        if (CpuidRegistersTopologyLeaf.x.Register.Ebx != 0) 
        {
            /*
             * Two Examples below. 
             *  
             *     1. Software that supports 3 Domains.
             *     2. Software that supports >3 Domains.
             */
            ParseCpu_CpuidThreeDomainExample(0x1F);
            ParseCpu_CpuidManyDomainExample(0x1F);
            FoundTopologyInfo = 1;
        }
    }

    if (CpuidRegisters.x.Register.Eax >= 0xB && FoundTopologyInfo == 0)
    {
        Tools_ReadCpuid(0xB, 0, &CpuidRegistersTopologyLeaf);
        if (CpuidRegistersTopologyLeaf.x.Register.Ebx != 0) 
        {
            /*
             * Both examples for Leaf Bh will yield the same results.  The reason 
             * for this example is to show that regardless of Leaf 1Fh or Leaf Bh, 
             * they can both use the new Leaf 1Fh documented algorithm so the code 
             * can be common. 
             */
            ParseCpu_CpuidThreeDomainExample(0xB);
            ParseCpu_CpuidManyDomainExample(0xB);
            
            FoundTopologyInfo = 1;
        }
    }

    if (FoundTopologyInfo == 0) 
    {
        printf("\nUsing legacy CPUID Methods.\n");
        ParseCpu_CpuidLegacyExample();
    }
    else
    {
        printf("\nNOTE: Attempting the legacy CPUID method on this platform for demonstration.\n");
        printf("This method is superceded by Leaf Bh and Leaf 1Fh\n");
        printf("Large platforms may show incorrect information since X2APIC are 32 bit\n");
        printf("and this legacy method is for 8 bit APIC IDs.\n\n");
        ParseCpu_CpuidLegacyExample();
    }
}









/*
 * CpuidTopology_InitGlobal
 *
 *    Initialize the global structure for this file.
 *
 * Arguments:
 *     None
 *     
 * Return:
 *     None
 */
void CpuidTopology_InitGlobal(void)
{
    g_GlobalData.UseNativeCpuid = BOOL_TRUE;
}



