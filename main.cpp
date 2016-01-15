/*
  Gothic Memory Fix

  @author: lviper
  
  @license: GNU GPL 2.0
    (brief: you free to distribute or change code and dll, but if you publish
     changed version you also must make your code open under GPL 2.0 license).

  @version: 05

================================================================================

  SHW32.DLL is SmartHeap memory allocation library, that use in Gothic, 
    Gothic II and Gothic II: Night of the Raven games.
  
  This proxy dll fix was created for support very large mods and reduce lags 
    and OutOfMemory errors on modern hardware. You can use this fix with any
	what you want mods, or without any. 

  Recommended to use with 4gb_patch on x64 systems
   (http://www.ntcore.com/4gb_patch.php).

================================================================================

  Just compile and replace original Shw32.dll in Game/System directory.

  I compiled this program with Hoard 3.10 library, but you can experiment with 
    your favorite allocation memroy library for best results for yourself.
  If any other game or program use old version of SmartHeap library (shw32.dll) 
    you can try to use this fix, but in this case you must implement all 
    functions, that program uses and maybe change ordinal value at def file 
    or add some new functions if version shw32.dll is differrent.


  Happy gaming and coding :)
           lviper, Moscow, 2016/01/14
*/

#define MEMCALLTYPE __cdecl
#define SET_MALLOC_HANDLER			// check OutOfMemory in new_handler (always exit) or in regular function (where will be abort, retry and skip options)

#define NOMINMAX
#include <Windows.h>
#include <stdlib.h>

#ifdef SET_MALLOC_HANDLER
#include <new>
#include <new.h>
#include <algorithm>


size_t emergencyBUfferSizeInMb = 50;
const size_t minEmergencyBUfferSizeInMb = 5;
const size_t maxEmergencyBUfferSizeInMb = 300;
void *emergencyBuffer = nullptr;				// free in case OutOfMemory for correct work system functions

bool showGothicErrorMessage = true;				// in case out of memory need show gothic error message for callstack
bool showMessageBox = false;					//                                 messagebox with OutOfMemory message

bool isOutOfMemoryAlreadyHappens = false;		// sometimes OutOfMemory handler's exit and fatal will ignore. Then just make OutOfMemory again in the next call shi_ function.
#endif

#pragma comment(lib, "libhoard.lib")

void TerminateProgram() {
	if (showGothicErrorMessage) {
		__asm int 3								// breakpoint for stop at debugger/show fatal gothic messagebox
	} else {
		std::exit(1);
	}
}

#ifdef SET_MALLOC_HANDLER
void OutOfMemoryHandler() {
	if (emergencyBuffer)
		free(emergencyBuffer);
	emergencyBuffer = nullptr;
	if (!isOutOfMemoryAlreadyHappens && showMessageBox)
		MessageBoxA(nullptr, "OutOfMemory!", nullptr, MB_OK | MB_ICONERROR);

	isOutOfMemoryAlreadyHappens = true;
	TerminateProgram();
}
#endif

void ExecuteOutOfMemoryHandlerIfFatalHappens() {
	if (isOutOfMemoryAlreadyHappens)
		malloc(std::numeric_limits<size_t>::max());			// call out of memory handler
}

bool IsNeedReallocate(void *data) {
	bool result = false;

#ifndef SET_MALLOC_HANDLER
	if (data == nullptr) {
		int chose = MessageBoxA(nullptr, "OutOfMemory!", nullptr, MB_ABORTRETRYIGNORE | MB_ICONERROR);
		if (chose == IDABORT) {
			TerminateProgram();
		}
		else if (chose == IDRETRY) {
			result = true;
		}
	}
#endif

	return result;
}

