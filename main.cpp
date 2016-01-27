/*
  Gothic Memory Fix

  @author: lviper
  
  @license: GNU GPL 2.0 / BSD-derived
    See Readme_G_mem_fix_en.txt for details.
    (brief: you free to distribute or change code and dll).

  @version: 07

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

  I compiled this program with Hoard 3.10, JeMalloc 4.0.4, TCMalloc 2.4 and
    allocators from standard VS 2015 C++ library, but you can experiment with 
    your favorite allocation memroy library for best results for yourself.
  If any other game or program use old version of SmartHeap library (shw32.dll) 
    you can try to use this fix, but in this case you must implement all 
    functions, that program uses and maybe change ordinal value at def file 
    or add some new functions if version shw32.dll is differrent.


  Happy gaming and coding :)
           lviper, Moscow, 2016/01/24
*/


//#define MEM_USE_LIBHOARD		// Compile with libhoard (need also add UseLibHoard.cpp)
//#define MEM_USE_JEMALLOC		// Compile with JeMalloc
//#define MEM_USE_TCMALLOC		// Compile with TCMalloc
//#define MEM_USE_CRT			// Compile with standard library (by default if not defined others)

// add $(PlatformToolset) to definitions in project settings
#ifdef v140_xp
#define USE_XP_COMPATIBILITY
#endif

#if !defined(MEM_USE_LIBHOARD) && !defined(MEM_USE_JEMALLOC) && !defined(MEM_USE_TCMALLOC) && !defined(MEM_USE_CRT)
#define MEM_USE_CRT
#endif

//#define KEEP_ALLOCATED_NOTHING	// Don't save any information about blocks, returned by shw_*alloc
//#define KEEP_ALLOCATED_POINTERS	// Save only pointers to allocated blocks
//#define KEEP_ALLOCATED_SIZES		// Save pointers and size of allocated blocks

#define MEMCALLTYPE __cdecl


#define NOMINMAX
#include <Windows.h>
#include <stdlib.h>
#include <string>
using namespace std::string_literals;

#ifdef MEM_USE_JEMALLOC
#define KEEP_ALLOCATED_POINTERS
#define USE_STATIC
#include <jemalloc/jemalloc.h>
#ifdef USE_XP_COMPATIBILITY
#pragma comment(lib, "libjemalloc_x86_Release-Static-xp.lib" )
#else
#pragma comment(lib, "libjemalloc_x86_Release-Static.lib" )
#endif
#define shw_malloc je_malloc
#define shw_free je_free
#define shw_calloc je_calloc
#define shw_realloc je_realloc
#define shw_msize je_malloc_usable_size
#define shw_init je_init()
#define shw_uninit je_uninit()

#elif defined(MEM_USE_TCMALLOC)
#define KEEP_ALLOCATED_SIZES

#include <gperftools\tcmalloc.h>
#ifdef USE_XP_COMPATIBILITY
#pragma comment(lib, "libtcmalloc_minimal-xp.lib")
#else
#pragma comment(lib, "libtcmalloc_minimal.lib")
#endif

#define shw_malloc tc_malloc
#define shw_free tc_free
#define shw_calloc tc_calloc
#define shw_realloc tc_realloc
#define shw_msize tc_malloc_size		// >= required size by malloc, so better KEEP_ALLOCATED_SIZES
#define shw_init
#define shw_uninit

#else
#ifdef MEM_USE_LIBHOARD
#include <uselibhoard.cpp>

#ifdef USE_XP_COMPATIBILITY
#pragma comment(lib, "libhoard-xp.lib")
#else
#pragma comment(lib, "libhoard.lib")
#endif
#endif
#define KEEP_ALLOCATED_NOTHING

#define shw_malloc malloc
#define shw_free free
#define shw_calloc calloc
#define shw_realloc realloc
#define shw_msize _msize
#define shw_init
#define shw_uninit
#endif


#include <new>
#include <new.h>
#include <algorithm>
#include <memory>

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

// Because memory must be deallocated with the same allocation library we need to keep all pointers that we allocated.
// For some libraries (like Hoard or CRT) this isn't necessary at all, but I think this is still good to know what memory 
//   we allocated.
// For some libraries (TCMalloc for example) we also need to keep allocated sizes, because there isn't way to get exacly 
//   size, that was required in malloc.
class AllocatedKeeper {
#pragma region Allocator
	template <class T> class Allocator;
	template <> class Allocator<void>
	{
	public:
		typedef void * pointer;
		typedef const void* const_pointer;
		typedef void value_type;
		template <class U> struct rebind { typedef Allocator<U> other; };
	};

	template< typename T >
	class Allocator {
	public:
		typedef T value_type;
		typedef size_t size_type;
		typedef ptrdiff_t difference_type;
		typedef T* pointer;
		typedef const T* const_pointer;
		typedef T& reference;
		typedef const T& const_reference;

