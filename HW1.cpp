/*
 * Copyright (C) 2007-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*! @file
 *  This is an example of the PIN tool that demonstrates some basic PIN APIs 
 *  and could serve as the starting point for developing your first PIN tool
 */

#include "pin.H"
#include <iostream>
#include <fstream>
#include <iomanip>   
#include <set>
#include <map>

using std::cerr;
using std::endl;
using std::string;
using std::set;
using std::map;
using std::max;
using std::min;

/* ================================================================== */
// Global variables
/* ================================================================== */

UINT64 fastForwardCount = 0;
UINT64 icount = 0;

// ---- Part C ----
UINT64 singleChunkIns = 0;
UINT64 multipleChunkIns = 0;
UINT64 singleChunkData = 0;
UINT64 multipleChunkData = 0;

set<UINT32> instructionAddress;
set<UINT32> dataAddress;

// ---- Part D ----
map<UINT32, UINT64> insLength;
map<UINT32, UINT64> insOp;
map<UINT32, UINT64> readReg;
map<UINT32, UINT64> writeReg;
map<UINT32, UINT64> memOp;
map<UINT32, UINT64> memReadOp;
map<UINT32, UINT64> memWriteOp;
UINT32 maxMemBytes = 0;
UINT64 totalMemBytes = 0;
INT32 maxImm = INT32_MIN;
INT32 minImm = INT32_MAX;
INT32 maxDisp = INT32_MIN;
INT32 minDisp = INT32_MAX;

std::ostream* out = &cerr;

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "", "specify output file name");
KNOB<UINT64> KnobFastForward(KNOB_MODE_WRITEONCE, "pintool", "f", "0", "fast forward in billions of instructions");

/* ===================================================================== */
// Utilities
/* ===================================================================== */

