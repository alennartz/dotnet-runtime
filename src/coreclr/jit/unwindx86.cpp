// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

/*XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XX                                                                           XX
XX                              UnwindInfo                                   XX
XX                                                                           XX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
*/

#include "jitpch.h"
#ifdef _MSC_VER
#pragma hdrstop
#endif

#ifndef TARGET_X86
#error "This should be included only for x86"
#endif // TARGET_X86

#if defined(FEATURE_CFI_SUPPORT)
short Compiler::mapRegNumToDwarfReg(regNumber reg)
{
    short dwarfReg = DWARF_REG_ILLEGAL;

    NYI("CFI codes");

    return dwarfReg;
}
#endif // FEATURE_CFI_SUPPORT

void Compiler::unwindBegProlog()
{
}

void Compiler::unwindEndProlog()
{
}

void Compiler::unwindBegEpilog()
{
}

void Compiler::unwindEndEpilog()
{
}

void Compiler::unwindPush(regNumber reg)
{
}

void Compiler::unwindAllocStack(unsigned size)
{
}

void Compiler::unwindSetFrameReg(regNumber reg, unsigned offset)
{
}

void Compiler::unwindSaveReg(regNumber reg, unsigned offset)
{
}

//------------------------------------------------------------------------
// Compiler::unwindReserve: Ask the VM to reserve space for the unwind information
// for the function and all its funclets. Called once, just before asking the VM
// for memory and emitting the generated code. Calls unwindReserveFunc() to handle
// the main function and each of the funclets, in turn.
//
void Compiler::unwindReserve()
{
#if defined(FEATURE_EH_FUNCLETS)
    assert(!compGeneratingProlog);
    assert(!compGeneratingEpilog);

    assert(compFuncInfoCount > 0);
    for (unsigned funcIdx = 0; funcIdx < compFuncInfoCount; funcIdx++)
    {
        unwindReserveFunc(funGetFunc(funcIdx));
    }
#endif
}

//------------------------------------------------------------------------
// Compiler::unwindEmit: Report all the unwind information to the VM.
//
// Arguments:
//    pHotCode  - Pointer to the beginning of the memory with the function and funclet hot  code.
//    pColdCode - Pointer to the beginning of the memory with the function and funclet cold code.
//
void Compiler::unwindEmit(void* pHotCode, void* pColdCode)
{
#if defined(FEATURE_EH_FUNCLETS)
    assert(!compGeneratingProlog);
    assert(!compGeneratingEpilog);

    assert(compFuncInfoCount > 0);
    for (unsigned funcIdx = 0; funcIdx < compFuncInfoCount; funcIdx++)
    {
        unwindEmitFunc(funGetFunc(funcIdx), pHotCode, pColdCode);
    }
#endif // FEATURE_EH_FUNCLETS
}

#if defined(FEATURE_EH_FUNCLETS)
//------------------------------------------------------------------------
// Compiler::unwindReserveFunc: Reserve the unwind information from the VM for a
// given main function or funclet.
//
// Arguments:
//    func - The main function or funclet to reserve unwind info for.
//
void Compiler::unwindReserveFunc(FuncInfoDsc* func)
{
#ifdef DEBUG
    if (JitConfig.JitFakeProcedureSplitting() && (fgFirstColdBlock != nullptr))
    {
        assert(func->funKind == FUNC_ROOT); // No fake-splitting of funclets.
        unwindReserveFuncHelper(func, true);
    }
    else
#endif // DEBUG
    {
        unwindReserveFuncHelper(func, true);

        if (fgFirstColdBlock != nullptr)
        {
            unwindReserveFuncHelper(func, false);
        }
    }
}

//------------------------------------------------------------------------
// Compiler::unwindReserveFuncHelper: Reserve the unwind information from the VM for a
// given main function or funclet, for either the hot or the cold section.
//
// Arguments:
//    func      - The main function or funclet to reserve unwind info for.
//    isHotCode - 'true' to reserve the hot section, 'false' to reserve the cold section.
//
void Compiler::unwindReserveFuncHelper(FuncInfoDsc* func, bool isHotCode)
{
    bool isFunclet  = (func->funKind != FUNC_ROOT);
    bool isColdCode = !isHotCode;

    eeReserveUnwindInfo(isFunclet, isColdCode, sizeof(UNWIND_INFO));
}

