// Do not tear down renderer state after an unhandled exception. Preserve evidence.
#include "build_version.h"
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#endif
namespace {
#ifdef _WIN32
wchar_t dumpFolder[MAX_PATH]={};
LONG report(EXCEPTION_POINTERS *exception) {
    fprintf(stderr,"Fatal native failure; engine %s; exception 0x%08lx\n",DOOM_ENGINE_VERSION,
        exception?exception->ExceptionRecord->ExceptionCode:0xe06d7363ul);
    void *frames[48];USHORT count=CaptureStackBackTrace(0,48,frames,nullptr);
    auto base=(char*)GetModuleHandleW(nullptr);
    for(USHORT i=0;i<count;++i)fprintf(stderr,"stack RVA 0x%llx\n",(unsigned long long)((char*)frames[i]-base));
    HMODULE module=LoadLibraryExW(L"dbghelp.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    auto write=module?reinterpret_cast<decltype(&MiniDumpWriteDump)>(GetProcAddress(module,"MiniDumpWriteDump")):nullptr;
    if(write) {
        wchar_t path[MAX_PATH];swprintf_s(path,L"%s\\engine-%lu-%llu.dmp",dumpFolder,GetCurrentProcessId(),GetTickCount64());
        HANDLE file=CreateFileW(path,GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file!=INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION info={GetCurrentThreadId(),exception,FALSE};
            BOOL ok=write(GetCurrentProcess(),GetCurrentProcessId(),file,MiniDumpWithIndirectlyReferencedMemory,exception?&info:nullptr,nullptr,nullptr);
            fprintf(stderr,"Crash dump %s: %ls\n",ok?"written":"failed",path);CloseHandle(file);
        } else fprintf(stderr,"Crash dump unavailable: Windows error %lu\n",GetLastError());
    }
    fflush(stderr);return EXCEPTION_EXECUTE_HANDLER;
}
#endif
void terminated() {
    try {auto error=std::current_exception();if(error)std::rethrow_exception(error);}
    catch(const std::exception &e){fprintf(stderr,"Unhandled C++ exception: %s\n",e.what());}
    catch(...){fprintf(stderr,"Unhandled non-standard C++ exception\n");}
#ifdef _WIN32
    report(nullptr);
    if(!GetEnvironmentVariableA("TUIN_CRASH_NO_DIALOG",nullptr,0))
        MessageBoxW(nullptr,L"The engine encountered an unexpected error. See last-game.log and the crash dump for details.",L"Tuin Doom Enhanced",MB_OK|MB_ICONERROR);
    TerminateProcess(GetCurrentProcess(),0xc0000409);
#endif
    fflush(stderr);std::_Exit(EXIT_FAILURE);
}
struct Install {
    Install() {
        setvbuf(stderr,nullptr,_IONBF,0);
        fprintf(stderr,"Engine build %s; native diagnostics enabled.\n",DOOM_ENGINE_VERSION);
#ifdef _WIN32
        DWORD length=GetEnvironmentVariableW(L"TUIN_CRASH_DIR",dumpFolder,MAX_PATH);
        if(!length||length>=MAX_PATH)GetCurrentDirectoryW(MAX_PATH,dumpFolder);
        SetUnhandledExceptionFilter(report);
#endif
        std::set_terminate(terminated);
    }
} install;
}

// An indirect C++ call retains exception handling around the legacy C main loop.
using EngineEntry=void(*)();
extern "C" void I_RunEngine(EngineEntry entry) {
    try {entry();}catch(...) {terminated();}
}

#ifdef DOOM_TEST_DRIVER
#include <stdexcept>
static void testThrow() {throw std::runtime_error("intentional diagnostic regression");}
extern "C" void I_TestNativeCrash() {testThrow();}
#endif
