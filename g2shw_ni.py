# python3
# for gothic2's shw32.dll generate function declaration in cpp and definition in def

shw32_exported_functions = """00000002	MemVersion
00000003	_MemPoolSetGrowIncrement
00000004	MemRegisterTask
00000005	_MemPoolSetHighThreads
00000006	MemUnregisterTask
00000007	MemProcessSetLargeBlockThreshold
00000008	MemProcessSetGrowIncrement
00000009	MemProcessSetFreeBytes
0000000A	MemPoolInit
0000000B	MemPoolInitFS
0000000C	MemPoolInitNamedShared
0000000D	MemPoolInitNamedSharedEx
0000000E	MemPoolAttachShared
0000000F	MemPoolFree
00000010	MemPoolInitRegion
00000011	MemPoolInitRegionEx
00000012	MemPoolLock
00000013	MemPoolUnlock
00000014	MemPoolSetPageSize
00000015	MemPoolSetSmallBlockAllocator
00000016	MemPoolSetBlockSizeFS
00000017	MemPoolSetSmallBlockSize
00000018	MemPoolSetFloor
00000019	MemPoolSetCeiling
0000001A	MemPoolSetFreeBytes
0000001B	shi_MemDefaultPool
0000001C	MemPoolPreAllocate
0000001D	MemPoolPreAllocateHandles
0000001E	MemPoolShrink
0000001F	shi_freeingDLL
00000020	shi_loadingDLL
00000021	_shi_patchMallocs
00000022	MemPoolSize
00000023	MemPoolCount
00000028	MemPoolInfo
0000002A	MemPoolFirst
0000002B	MemPoolNext
0000002E	MemPoolWalk
00000030	MemPoolCheck
00000032	MemAlloc
00000034	MemFree
00000036	MemReAlloc
00000038	MemLock
00000039	MemUnlock
0000003A	MemFix
0000003B	MemUnfix
0000003C	MemLockCount
0000003E	MemIsMoveable
00000040	MemHandle
00000042	MemSize
00000043	MemSizeRequested
00000046	MemAllocPtr
00000048	MemFreePtr
0000004A	MemReAllocPtr
0000004D	MemSizePtr
0000004F	MemCheckPtr
00000054	MemAllocFS
00000056	MemFreeFS
0000005A	MemDefaultErrorHandler
0000005E	MemSetErrorHandler
00000060	MemErrorUnwind
000000FB	shi_new
000000FC	shi_expand
000000FD	shi_MemFreeDefaultPool
000000FE	shi_heapmin
00000100	shi_heapwalk
00000104	shi_heapset
00000106	shi_heapadd
00000107	shi_heapused
00000108	shi_heapchk
0000010A	shi_deleteStdcall
0000010B	shi_deletePtrStdcall
0000010C	shi_set_new_handler
0000010D	shi_newStdcall
00000126	shi_notSMP
00000127	shi_getThreadPool
00000128	shi_freeThreadPools
0000012A	_shi_enterCriticalSection
0000012B	_shi_leaveCriticalSection""".split("\n")

template_cpp = """
	void MEMCALLTYPE {0}() {{
		notImplemented(__FUNCTION__);
	}}"""
template_def = "	{0}	@{1}\n"


with open("fcpp.txt", "w") as file_cpp, open("fdef.txt", "w") as file_def:
  for line in shw32_exported_functions:
    line = line.strip().split()
    function_name = "notImplemented_{0}_{1}".format(line[0], line[1])
  
    file_cpp.write(template_cpp.format(function_name))
    file_def.write(template_def.format(function_name, int(line[0],16)))

