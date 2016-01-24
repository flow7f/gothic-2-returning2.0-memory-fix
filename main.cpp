/*
  Gothic Memory Fix

  @author: lviper
  
  @license: GNU GPL 2.0
    (brief: you free to distribute or change code and dll, but if you publish
     changed version you also must make your code open under GPL 2.0 license).

  @version: 06

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
           lviper, Moscow, 2016/01/16
*/

//#define MEM_USE_LIBHOARD
//#define MEM_USE_JEMALLOC
//#define MEM_USE_CRT
#define MEM_USE_TCMALLOC

#ifdef MEM_USE_LIBHOARD
#pragma comment(lib, "libhoard.lib")
#elif defined(MEM_USE_JEMALLOC)
#pragma comment(lib, "libjemalloc_x86_Release-Static.lib" )
#elif defined(MEM_USE_TCMALLOC)
#pragma comment(lib, "libtcmalloc_minimal.lib")
#endif

#define MEMCALLTYPE __cdecl


#define NOMINMAX
#include <Windows.h>
#include <stdlib.h>

#ifdef MEM_USE_JEMALLOC
#define USE_STATIC
#define STATIC_ENABLE
#include <jemalloc/jemalloc.h>

#define shw_malloc je_malloc
#define shw_free je_free
#define shw_calloc je_calloc
#define shw_realloc je_realloc
#define shw_msize je_malloc_usable_size
#define shw_init je_init()
#define shw_uninit je_uninit()
#elif defined(MEM_USE_TCMALLOC)
#include <gperftools\tcmalloc.h>
#define KEEP_ALLOCATED_SIZES

#define shw_malloc tc_malloc
#define shw_free tc_free
#define shw_calloc tc_calloc
#define shw_realloc tc_realloc
#define shw_msize tc_malloc_size		// >= required size by malloc, so better KEEP_ALLOCATED_SIZES
#define shw_init
#define shw_uninit
#else
#define shw_malloc malloc
#define shw_free free
#define shw_calloc calloc
#define shw_realloc realloc
#define shw_msize _msize
#define shw_init
#define shw_uninits
#endif




#include <new>
#include <new.h>
#include <algorithm>

#include <unordered_map>
#include <unordered_set>

extern "C" {
	size_t MEMCALLTYPE shi_msize(void *data);
	BOOL MEMCALLTYPE shi_MemInitDefaultPool();
	void* MEMCALLTYPE shi_malloc(size_t size);
	void* MEMCALLTYPE shi_calloc(size_t num, size_t size);
	void MEMCALLTYPE shi_delete(void *data);
	void* MEMCALLTYPE shi_realloc(void* data, size_t size);
	void MEMCALLTYPE shi_free(void *data);
}

#ifdef KEEP_ALLOCATED_SIZES
std::unordered_map<void*, size_t> allocatedWithSizes;
#define allocatedContainer allocatedWithSizes
#else
std::unordered_set<void*> allocated;
#define allocatedContainer allocated
#endif


// sometimes OutOfMemory handler's exit and fatal will ignore interruption or exit. Then just use this flag to correct OutOfMemory again in the next call shi_* function.
bool isOutOfMemoryAlreadyHappens = false;

namespace Settings {
	bool showGothicErrorMessage = true;	// show Gothic error message
	bool showMessageBox = false;		// show MessageBox when OutOfMemory happens
	bool useNewHandler = true;			// check OutOfMemory in new_handler (always exit) or in regular function (where will be abort, retry and skip options)

	const char *iniFile = ".\\SystemPack.ini";
	const char *iniAppName = "SHW32";

	bool LoadIntFromIni(const char *varName, int &value) {
		int notExistValue = -12345;
		int currentValue = GetPrivateProfileIntA(iniAppName, varName, notExistValue, iniFile);
		if (currentValue == notExistValue)
			return false;

		value = currentValue;
		return true;
	}

	void SaveIntToIni(const char *varName, int value) {
		char buffer[64];
		_itoa_s(value, buffer, 10);
		WritePrivateProfileStringA(iniAppName, varName, buffer, iniFile);
	}

	template< typename T >
	void LoadValueFromIniAndSaveIfValueDontExists(const char *varName, T &value) {
		int intValue = static_cast<int>(value);
		if (!LoadIntFromIni(varName, intValue))
			SaveIntToIni(varName, intValue);
		value = static_cast<T>(intValue);
	}

	void LoadFromIni() {
		LoadValueFromIniAndSaveIfValueDontExists("bShowGothicError", showGothicErrorMessage);
		LoadValueFromIniAndSaveIfValueDontExists("bShowMsgBox", showMessageBox);
		LoadValueFromIniAndSaveIfValueDontExists("bUseNewHandler", useNewHandler);
	}
};

