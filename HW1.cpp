/*
 * Copyright (C) 2007-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*! @file
 *  CS422 HW1 PIN Tool - Parts A, B, C, and D
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

// ---- Part A ----
typedef struct _InstMetrics
{
    UINT64 numLoads          = 0;
    UINT64 numStores         = 0;
    UINT64 numNops           = 0;
    UINT64 numDirectCalls    = 0;
    UINT64 numIndirectCalls  = 0;
    UINT64 numReturns        = 0;
    UINT64 numUncondBranches = 0;
    UINT64 numCondBranches   = 0;
    UINT64 numLogicalOps     = 0;
    UINT64 numRotateShift    = 0;
    UINT64 numFlagOps        = 0;
    UINT64 numVector         = 0;
    UINT64 numCondMoves      = 0;
    UINT64 numMMXSSE         = 0;
    UINT64 numSysCalls       = 0;
    UINT64 numFP             = 0;
    UINT64 numRest           = 0;
} InstMetrics;

InstMetrics *instMetrics = 0;

// ---- Part C ----
UINT64 singleChunkIns    = 0;
UINT64 multipleChunkIns  = 0;
UINT64 singleChunkData   = 0;
UINT64 multipleChunkData = 0;

unordered_set<UINT64> instructionAddress;
unordered_set<UINT64> dataAddress;

// ---- Part D ----
map<UINT32, UINT64> insLength;
map<UINT32, UINT64> insOp;
map<UINT32, UINT64> readReg;
map<UINT32, UINT64> writeReg;
map<UINT32, UINT64> memOp;
map<UINT32, UINT64> memReadOp;
map<UINT32, UINT64> memWriteOp;
UINT32 maxMemBytes    = 0;
UINT64 totalMemBytes  = 0;
INT32  maxImm         = INT32_MIN;
INT32  minImm         = INT32_MAX;
INT32  maxDisp        = INT32_MIN;
INT32  minDisp        = INT32_MAX;

std::ostream* out = &cerr;

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "",  "specify output file name");
KNOB<UINT64> KnobFastForward(KNOB_MODE_WRITEONCE, "pintool", "f", "0", "fast forward in billions of instructions");

/* ===================================================================== */
// Utilities
/* ===================================================================== */