extern "C" {
	size_t MEMCALLTYPE shi_msize(void *data) {
		ExecuteOutOfMemoryHandlerIfFatalHappens();
		return _msize(data);
	}

	BOOL MEMCALLTYPE shi_MemInitDefaultPool() {
		return TRUE;
	}

	void* MEMCALLTYPE shi_malloc(size_t size) {	
		ExecuteOutOfMemoryHandlerIfFatalHappens();

		void *result = nullptr;
		do {
			result = malloc(size);
		} while (IsNeedReallocate(result));
		return result;
	}

	void* MEMCALLTYPE shi_calloc(size_t num, size_t size) {
		ExecuteOutOfMemoryHandlerIfFatalHappens();

		void *result = nullptr;
		do {
			result = calloc(num, size);
		} while (IsNeedReallocate(result));
		return result;
	}

	void MEMCALLTYPE shi_delete(void *data) {
		free(data);
		ExecuteOutOfMemoryHandlerIfFatalHappens();
	}

	void* MEMCALLTYPE shi_realloc(void* data, size_t size) {
		ExecuteOutOfMemoryHandlerIfFatalHappens();

		void *result = nullptr;
		do {
			result = realloc(data, size);
		} while (size != 0 && IsNeedReallocate(result));
		return result;
	}

	void MEMCALLTYPE shi_free(void *data) {
		shi_delete(data);
	}

	// nb: next functions not used, but gothic2.exe failed to start if they don't exist in export table
	//  for regenrate that functions use g2shw_ni.py
#pragma region NOTIMPLEMENTED
	void MEMCALLTYPE notImplemented(const char *msg = "Not Implemented") {
		MessageBoxA(nullptr, msg, "Shw32.dll fix: fatal error", MB_OK | MB_ICONERROR);
		__asm int 3
		exit(1);
	}

	void MEMCALLTYPE notImplemented_00000002_MemVersion() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000003__MemPoolSetGrowIncrement() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000004_MemRegisterTask() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000005__MemPoolSetHighThreads() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000006_MemUnregisterTask() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000007_MemProcessSetLargeBlockThreshold() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000008_MemProcessSetGrowIncrement() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000009_MemProcessSetFreeBytes() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000000A_MemPoolInit() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000000B_MemPoolInitFS() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000000C_MemPoolInitNamedShared() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000000D_MemPoolInitNamedSharedEx() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000000E_MemPoolAttachShared() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000000F_MemPoolFree() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000010_MemPoolInitRegion() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000011_MemPoolInitRegionEx() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000012_MemPoolLock() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000013_MemPoolUnlock() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000014_MemPoolSetPageSize() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000015_MemPoolSetSmallBlockAllocator() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000016_MemPoolSetBlockSizeFS() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000017_MemPoolSetSmallBlockSize() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000018_MemPoolSetFloor() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000019_MemPoolSetCeiling() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000001A_MemPoolSetFreeBytes() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000001B_shi_MemDefaultPool() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000001C_MemPoolPreAllocate() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000001D_MemPoolPreAllocateHandles() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000001E_MemPoolShrink() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000001F_shi_freeingDLL() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000020_shi_loadingDLL() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000021__shi_patchMallocs() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000022_MemPoolSize() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000023_MemPoolCount() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000028_MemPoolInfo() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000002A_MemPoolFirst() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000002B_MemPoolNext() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000002E_MemPoolWalk() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000030_MemPoolCheck() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000032_MemAlloc() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000034_MemFree() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000036_MemReAlloc() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000038_MemLock() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000039_MemUnlock() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000003A_MemFix() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000003B_MemUnfix() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000003C_MemLockCount() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000003E_MemIsMoveable() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000040_MemHandle() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000042_MemSize() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000043_MemSizeRequested() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000046_MemAllocPtr() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000048_MemFreePtr() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000004A_MemReAllocPtr() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000004D_MemSizePtr() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000004F_MemCheckPtr() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000054_MemAllocFS() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000056_MemFreeFS() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000005A_MemDefaultErrorHandler() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000005E_MemSetErrorHandler() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000060_MemErrorUnwind() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_000000FB_shi_new() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_000000FC_shi_expand() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_000000FD_shi_MemFreeDefaultPool() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_000000FE_shi_heapmin() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000100_shi_heapwalk() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000104_shi_heapset() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000106_shi_heapadd() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000107_shi_heapused() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000108_shi_heapchk() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000010A_shi_deleteStdcall() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000010B_shi_deletePtrStdcall() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000010C_shi_set_new_handler() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000010D_shi_newStdcall() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000126_shi_notSMP() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000127_shi_getThreadPool() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_00000128_shi_freeThreadPools() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000012A__shi_enterCriticalSection() {
		notImplemented(__FUNCTION__);
	}
	void MEMCALLTYPE notImplemented_0000012B__shi_leaveCriticalSection() {
		notImplemented(__FUNCTION__);
	}
#pragma endregion
}

int loadIntFromIniAndSaveIfValueDontExists(const char *varName, int defaultValue) {
	const char *iniFile = ".\\SystemPack.ini";
	int currentValue = GetPrivateProfileIntA("SHW32", varName, -12345, iniFile);
	if (currentValue == -12345) {
		// memory don't exists (or incorrect in any case), save defaault value
		char buffer[16];
		_itoa_s(defaultValue, buffer, 10);
		WritePrivateProfileStringA("SHW32", varName, buffer, iniFile);

		return defaultValue;
	}

	return currentValue;
}

void loadAndSaveSettingsFromIni() {
	emergencyBUfferSizeInMb = loadIntFromIniAndSaveIfValueDontExists("reserveInMb", emergencyBUfferSizeInMb);

	showGothicErrorMessage = loadIntFromIniAndSaveIfValueDontExists("bShowGothicError", showGothicErrorMessage);
	showMessageBox = loadIntFromIniAndSaveIfValueDontExists("bShowMsgBox", showMessageBox);

	if (!showGothicErrorMessage && !showMessageBox)
		emergencyBUfferSizeInMb = minEmergencyBUfferSizeInMb;
	else
		emergencyBUfferSizeInMb = std::min(maxEmergencyBUfferSizeInMb, std::max(emergencyBUfferSizeInMb, minEmergencyBUfferSizeInMb));
}

BOOL WINAPI DllMain(HINSTANCE module_handle, DWORD reason_for_call, LPVOID reserved)
{
	if (reason_for_call == DLL_PROCESS_ATTACH) {
#ifdef SET_MALLOC_HANDLER
		loadAndSaveSettingsFromIni();
		if( emergencyBUfferSizeInMb > 0 )
			emergencyBuffer = malloc(emergencyBUfferSizeInMb*1024*1024);

		std::set_new_handler(&OutOfMemoryHandler);
		_set_new_mode(1);
#endif
	}
	return TRUE;
}