// Emergency buffer for free after OutOfMemory to correct work system functions
class EmergencyBufferForOutOfMemory {
public:
	void FreeMessageBoxBuffer() {
		shi_free(bufferForMessageBox);
		bufferForMessageBox = nullptr;
	}
	void FreeGothicBuffer() {
		shi_free(bufferForGothicError);
		bufferForGothicError = nullptr;
	}
												
												
	// call after Settings::Load
	void Initialize() {
		size_t sizeForGothicError = 50;
		Settings::LoadValueFromIniAndSaveIfValueDontExists("reserveInMb", sizeForGothicError);
		sizeForGothicError = std::max(minSize, std::min(maxSize, sizeForGothicError));
												
		size_t sizeForMessageBox = 0;
		if (!Settings::showGothicErrorMessage && !Settings::showMessageBox)
			sizeForGothicError = 5;
		else if (Settings::showMessageBox) {
			if (Settings::showGothicErrorMessage) {
				if (sizeForGothicError >= 100)
					sizeForMessageBox = 30;
				else if (sizeForGothicError >= 60)
					sizeForMessageBox = 10;
				else if (sizeForGothicError >= 10)
					sizeForMessageBox = 5;
			}
			else {
				sizeForMessageBox = sizeForGothicError;
			}
			sizeForGothicError -= sizeForMessageBox;
		}
												
		constexpr size_t Mb = 1024 * 1024;
		if (sizeForGothicError > 0)
			bufferForGothicError = shi_malloc(sizeForGothicError*Mb);
		if (sizeForMessageBox > 0)
			bufferForMessageBox = shi_malloc(sizeForMessageBox*Mb);
	}
												
private:
	constexpr static size_t minSize = 5;
	constexpr static size_t maxSize = 300;
												
	void *bufferForGothicError = nullptr;
	void *bufferForMessageBox = nullptr;
};
EmergencyBufferForOutOfMemory emergencyBuffer;


void TerminateProgram() {
	isOutOfMemoryAlreadyHappens = true;
	emergencyBuffer.FreeGothicBuffer();

	if (Settings::showGothicErrorMessage) {
		static bool isInterrupted = false;
		if (!isInterrupted) {
			isInterrupted = true;
			__asm int 3h; breakpoint
		}
	}
	else {
		std::exit(1);
	}
}

void OutOfMemoryHandler() {
	emergencyBuffer.FreeMessageBoxBuffer();
	if (!isOutOfMemoryAlreadyHappens && Settings::showMessageBox)
		MessageBoxA(nullptr, "OutOfMemory!", nullptr, MB_OK | MB_ICONERROR);

	std::set_new_handler(nullptr);
	_set_new_mode(0);

	TerminateProgram();
}

bool IsNeedReallocate(void *data) {
	bool result = false;

	if (data == nullptr && !isOutOfMemoryAlreadyHappens) {
		emergencyBuffer.FreeMessageBoxBuffer();
		int chose = MessageBoxA(nullptr, "OutOfMemory!", nullptr, MB_ABORTRETRYIGNORE | MB_ICONERROR);
		if (chose == IDABORT) {
			TerminateProgram();
		}
		else if (chose == IDRETRY) {
			result = true;
		}
	}

	return result;
}

void AddAllocated(void *data, size_t size) {
	if (!data)
		return;
#ifdef KEEP_ALLOCATED_SIZES
	allocatedWithSizes[data] = size;
#else
	allocated.insert(data);
#endif
}

extern "C" {
	size_t MEMCALLTYPE shi_msize(void *data) {
		auto size = allocatedContainer.find(data);
		if (size != allocatedContainer.end())
#ifdef KEEP_ALLOCATED_SIZES
			return size->second;
#else
			return shw_msize(data);
#endif
		return _msize(data);
	}

	BOOL MEMCALLTYPE shi_MemInitDefaultPool() {
		return TRUE;
	}

	void* MEMCALLTYPE shi_malloc(size_t size) {
		void *result = nullptr;
		do {
			result = shw_malloc(size);
		} while (IsNeedReallocate(result));
		AddAllocated(result, size);
		return result;
	}

	void* MEMCALLTYPE shi_calloc(size_t num, size_t size) {
		void *result = nullptr;
		do {
			result = shw_calloc(num, size);
		} while (IsNeedReallocate(result));
		AddAllocated(result, size);
		return result;
	}

	void MEMCALLTYPE shi_delete(void *data) {
		auto size = allocatedContainer.find(data);
		if (size != allocatedContainer.end()) {
			allocatedContainer.erase(size);
			shw_free(data);
			return;
		}
		free(data);
	}

	void* MEMCALLTYPE shi_realloc(void* data, size_t size) {
		if (size == 0) {
			shi_delete(data);
			return nullptr;
		}

		auto oldSizeIter = allocatedContainer.find(data);
		if (oldSizeIter != allocatedContainer.end()) {
			void *result = nullptr;
			do {
				result = shw_realloc(data, size);
			} while (IsNeedReallocate(result));
			if (result != nullptr) {
				allocatedContainer.erase(oldSizeIter);
				AddAllocated(result, size);
			}
			return result;
		}
		else {
			void *result = nullptr;
			do {
				result = realloc(data, size);
			} while (IsNeedReallocate(result));
			return result;
		}

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


BOOL WINAPI DllMain(HINSTANCE module_handle, DWORD reason_for_call, LPVOID reserved)
{
	if (reason_for_call == DLL_PROCESS_ATTACH) {
		shw_init;

		Settings::LoadFromIni();
		emergencyBuffer.Initialize();

		if (Settings::useNewHandler) {
			std::set_new_handler(&OutOfMemoryHandler);
			_set_new_mode(1);
		}
	}
	else if (reason_for_call == DLL_PROCESS_DETACH) {
		shw_uninit;
	}
	return TRUE;
}
