// Built by Cristei Gabriel-Marian (cristeigabriel)
// (cristei <dot> g772 <at> gmail <dot> com)
// 
// ~TeamSCALEFORM~

#include <thread>
#include <filesystem>
#include "init.hpp"

static void run_init()
{
    LOG("https://github.com/sanyaleks2012-lab/ScaleformReborn\n"
        "~~~~~~~~~~~~\n",
        std::filesystem::current_path().string().c_str());

    ::init();
}

#ifdef WIN32
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    if (reason != DLL_PROCESS_ATTACH)
        return FALSE;
    (void)(reserved);

    // Console is disabled to avoid showing a window.
    // If you need debug output, use DebugView or enable console manually.
    // AllocConsole();
    // freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);

    run_init();
    return TRUE;
}
#else
static void __attribute__((constructor)) attach()
{
    std::thread _{ run_init };
    _.detach();
}
#endif