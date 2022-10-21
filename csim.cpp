#include "cache.h"
#include "pin.H"
#include <cstdio>
#include <iostream>

#define IGNORE_STACK 0

// size_t stackAccesses{0};
// size_t num_threads{0};
Cache g_cache;

// size: #bytes accessed
static void mem_access(ADDRINT addr, UINT32 size)
{
    // calculate memory block (cacheline) of low address
    Addr line_addr1 = (Addr)(addr >> g_cache.line_bits_);

    // calculate memory block (cacheline) of high address
    Addr line_addr2 = (Addr)((addr + size - 1) >> g_cache.line_bits_);

    // single memory block accessed
    if (line_addr1 == line_addr2) {
        g_cache.on_access(line_addr1);
    }
    // memory access spans across two memory blocks (cache line cross)
    else {
        g_cache.on_access(line_addr1);
        g_cache.on_access(line_addr2);
    }

    return;
}

static /* PIN_FAST_ANALYSIS_CALL */ VOID mem_read(THREADID tid, ADDRINT addr, UINT32 size)
{
    if (tid > 0) {
        return;
    }
    mem_access(addr, size);
}

static /* PIN_FAST_ANALYSIS_CALL */ VOID mem_write(THREADID tid, ADDRINT addr, UINT32 size)
{
    if (tid > 0) {
        return;
    }

    mem_access(addr, size);
}

// [[maybe_unused]] static /* PIN_FAST_ANALYSIS_CALL */ VOID stackAccess() { stackAccesses++; }

/* ===================================================================== */
/* Instrumentation                                                       */
/* ===================================================================== */

VOID Instruction(INS ins, VOID *v)
{
    if (IGNORE_STACK && (INS_IsStackRead(ins) || INS_IsStackWrite(ins))) {
        // INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)stackAccess, IARG_END);
        return;
    }

    // IARG_FAST_ANALYSIS_CALL
    UINT32 memOperands = INS_MemoryOperandCount(ins);

    for (UINT32 memOp = 0; memOp < memOperands; memOp++) {
        if (INS_MemoryOperandIsRead(ins, memOp))
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)mem_read, /*IARG_FAST_ANALYSIS_CALL*/ IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp,
                                     IARG_UINT32, INS_MemoryOperandSize(ins, memOp), IARG_END);

        if (INS_MemoryOperandIsWritten(ins, memOp))
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)mem_write, /*IARG_FAST_ANALYSIS_CALL*/ IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp,
                                     IARG_UINT32, INS_MemoryOperandSize(ins, memOp), IARG_END);
    }
}

/* ===================================================================== */
/* Callbacks from Pin                                                    */
/* ===================================================================== */

VOID ThreadStart(THREADID tid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
    fprintf(stderr, "Thread %d started\n", tid);
}

VOID ThreadFini(THREADID tid, const CONTEXT *ctxt, INT32 flags, VOID *v)
{
    fprintf(stderr, "Thread %d finished\n", tid);
}

/* ===================================================================== */
/* Output results at exit                                                */
/* ===================================================================== */

VOID Exit(INT32 code, VOID *v)
{
    std::cout << "misses: " << g_cache.miss() << " hits: " << g_cache.hits() << '\n';
}

/* ===================================================================== */
/* Usage/Main Function of the Pin Tool                                   */
/* ===================================================================== */

INT32 Usage()
{
    PIN_ERROR("CSim:\n" + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

int main(int argc, char *argv[])
{
    if (PIN_Init(argc, argv)) {
        return Usage();
    }

    PIN_InitSymbols();
    INS_AddInstrumentFunction(Instruction, 0);

    PIN_AddFiniFunction(Exit, 0);
    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddThreadFiniFunction(ThreadFini, 0);

    const size_t line_size = 64U;
    const size_t cache_size = 32768U;
    const size_t cache_assoc = 8U;

    g_cache = Cache{line_size, cache_size, cache_assoc};

    PIN_StartProgram();
    return 0;
}