		template< typename U > struct rebind { typedef Allocator<U> other; };
		Allocator(void) {}
		Allocator(const Allocator& other) {}
		template <class U> Allocator(const Allocator<U>&) {}
		pointer address(reference r) const { return &r; }
		const_pointer address(const_reference r) const { return &r; }
		pointer allocate(size_type n, Allocator<void>::const_pointer hint = 0) {
			return reinterpret_cast<pointer>(shw_malloc(n));
		}
		void deallocate(pointer p, size_type n) {
			shw_free(p);
		}
		size_type max_size() const {
			return 0xFFFFFFFF / sizeof(T);
		}
		void construct(pointer ptr) {
			::new (reinterpret_cast<void*>(ptr)) T;
		}
		template <class U> void construct(pointer ptr, const U& val)	{
			::new (reinterpret_cast<void*>(ptr)) T(val);
		}
		void construct(pointer ptr, const T& val) {
			::new (reinterpret_cast<void*>(ptr)) T(val);
		}
		void destroy(pointer p) {
			p->T::~T();
		}
	};
#pragma endregion

public:
	// Check that block was allocated by shi_*alloc
	bool IsAllocated(void *block) {
#ifdef KEEP_ALLOCATED_NOTHING
		return true;
#else
		return allocated.count(block) > 0;
#endif
	}

	// Add new block to allocated
	void AddAllocated(void *block, size_t size) {
#ifdef KEEP_ALLOCATED_NOTHING
		return;
#else
		if (!block)
			return;
#ifdef KEEP_ALLOCATED_SIZES
		allocated[block] = size;
#else
		allocated.insert(block);
#endif
#endif
	}

	// Remove block from allocated
	bool Free(void *block) {
#ifndef KEEP_ALLOCATED_NOTHING
		if (!block)
			return false;

		return allocated.erase(block) > 0;
#endif
		return false;
	}

	// Get size of allocated block
	size_t GetSize(void *block) {
#ifndef KEEP_ALLOCATED_NOTHING
		auto size = allocated.find(block);
		if (size != allocated.end()) {
#ifdef KEEP_ALLOCATED_SIZES
			return size->second;
#else
			return shw_msize(block);
#endif
		}
		return _msize(block);
#else
		return shw_msize(block);
#endif
	}

private:
#ifndef KEEP_ALLOCATED_NOTHING
#ifdef KEEP_ALLOCATED_SIZES
	typedef std::unordered_map<void*, size_t, std::hash<void*>, std::equal_to<void*>, Allocator<std::pair<void*, size_t>>> AllocatedContainer;
#else
	typedef std::unordered_set<void*, std::hash<void*>, std::equal_to<void*>, Allocator<void*>> AllocatedContainer;
#endif

	AllocatedContainer allocated;
#endif
};
AllocatedKeeper allocatedKeeper;

class Settings {
public:
	// Update path with SystemPack.ini with settings
	void UpdateIniPath() {
		char fullPath[MAX_PATH+1];
		auto readedPath = ::GetModuleFileNameA(nullptr, fullPath, MAX_PATH + 1);
		if ( readedPath >= MAX_PATH ) {
			fullPath[MAX_PATH] = '\0';		// On XP if path too long, then it won't be '\0' truncated
		}
		else if (readedPath == 0) {
			SetDefaultIniPath();
			return;
		}

		iniPath = fullPath;
		
		auto lastDir = iniPath.find_last_of("/\\");
		if (lastDir == iniPath.npos) {
			SetDefaultIniPath();
			return;
		}

		iniPath = iniPath.substr(0, lastDir + 1) + iniFile;
	}

	// Load integer type (keep original value if don't found)
	template< typename T >
	bool LoadInt(const char *name, T &value, int notExistMarker = -123456 ) {
		int currentValue = ::GetPrivateProfileIntA(iniAppName, name, notExistMarker, iniPath.c_str());
		if (currentValue == notExistMarker) {
			needSave = true;
			return false;
		}

		value = static_cast<T>(currentValue);
		return true;
	}

	void SaveInt(const char *name, int value) {
		WritePrivateProfileStringA(iniAppName, name, std::to_string(value).c_str(), iniPath.c_str());
	}

	bool IsNeedSave() const {
		return needSave;
	}

	void Save(bool forceCreateFile) {
		if (!needSave)
			return;

		UpdateIniPath();

		if (!forceCreateFile) {
			WIN32_FIND_DATAA data;
			HANDLE hFile = FindFirstFileA(iniPath.c_str(), &data);
			if (hFile == INVALID_HANDLE_VALUE) {
				return;
			}
			FindClose(hFile);
		}

		SaveInt("bShowGothicError", showGothicErrorMessage);
		SaveInt("bShowMsgBox", showMessageBox);
		SaveInt("reserveInMb", reserveInMB);
		SaveInt("bUseNewHandler", useNewHandler);
		needSave = false;
	}


	void Load() {
		needSave = false;
		UpdateIniPath();
		LoadInt("bShowGothicError", showGothicErrorMessage);
		LoadInt("bShowMsgBox", showMessageBox);
		LoadInt("reserveInMb", reserveInMB);
		LoadInt("bUseNewHandler", useNewHandler);
	}


	bool IsUseNewHandler() const {
		return useNewHandler;
	}