INT32 Usage()
{
    cerr << "PIN tool for CS422 HW1 - Part C and D" << endl;
    cerr << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

void PrintResults(const string &title, const map<UINT32, UINT64> &data)
{
    *out << title << " Results: " << endl;
    for (const auto &entry : data) {
        *out << entry.first << " : " << entry.second << endl;
    }
    *out << endl;
}

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

// Count every instruction (including false predicate) for icount
VOID InsCount()
{
    icount++;
}

// Returns non-zero when we should stop
ADDRINT Terminate(void)
{
    return (icount >= fastForwardCount + (UINT64)1e9);
}

// Returns non-zero when we are in the analysis window
ADDRINT FastForward(void)
{
    return (icount >= fastForwardCount);
}

// ---- Part C ----

VOID ins_footprint(UINT32 startAddr, UINT32 endAddr)
{
    UINT32 nextAddr = startAddr + 32;
    if (endAddr <= startAddr) singleChunkIns++;   // endAddr == startAddr means same chunk
    else if (endAddr < nextAddr) singleChunkIns++;
    else multipleChunkIns++;

    UINT32 addr = startAddr;
    while (addr <= endAddr) {
        instructionAddress.insert(addr);
        addr += 32;
    }
}

VOID data_footprint(ADDRINT memAddr, UINT32 memSize)
{
    UINT32 mAddr = (UINT32)memAddr;
    UINT32 startAddr = (mAddr / 32) * 32;
    UINT32 nextAddr = startAddr + 32;
    UINT32 endAddr = ((mAddr + memSize - 1) / 32) * 32; 

    if (endAddr < nextAddr) singleChunkData++;
    else multipleChunkData++;

    UINT32 addr = startAddr;
    while (addr <= endAddr) {
        dataAddress.insert(addr);
        addr += 32;
    }
}

// ---- Part D ----

VOID ins_distribution(UINT32 insSize, UINT32 opCount, UINT32 insReadReg, UINT32 insWriteReg)
{
    insLength[insSize]++;
    insOp[opCount]++;
    readReg[insReadReg]++;
    writeReg[insWriteReg]++;
}

VOID mem_distribution(UINT32 numRead, UINT32 numWrite, UINT32 totalMemSize)
{
    memOp[numRead + numWrite]++;
    memReadOp[numRead]++;
    memWriteOp[numWrite]++;
    maxMemBytes = max(maxMemBytes, totalMemSize);
    totalMemBytes += totalMemSize;
}

VOID imm_distribution(ADDRINT immValue)
{
    INT32 imm = (INT32)immValue;
    maxImm = max(maxImm, imm);
    minImm = min(minImm, imm);
}

VOID disp_distribution(ADDRINT dispValue)
{
    INT32 disp = (INT32)dispValue;
    maxDisp = max(maxDisp, disp);
    minDisp = min(minDisp, disp);
}

/* ===================================================================== */
// Exit and reporting
/* ===================================================================== */

void MyExitRoutine()
{
    // Compute average memory bytes over instructions with at least one memory operand
    UINT64 memInsCount = 0;
    for (const auto &entry : memOp) {
        if (entry.first > 0) memInsCount += entry.second;
    }
    double avgMemBytes = (memInsCount == 0) ? 0.0 : ((1.0 * totalMemBytes) / memInsCount);

    //Adjust memReadOp[0] and memWriteOp[0] to exclude instructions
    // with no memory operands at all (those were counted in memOp[0])
    // Guard against underflow
    UINT64 noMemOps = memOp.count(0) ? memOp[0] : 0;
    if (memReadOp.count(0) && memReadOp[0] >= noMemOps)
        memReadOp[0] -= noMemOps;
    if (memWriteOp.count(0) && memWriteOp[0] >= noMemOps)
        memWriteOp[0] -= noMemOps;

    *out << "Part C Analysis Results: " << endl;
    *out << "------------------------------------------------------------" << endl;
    *out << "Instruction Footprint Blocks            = " << instructionAddress.size() << endl;
    *out << "Instruction Footprint (in bytes)        = " << 32 * instructionAddress.size() << endl;
    *out << "Data Footprint Blocks                   = " << dataAddress.size() << endl;
    *out << "Data Footprint (in bytes)               = " << 32 * dataAddress.size() << endl;
    *out << "Single Memory Chunk Instruction accesses  = " << singleChunkIns << endl;
    *out << "Multiple Memory Chunk Instruction accesses= " << multipleChunkIns << endl;
    *out << "Single Memory Chunk Data accesses         = " << singleChunkData << endl;
    *out << "Multiple Memory Chunk Data accesses       = " << multipleChunkData << endl;
    *out << "============================================================" << endl;

    *out << "Part D Analysis Results: " << endl;
    *out << "------------------------------------------------------------" << endl;
    PrintResults("Distribution of instruction length", insLength);
    PrintResults("Distribution of number of operands", insOp);
    *out << "Instructions with 2 register read operands  : " << (readReg.count(2)  ? readReg[2]  : 0) << endl;
    *out << "Instructions with 1 register write operand  : " << (writeReg.count(1) ? writeReg[1] : 0) << endl;
    *out << "Instructions with 3 memory operands         : " << (memOp.count(3)    ? memOp[3]    : 0) << endl;
    *out << "Instructions with 1 memory read operand     : " << (memReadOp.count(1)? memReadOp[1]: 0) << endl;
    *out << "Instructions with 2 memory write operands   : " << (memWriteOp.count(2)? memWriteOp[2]: 0) << endl;
    *out << "Max bytes touched by a memory instruction   : " << maxMemBytes << endl;
    *out << "Avg bytes touched by a memory instruction   : " << std::fixed << std::setprecision(6) << avgMemBytes << endl;
    *out << "Max immediate value                         : " << maxImm << endl;
    *out << "Min immediate value                         : " << minImm << endl;
    *out << "Max displacement value                      : " << maxDisp << endl;
    *out << "Min displacement value                      : " << minDisp << endl;

    exit(0);
}

/* ===================================================================== */
// Instrumentation callback
/* ===================================================================== */

VOID Trace(TRACE trace, VOID* v)
{
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        BBL_InsertIfCall(bbl, IPOINT_BEFORE, (AFUNPTR)Terminate, IARG_END);
        BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)MyExitRoutine, IARG_END);

        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
        {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)InsCount, IARG_END);

            // Compute chunk addresses at instrumentation time
            UINT32 insAddr = INS_Address(ins);
            UINT32 insSize = INS_Size(ins);
            UINT32 startAddr = (insAddr / 32) * 32;
            UINT32 endAddr   = ((insAddr + insSize - 1) / 32) * 32;  
            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
            INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)ins_footprint,
                               IARG_UINT32, startAddr,
                               IARG_UINT32, endAddr,
                               IARG_END);

            // ----- Always: Part D - instruction length, operand count, reg read/write -----
            UINT32 opCount    = INS_OperandCount(ins);
            UINT32 insReadReg = INS_MaxNumRRegs(ins);
            UINT32 insWriteReg= INS_MaxNumWRegs(ins);

            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
            INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)ins_distribution,
                               IARG_UINT32, insSize,
                               IARG_UINT32, opCount,
                               IARG_UINT32, insReadReg,
                               IARG_UINT32, insWriteReg,
                               IARG_END);

            // ----- Always: immediate distribution (ALL instructions, no predicate) -----
            for (UINT32 opIdx = 0; opIdx < opCount; opIdx++) {
                if (INS_OperandIsImmediate(ins, opIdx)) {
                    ADDRINT immValue = (ADDRINT)(INT32)INS_OperandImmediate(ins, opIdx);
                    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                    INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)imm_distribution,
                                      IARG_ADDRINT, immValue, IARG_END);
                }
            }

            // ----- Predicated: memory operands (data footprint + mem distribution) -----
            UINT32 memOperands = INS_MemoryOperandCount(ins);

            // We must call mem_distribution once per instruction with correct totals.
            // Since these are static properties of the instruction, computing here.
            UINT32 numRead = 0, numWrite = 0, totalMemSize = 0;
            for (UINT32 memOper = 0; memOper < memOperands; memOper++) {
                UINT32 memSize = INS_MemoryOperandSize(ins, memOper);
                totalMemSize += memSize;

                if (INS_MemoryOperandIsRead(ins, memOper))  numRead++;
                if (INS_MemoryOperandIsWritten(ins, memOper)) numWrite++;

                // Data footprint: one call per memory operand (needs runtime EA)
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)data_footprint,
                                            IARG_MEMORYOP_EA, memOper,
                                            IARG_UINT32, memSize,
                                            IARG_END);

                // Displacement distribution (predicated, per memory operand)
                if (INS_OperandIsMemory(ins, memOper)) {
                    ADDRDELTA displacement = INS_OperandMemoryDisplacement(ins, memOper);
                    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                    INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)disp_distribution,
                                                IARG_ADDRINT, (ADDRINT)displacement, IARG_END);
                }
            }

            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
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

    string fileName = KnobOutputFile.Value();
    fastForwardCount = KnobFastForward.Value() * (UINT64)1e9;

    if (!fileName.empty()) {
        out = new std::ofstream(fileName.c_str());
    }

    TRACE_AddInstrumentFunction(Trace, 0);
    PIN_AddFiniFunction(Fini, 0);

    cerr << "===============================================" << endl;
    cerr << "CS422 HW1 PIN Tool - Part C & D" << endl;
    cerr << "Fast forward: " << KnobFastForward.Value() << " billion instructions" << endl;
    if (!fileName.empty())
        cerr << "Output file: " << fileName << endl;
    cerr << "===============================================" << endl;

    PIN_StartProgram();
    return 0;
}