//------------------------------------------------------------------------
// Compiler::unwindEmitFunc: Report the unwind information to the VM for a
// given main function or funclet. Reports the hot section, then the cold
// section if necessary.
//
// Arguments:
//    func      - The main function or funclet to reserve unwind info for.
//    pHotCode  - Pointer to the beginning of the memory with the function and funclet hot  code.
//    pColdCode - Pointer to the beginning of the memory with the function and funclet cold code.
//
void Compiler::unwindEmitFunc(FuncInfoDsc* func, void* pHotCode, void* pColdCode)
{
    // Verify that the JIT enum is in sync with the JIT-EE interface enum
    static_assert_no_msg(FUNC_ROOT == (FuncKind)CORJIT_FUNC_ROOT);
    static_assert_no_msg(FUNC_HANDLER == (FuncKind)CORJIT_FUNC_HANDLER);
    static_assert_no_msg(FUNC_FILTER == (FuncKind)CORJIT_FUNC_FILTER);

#ifdef DEBUG
    if (JitConfig.JitFakeProcedureSplitting() && (pColdCode != nullptr))
    {
        fakeUnwindEmitFuncHelper(func, pHotCode);
    }
    else
#endif // DEBUG
    {
        unwindEmitFuncHelper(func, pHotCode, pColdCode, true);

        if (pColdCode != nullptr)
        {
            unwindEmitFuncHelper(func, pHotCode, pColdCode, false);
        }
    }
}

//------------------------------------------------------------------------
// Compiler::unwindEmitFuncHelper: Report the unwind information to the VM for a
// given main function or funclet, for either the hot or cold section.
//
// Arguments:
//    func      - The main function or funclet to reserve unwind info for.
//    pHotCode  - Pointer to the beginning of the memory with the function and funclet hot  code.
//    pColdCode - Pointer to the beginning of the memory with the function and funclet cold code.
//                Ignored if 'isHotCode' is true.
//    isHotCode - 'true' to report the hot section, 'false' to report the cold section.
//
void Compiler::unwindEmitFuncHelper(FuncInfoDsc* func, void* pHotCode, void* pColdCode, bool isHotCode)
{
    UNATIVE_OFFSET startOffset;
    UNATIVE_OFFSET endOffset;

    if (isHotCode)
    {
        emitLocation* startLoc;
        emitLocation* endLoc;

        unwindGetFuncLocations(func, true, &startLoc, &endLoc);

        if (startLoc == nullptr)
        {
            startOffset = 0;
        }
        else
        {
            startOffset = startLoc->CodeOffset(GetEmitter());
        }

        if (endLoc == nullptr)
        {
            endOffset = info.compNativeCodeSize;
        }
        else
        {
            endOffset = endLoc->CodeOffset(GetEmitter());
        }
    }
    else
    {
        emitLocation* coldStartLoc;
        emitLocation* coldEndLoc;

        assert(fgFirstColdBlock != nullptr);
        assert(func->funKind == FUNC_ROOT); // No splitting of funclets.

        unwindGetFuncLocations(func, false, &coldStartLoc, &coldEndLoc);

        if (coldStartLoc == nullptr)
        {
            startOffset = 0;
        }
        else
        {
            startOffset = coldStartLoc->CodeOffset(GetEmitter());
        }

        if (coldEndLoc == nullptr)
        {
            endOffset = info.compNativeCodeSize;
        }
        else
        {
            endOffset = coldEndLoc->CodeOffset(GetEmitter());
        }
    }

    // Adjust for cold or hot code:
    // 1. The VM doesn't want the cold code pointer unless this is cold code.
    // 2. The startOffset and endOffset need to be from the base of the hot section for hot code
    //    and from the base of the cold section for cold code

    if (isHotCode)
    {
        assert(endOffset <= info.compTotalHotCodeSize);
        pColdCode = nullptr;
    }
    else
    {
        assert(startOffset >= info.compTotalHotCodeSize);
        startOffset -= info.compTotalHotCodeSize;
        endOffset -= info.compTotalHotCodeSize;
    }

    UNWIND_INFO unwindInfo;

    unwindInfo.FunctionLength = (ULONG)(endOffset - startOffset);

    eeAllocUnwindInfo((BYTE*)pHotCode, (BYTE*)pColdCode, startOffset, endOffset, sizeof(UNWIND_INFO),
                      (BYTE*)&unwindInfo, (CorJitFuncKind)func->funKind);
}

#ifdef DEBUG
void Compiler::fakeUnwindEmitFuncHelper(FuncInfoDsc* func, void* pHotCode)
{
    assert(fgFirstColdBlock != nullptr);
    assert(func->funKind == FUNC_ROOT); // No fake-splitting of funclets.

    const UNATIVE_OFFSET startOffset = 0;
    const UNATIVE_OFFSET endOffset   = info.compNativeCodeSize;

    UNWIND_INFO unwindInfo;
    unwindInfo.FunctionLength = (ULONG)(endOffset);

    // Pass pColdCode = nullptr; VM allocs unwind info for combined hot/cold section
    eeAllocUnwindInfo((BYTE*)pHotCode, nullptr, startOffset, endOffset, sizeof(UNWIND_INFO), (BYTE*)&unwindInfo,
                      (CorJitFuncKind)func->funKind);
}
#endif // DEBUG

#endif // FEATURE_EH_FUNCLETS