INT32 Usage()
{
    cerr << "PIN tool for CS422 HW1 - Parts A, B, C, and D" << endl;
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

// ---- Part A: Type A category counter ----
VOID inc_counter(UINT64 *counter)
{
    (*counter)++;
}

// ---- Part A: Load/Store accounting for Type B ----
VOID mem_count(UINT64 *typeACounter, UINT64 numLoads, UINT64 numStores)
{
    (*typeACounter)++;
    instMetrics->numLoads  += numLoads;
    instMetrics->numStores += numStores;
}

// ---- Part C ----
VOID ins_footprint(UINT32 startAddr, UINT32 endAddr)
{
    // Track single vs multiple chunk
    UINT32 nextAddr = startAddr + 32;
    if (endAddr < nextAddr) singleChunkIns++;
    else                    multipleChunkIns++;

    // Insert all touched 32-byte chunk base addresses
    UINT32 addr = startAddr;
    while (addr <= endAddr) {
        instructionAddress.insert(addr);
        addr += 32;
    }
}

VOID data_footprint(ADDRINT memAddr, UINT32 memSize)
{
    UINT32 mAddr     = (UINT32)memAddr;
    UINT32 startAddr = (mAddr / 32) * 32;
    UINT32 nextAddr  = startAddr + 32;
    UINT32 endAddr   = ((mAddr + memSize - 1) / 32) * 32;

    if (endAddr < nextAddr) singleChunkData++;
    else                    multipleChunkData++;

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
    maxMemBytes   = max(maxMemBytes, totalMemSize);
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
    // ---- Part A totals ----
    UINT64 total = 0;
    total += instMetrics->numLoads;
    total += instMetrics->numStores;
    total += instMetrics->numNops;
    total += instMetrics->numDirectCalls;
    total += instMetrics->numIndirectCalls;
    total += instMetrics->numReturns;
    total += instMetrics->numUncondBranches;
    total += instMetrics->numCondBranches;
    total += instMetrics->numLogicalOps;
    total += instMetrics->numRotateShift;
    total += instMetrics->numFlagOps;
    total += instMetrics->numVector;
    total += instMetrics->numCondMoves;
    total += instMetrics->numMMXSSE;
    total += instMetrics->numSysCalls;
    total += instMetrics->numFP;
    total += instMetrics->numRest;

    // ---- Part D: average memory bytes ----
    UINT64 memInsCount = 0;
    for (const auto &entry : memOp) {
        if (entry.first > 0) memInsCount += entry.second;
    }
    double avgMemBytes = (memInsCount == 0) ? 0.0 : ((1.0 * totalMemBytes) / memInsCount);

    // Adjust memReadOp[0] and memWriteOp[0] to exclude instructions with no memory operands
    UINT64 noMemOps = memOp.count(0) ? memOp[0] : 0;
    if (memReadOp.count(0)  && memReadOp[0]  >= noMemOps) memReadOp[0]  -= noMemOps;
    if (memWriteOp.count(0) && memWriteOp[0] >= noMemOps) memWriteOp[0] -= noMemOps;

    // ================================================================
    // Part A Output
    // ================================================================
    *out << "===============================================" << endl;
    *out << "CS422 HW1 PIN Tool - Parts A, B, C & D" << endl;
    *out << "Fast forward: " << KnobFastForward.Value() << " billion instructions" << endl;
    *out << "===============================================" << endl;

    *out << "\n===================== PART A =====================" << endl;
    *out << "Total instruction count (micro-ops): " << total << endl;
    *out << "------------------------------------------------------------" << endl;

    auto printMetric = [&](const string &label, UINT64 val) {
        *out << std::setw(40) << std::left  << label
             << std::setw(12) << std::right << val
             << "  (" << std::fixed << std::setprecision(2)
             << (total ? 100.0 * val / total : 0.0) << "%)" << endl;
    };

    printMetric("Loads:",                    instMetrics->numLoads);
    printMetric("Stores:",                   instMetrics->numStores);
    printMetric("NOPs:",                     instMetrics->numNops);
    printMetric("Direct calls:",             instMetrics->numDirectCalls);
    printMetric("Indirect calls:",           instMetrics->numIndirectCalls);
    printMetric("Returns:",                  instMetrics->numReturns);
    printMetric("Unconditional branches:",   instMetrics->numUncondBranches);
    printMetric("Conditional branches:",     instMetrics->numCondBranches);
    printMetric("Logical operations:",       instMetrics->numLogicalOps);
    printMetric("Rotate and shift:",         instMetrics->numRotateShift);
    printMetric("Flag operations:",          instMetrics->numFlagOps);
    printMetric("Vector instructions:",      instMetrics->numVector);
    printMetric("Conditional moves:",        instMetrics->numCondMoves);
    printMetric("MMX and SSE instructions:", instMetrics->numMMXSSE);
    printMetric("System calls:",             instMetrics->numSysCalls);
    printMetric("Floating-point:",           instMetrics->numFP);
    printMetric("Others:",                   instMetrics->numRest);

    // ================================================================
    // Part B Output
    // ================================================================
    *out << "\n===================== PART B =====================" << endl;
    // Each load/store charged 70 cycles; every other instruction charged 1 cycle
    double cpi = (total == 0) ? 0.0 :
        ((instMetrics->numLoads + instMetrics->numStores) * 70.0
         + (total - instMetrics->numLoads - instMetrics->numStores) * 1.0)
        / total;
    *out << "CPI (loads/stores = 70 cycles, others = 1 cycle): "
         << std::fixed << std::setprecision(6) << cpi << endl;

    // ================================================================
    // Part C Output
    // ================================================================
    *out << "\n===================== PART C =====================" << endl;
    *out << "------------------------------------------------------------" << endl;
    *out << "Instruction Footprint Blocks              = " << instructionAddress.size() << endl;
    *out << "Instruction Footprint (in bytes)          = " << 32 * instructionAddress.size() << endl;
    *out << "Data Footprint Blocks                     = " << dataAddress.size() << endl;
    *out << "Data Footprint (in bytes)                 = " << 32 * dataAddress.size() << endl;
    *out << "Single Memory Chunk Instruction accesses  = " << singleChunkIns << endl;
    *out << "Multiple Memory Chunk Instruction accesses= " << multipleChunkIns << endl;
    *out << "Single Memory Chunk Data accesses         = " << singleChunkData << endl;
    *out << "Multiple Memory Chunk Data accesses       = " << multipleChunkData << endl;

    // ================================================================
    // Part D Output
    // ================================================================
    *out << "\n===================== PART D =====================" << endl;
    *out << "------------------------------------------------------------" << endl;

    *out << "\nD1: Distribution of instruction length (all instructions):" << endl;
    for (const auto &e : insLength)
        *out << "  " << e.first << " bytes : " << e.second << endl;

    *out << "\nD2: Distribution of number of operands (all instructions):" << endl;
    for (const auto &e : insOp)
        *out << "  " << e.first << " operands : " << e.second << endl;

    *out << "\nD3: Instructions with 2 register read operands  : "
         << (readReg.count(2)  ? readReg[2]  : 0) << endl;
    *out << "D4: Instructions with 1 register write operand  : "
         << (writeReg.count(1) ? writeReg[1] : 0) << endl;
    *out << "D5: Instructions with 3 memory operands         : "
         << (memOp.count(3)    ? memOp[3]    : 0) << endl;
    *out << "D6: Instructions with 1 memory read operand     : "
         << (memReadOp.count(1) ? memReadOp[1]: 0) << endl;
    *out << "D7: Instructions with 2 memory write operands   : "
         << (memWriteOp.count(2)? memWriteOp[2]: 0) << endl;
    *out << "D8: Max bytes touched by a memory instruction   : " << maxMemBytes << endl;
    *out << "D8: Avg bytes touched by a memory instruction   : "
         << std::fixed << std::setprecision(6) << avgMemBytes << endl;
    *out << "D9: Max immediate value                         : " << maxImm << endl;
    *out << "D9: Min immediate value                         : " << minImm << endl;
    *out << "D10: Max displacement value                     : " << maxDisp << endl;
    *out << "D10: Min displacement value                     : " << minDisp << endl;

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

            // Identify Type A category (order per assignment spec)
            UINT32 cat = INS_Category(ins);
            if (cat == XED_CATEGORY_NOP) {
                typeACounter = &instMetrics->numNops;
            }
            else if (cat == XED_CATEGORY_CALL) {
                if (INS_IsDirectCall(ins))
                    typeACounter = &instMetrics->numDirectCalls;
                else
                    typeACounter = &instMetrics->numIndirectCalls;
            }
            else if (cat == XED_CATEGORY_RET) {
                typeACounter = &instMetrics->numReturns;
            }
            else if (cat == XED_CATEGORY_UNCOND_BR) {
                typeACounter = &instMetrics->numUncondBranches;
            }
            else if (cat == XED_CATEGORY_COND_BR) {
                typeACounter = &instMetrics->numCondBranches;
            }
            else if (cat == XED_CATEGORY_LOGICAL) {
                typeACounter = &instMetrics->numLogicalOps;
            }
            else if (cat == XED_CATEGORY_ROTATE || cat == XED_CATEGORY_SHIFT) {
                typeACounter = &instMetrics->numRotateShift;
            }
            else if (cat == XED_CATEGORY_FLAGOP) {
                typeACounter = &instMetrics->numFlagOps;
            }
            else if (cat == XED_CATEGORY_AVX    || cat == XED_CATEGORY_AVX2 ||
                     cat == XED_CATEGORY_AVX2GATHER || cat == XED_CATEGORY_AVX512) {
                typeACounter = &instMetrics->numVector;
            }
            else if (cat == XED_CATEGORY_CMOV) {
                typeACounter = &instMetrics->numCondMoves;
            }
            else if (cat == XED_CATEGORY_MMX || cat == XED_CATEGORY_SSE) {
                typeACounter = &instMetrics->numMMXSSE;
            }
            else if (cat == XED_CATEGORY_SYSCALL) {
                typeACounter = &instMetrics->numSysCalls;
            }
            else if (cat == XED_CATEGORY_X87_ALU) {
                typeACounter = &instMetrics->numFP;
            }
            // else: numRest (already set above)

            // ----------------------------------------------------------------
            // Part C - Instruction footprint (all instructions, no predicate)
            // ----------------------------------------------------------------
            INS_InsertIfCall(ins,   IPOINT_BEFORE, (AFUNPTR)FastForward,    IARG_END);
            INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)ins_footprint,
                               IARG_UINT32, startAddr,
                               IARG_UINT32, endAddr,
                               IARG_END);

            // ----------------------------------------------------------------
            // Part D - Instruction length, operand count, reg read/write
            //          (all instructions, no predicate)
            // ----------------------------------------------------------------
            INS_InsertIfCall(ins,   IPOINT_BEFORE, (AFUNPTR)FastForward,      IARG_END);
            INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)ins_distribution,
                               IARG_UINT32, insSize,
                               IARG_UINT32, opCount,
                               IARG_UINT32, insReadReg,
                               IARG_UINT32, insWriteReg,
                               IARG_END);

            // ----------------------------------------------------------------
            // Part D - Immediate distribution (all instructions, no predicate)
            // ----------------------------------------------------------------
            for (UINT32 opIdx = 0; opIdx < opCount; opIdx++) {
                if (INS_OperandIsImmediate(ins, opIdx)) {
                    ADDRINT immValue = (ADDRINT)(INT32)INS_OperandImmediate(ins, opIdx);
                    INS_InsertIfCall(ins,   IPOINT_BEFORE, (AFUNPTR)FastForward,       IARG_END);
                    INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)imm_distribution,
                                      IARG_ADDRINT, immValue, IARG_END);
                }
            }

            // ----------------------------------------------------------------
            // Parts A & B - Predicated: count type A + loads/stores
            // ----------------------------------------------------------------
            INS_InsertIfCall(ins,             IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
            INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)mem_count,
                                         IARG_PTR,    typeACounter,
                                         IARG_UINT64, numLoads,
                                         IARG_UINT64, numStores,
                                         IARG_END);

            // ----------------------------------------------------------------
            // Parts C & D - Predicated: data footprint + mem distribution + displacement
            // ----------------------------------------------------------------
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
    fastForwardCount  = KnobFastForward.Value() * (UINT64)1e9;

    if (!fileName.empty()) {
        out = new std::ofstream(fileName.c_str());
    }

    instMetrics = new InstMetrics();

    TRACE_AddInstrumentFunction(Trace, 0);
    PIN_AddFiniFunction(Fini, 0);

    cerr << "===============================================" << endl;
    cerr << "CS422 HW1 PIN Tool - Parts A, B, C & D"        << endl;
    cerr << "Fast forward: " << KnobFastForward.Value()
         << " billion instructions"                          << endl;
    if (!fileName.empty())
        cerr << "Output file: " << fileName << endl;
    cerr << "===============================================" << endl;

    PIN_StartProgram();
    return 0;
}