	bool IsShowMessageBox() const {
		return showMessageBox;
	}

	bool IsShowGothicError() const {
		return showGothicErrorMessage;
	}

	size_t GetReserveInMB() const {
		return reserveInMB;
	}

private:
	void SetDefaultIniPath() {
		iniPath = ".\\"s + iniFile;
	}

	const char *iniAppName = "SHW32";
	const char *iniFile = "SystemPack.ini";

	std::string iniPath = ".\\SystemPack.ini"s;

	bool useNewHandler = true;
	bool showMessageBox = true;
	bool showGothicErrorMessage = true;
	size_t reserveInMB = 50;
	
	bool needSave = false;
};
Settings settings;

// sometimes OutOfMemory handler's exit and fatal will ignore interruption or exit. Then just use this flag to correct OutOfMemory again in the next call shi_* function.
bool isOutOfMemoryAlreadyHappens = false;


// Emergency buffer for free after OutOfMemory to correct work system functions
class EmergencyBufferForOutOfMemory {
public:
	~EmergencyBufferForOutOfMemory() {
		FreeMessageBoxBuffer();
		FreeGothicBuffer();
	}

	void FreeMessageBoxBuffer() {
		shw_free(bufferForMessageBox);
		bufferForMessageBox = nullptr;
	}
	void FreeGothicBuffer() {
		shw_free(bufferForGothicError);
		bufferForGothicError = nullptr;
	}
												
												
	// call after Settings::Load
	void Initialize() {
		size_t sizeForGothicError = 50;
		sizeForGothicError = settings.GetReserveInMB();
		sizeForGothicError = std::max(minSize, std::min(maxSize, sizeForGothicError));
												
		size_t sizeForMessageBox = 0;
		if (!settings.IsShowGothicError() && !settings.IsShowMessageBox())
			sizeForGothicError = 5;
		else if (settings.IsShowMessageBox()) {
			if (settings.IsShowGothicError()) {
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
			bufferForGothicError = shw_malloc(sizeForGothicError*Mb);
		if (sizeForMessageBox > 0)
			bufferForMessageBox = shw_malloc(sizeForMessageBox*Mb);
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

	if (settings.IsShowGothicError()) {
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
	if (!isOutOfMemoryAlreadyHappens && settings.IsShowMessageBox())
		MessageBoxW(nullptr, L"OutOfMemory!", nullptr, MB_OK | MB_ICONERROR);

	std::set_new_handler(nullptr);
	_set_new_mode(0);

	TerminateProgram();
}

bool IsNeedReallocate(void *data) {
	bool result = false;

	if (data == nullptr && !isOutOfMemoryAlreadyHappens) {
		emergencyBuffer.FreeMessageBoxBuffer();
		int chose = MessageBoxW(nullptr, L"OutOfMemory!", nullptr, MB_ABORTRETRYIGNORE | MB_ICONERROR);
		if (chose == IDABORT) {
			TerminateProgram();
		}
		else if (chose == IDRETRY) {
			result = true;
		}
	}

	return result;
}

extern "C" {
	size_t MEMCALLTYPE shi_msize(void *data) {
		return allocatedKeeper.GetSize(data);
	}

	BOOL MEMCALLTYPE shi_MemInitDefaultPool() {
		return TRUE;
	}

	void* MEMCALLTYPE shi_malloc(size_t size) {
		void *result = nullptr;
		do {
			result = shw_malloc(size);
		} while (IsNeedReallocate(result));

		allocatedKeeper.AddAllocated(result, size);
		return result;
	}

	void* MEMCALLTYPE shi_calloc(size_t num, size_t size) {
		void *result = nullptr;
		do {
			result = shw_calloc(num, size);
		} while (IsNeedReallocate(result));
		allocatedKeeper.AddAllocated(result, size);
		return result;
	}

	void MEMCALLTYPE shi_delete(void *data) {
		if (allocatedKeeper.Free(data))
			shw_free(data);
		else
			free(data);
	}

	void* MEMCALLTYPE shi_realloc(void* data, size_t size) {
		if (size == 0) {
			shi_delete(data);
			return nullptr;
		}

		if (allocatedKeeper.IsAllocated(data)) {
			void *result = nullptr;
			do {
				result = shw_realloc(data, size);
			} while (IsNeedReallocate(result));
			if (result != nullptr) {
				allocatedKeeper.Free(data);
				allocatedKeeper.AddAllocated(result, size);
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
		settings.Load();
		settings.Save(false);
		emergencyBuffer.Initialize();

		if (settings.IsUseNewHandler()) {
			std::set_new_handler(&OutOfMemoryHandler);
			_set_new_mode(1);
		}
	} else if (reason_for_call == DLL_PROCESS_DETACH) {
		emergencyBuffer.FreeMessageBoxBuffer();
		emergencyBuffer.FreeGothicBuffer();
		if (settings.IsNeedSave()) {
			settings.Load();		// try to load settings if any added by other modules
			settings.Save(true);	// save all settings to the file
		}
		shw_uninit;
	}

	return TRUE;
}
