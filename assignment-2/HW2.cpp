/*
 * Copyright (C) 2007-2026 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*! 
 *  CS422 HW2 PIN Tool 
 *  Krishna Agrawal 230574 Group 2 
 */

#include "pin.H"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <set>
#include <map>
#include <unordered_set>

using std::cerr;
using std::endl;
using std::string;
using std::set;
using std::map;
using std::unordered_set;
using std::max;
using std::min;

/* ================================================================== */
// Global variables
/* ================================================================== */

UINT64 fastForwardCount = 0;
UINT64 icount = 0;
UINT64 key = 0;

std::ostream* out = &cerr;

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "",  "specify output file name");

/* ===================================================================== */
// Utilities
/* ===================================================================== */

INT32 Usage()
{
    cerr << "PIN tool for CS422 Assignment 2 - Group 2 - Krishna Agrawal (230574)" << endl;
    cerr << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

// Count every instruction (including false predicate) for icount
VOID InsCount()
{
    icount++;
}

/* ===================================================================== */
// Exit and reporting
/* ===================================================================== */

void MyExitRoutine()
{
    *out << key << endl;

    exit(0);
}

/* ===================================================================== */
// Instrumentation callback
/* ===================================================================== */

VOID Trace(TRACE trace, VOID* v)
{
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        // Terminate check at BBL level (efficient)
        BBL_InsertIfCall(bbl,   IPOINT_BEFORE, (AFUNPTR)Terminate,      IARG_END);
        BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)MyExitRoutine,  IARG_END);

        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
        {
            // Always count every instruction (including false predicate)
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)InsCount, IARG_END);

            // ----------------------------------------------------------------
            // Compute static properties at instrumentation time
            // ----------------------------------------------------------------
            UINT32 insAddr  = INS_Address(ins);
            UINT32 insSize  = INS_Size(ins);
            UINT32 startAddr = (insAddr / 32) * 32;
            UINT32 endAddr   = ((insAddr + insSize - 1) / 32) * 32;

            UINT32 opCount     = INS_OperandCount(ins);
            UINT32 insReadReg  = INS_MaxNumRRegs(ins);
            UINT32 insWriteReg = INS_MaxNumWRegs(ins);
            UINT32 memOperands = INS_MemoryOperandCount(ins);

            // ---- Part A: determine type A counter pointer ----
            UINT64 *typeACounter = &instMetrics->numRest; // default

            // Memory loads/stores for Type B
            UINT64 numLoads  = 0;
            UINT64 numStores = 0;

            if (memOperands > 0) {
                for (UINT32 memOper = 0; memOper < memOperands; memOper++) {
                    UINT32 memSize = INS_MemoryOperandSize(ins, memOper);
                    if (INS_MemoryOperandIsRead(ins, memOper))
                        numLoads  += memSize / 4 + (memSize % 4 != 0);
                    if (INS_MemoryOperandIsWritten(ins, memOper))
                        numStores += memSize / 4 + (memSize % 4 != 0);
                }
            }

            INS_InsertIfCall(ins,   IPOINT_BEFORE, (AFUNPTR)FastForward,    IARG_END);
            INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)ins_footprint,
                               IARG_UINT32, startAddr,
                               IARG_UINT32, endAddr,
                               IARG_END);

            INS_InsertIfCall(ins,   IPOINT_BEFORE, (AFUNPTR)FastForward,      IARG_END);
            INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)ins_distribution,
                               IARG_UINT32, insSize,
                               IARG_UINT32, opCount,
                               IARG_UINT32, insReadReg,
                               IARG_UINT32, insWriteReg,
                               IARG_END);

            for (UINT32 opIdx = 0; opIdx < opCount; opIdx++) {
                if (INS_OperandIsImmediate(ins, opIdx)) {
                    ADDRINT immValue = (ADDRINT)(INT32)INS_OperandImmediate(ins, opIdx);
                    INS_InsertIfCall(ins,   IPOINT_BEFORE, (AFUNPTR)FastForward,       IARG_END);
                    INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)imm_distribution,
                                      IARG_ADDRINT, immValue, IARG_END);
                }
            }

            INS_InsertIfCall(ins,             IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
            INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)mem_count,
                                         IARG_PTR,    typeACounter,
                                         IARG_UINT64, numLoads,
                                         IARG_UINT64, numStores,
                                         IARG_END);

            UINT32 numRead = 0, numWrite = 0, totalMemSize = 0;
            for (UINT32 memOper = 0; memOper < memOperands; memOper++) {
                UINT32 memSize = INS_MemoryOperandSize(ins, memOper);
                totalMemSize += memSize;
                if (INS_MemoryOperandIsRead(ins, memOper))    numRead++;
                if (INS_MemoryOperandIsWritten(ins, memOper)) numWrite++;

                // Data footprint (runtime EA needed)
                INS_InsertIfCall(ins,             IPOINT_BEFORE, (AFUNPTR)FastForward,    IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)data_footprint,
                                             IARG_MEMORYOP_EA, memOper,
                                             IARG_UINT32, memSize,
                                             IARG_END);

                // Displacement distribution
                if (INS_OperandIsMemory(ins, memOper)) {
                    ADDRDELTA displacement = INS_OperandMemoryDisplacement(ins, memOper);
                    INS_InsertIfCall(ins,             IPOINT_BEFORE, (AFUNPTR)FastForward,        IARG_END);
                    INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)disp_distribution,
                                                 IARG_ADDRINT, (ADDRINT)displacement, IARG_END);
                }
            }

            INS_InsertIfCall(ins,             IPOINT_BEFORE, (AFUNPTR)FastForward,      IARG_END);
            INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)mem_distribution,
                                         IARG_UINT32, numRead,
                                         IARG_UINT32, numWrite,
                                         IARG_UINT32, totalMemSize,
                                         IARG_END);
        }
    }
}

/* ===================================================================== */
// Fini (fallback if application exits naturally)
/* ===================================================================== */

VOID Fini(INT32 code, VOID* v)
{
    MyExitRoutine();
}

/* ===================================================================== */
// main
/* ===================================================================== */

int main(int argc, char* argv[])
{
    if (PIN_Init(argc, argv)) return Usage();

    string fileName   = KnobOutputFile.Value();

    if (!fileName.empty()) {
        out = new std::ofstream(fileName.c_str());
    }

    instMetrics = new InstMetrics();

    TRACE_AddInstrumentFunction(Trace, 0);
    PIN_AddFiniFunction(Fini, 0);

    cerr << "===============================================" << endl;
    cerr << "CS422 Assignment 2 PIN Tool - Group 2 - Krishna Agrawal (230574)" << endl;
    if (!fileName.empty())
        cerr << "Output file: " << fileName << endl;
    cerr << "===============================================" << endl;

    PIN_StartProgram();
    return 0;
}
