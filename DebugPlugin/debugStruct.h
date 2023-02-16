#pragma once
#include <Windows.h>
#include <vector>
#include <list>
#include <TlHelp32.h>
#include "gui.h"


#define PADDING(type, name, size) union { type name; char name##_padding[size]; }
#define STATUS_WX86_SINGLE_STEP 0x4000001E
#define EXCEPTION_NO_HADNLE (ULONG64)(-1)
#define EXCEPTION_HADNLE    (ULONG64)(0x88)
#define UE_TRAP_FLAG (0x100)
#define UE_RESUME_FLAG (0x10000)

typedef struct _CREATE_THREAD_EVENT_ {
	ULONG64 hThread;
	ULONG64 lpStartAddress;
} CREATE_THREAD_EVENT, * PCREATE_THREAD_EVENT;


typedef struct _EXIT_THREAD_EVENT_ {
	ULONG dwExitCode;
} EXIT_THREAD_EVENT, * PEXIT_THREAD_EVENT;


typedef struct _LOAD_DLL_EVENT_ {
	ULONG64 hFile;
	ULONG64 lpBaseOfDll;
	ULONG64 nImageSize;
} LOAD_DLL_EVENT, * PLOAD_DLL_EVENT;

typedef struct _UNLOAD_DLL_EVENT_ {
	ULONG64 lpBaseOfDll;
} UNLOAD_DLL_EVENT, * PUNLOAD_DLL_EVENT;

typedef struct _DEBUG_EVENT_EX {
	ULONG dwDebugEventCode;
	ULONG64 dwProcessId;
	ULONG64 dwThreadId;
	union {

		CREATE_THREAD_EVENT CreateThread;
		EXIT_THREAD_EVENT ExitThread;
		LOAD_DLL_EVENT LoadDll;
	} u;
} DEBUG_EVENT_EX, * PDEBUG_EVENT_EX;

typedef struct _SUSUPENTHREAD_DATA_ {
	HANDLE hThread;
	HANDLE nTid;
}SUSUPENTHREAD_DATA, * PSUSUPENTHREAD_DATA;
struct CriticalSectionLock
{
	CRITICAL_SECTION cs;

	void Init()
	{
		InitializeCriticalSection(&cs);
	}

	void Enter()
	{
		EnterCriticalSection(&cs);
	}
	BOOL TryEnter()
	{
		return TryEnterCriticalSection(&cs);
	}

	void Leave()
	{
		LeaveCriticalSection(&cs);
	}
	void UnLoad()
	{
		DeleteCriticalSection(&cs);
	}
};
enum  ModuleTypes :int
{
	eApi,
	ePeb,
	eVad
};
typedef struct _VEHDebugSharedMem_
{
	PADDING(CONTEXT, CurrentContext, 0x1000);
	PADDING(DEBUG_EVENT, DebugEvent, 0x100);
	PADDING(HANDLE, HasDebugEvent, 8); //�����Խ��̣����쳣�¼�
	PADDING(HANDLE, HasHandledDebugEvent, 8); //������ �Ƿ���������쳣�¼�
	PADDING(HANDLE, hDevice, 8);
	ULONG veh_debug_active;
	ULONG dwContinueStatus;
	char ConfigName[2][256];
}VEHDebugSharedMem, * PVEHDebugSharedMem;
struct DebugStruct
{
	HANDLE nCurDebuggerPid;       //��ǰ�����Խ��̵�PID
	PVEHDebugSharedMem  pShareMem;      //ָ�����ڴ� VEHDebugSharedMem*
	HANDLE hCurDebuggerHandle;    //��ǰ�����Խ��̵ľ��
	HANDLE hFileMapping;  //�����ڴ�ľ��
	HANDLE HasDebugEvent; //�����Խ��̣����쳣�¼�
	HANDLE HasHandledDebugEvent;//������ �Ƿ���������쳣�¼�
	HANDLE hGetDebugEventThread;//��ȡ ����/�����߳� ģ����ص��߳̾��
	HANDLE hNotifyEvent;           //�¼� :���� ֪ͨ hGetDebugEventThread �̶߳�ȡ �¼�
	HANDLE hDevice;              //�������
	gui* pGui;
	std::list<DEBUG_EVENT> m_event; //�������е��¼�(�̴߳���/����,�쳣���¼�)
	std::list<MODULEENTRY32W> m_moduleInfo;//ֻ�ǵ����� DebugActiveProcess ���� �����Խ��̵�����ģ����Ϣ 
	std::list<SUSUPENTHREAD_DATA> m_SuspendThread; //�����½��߳�
	CriticalSectionLock handler_cs; //��
	bool bStop;
	char ConfigName[3][256]; //
	DEBUG_EVENT_EX DebugEvent[100] = { 0 }; //һ��ȫ�ֱ��������������ں� �߳�/ģ��ص� ����Ϣ
	ULONG_PTR dr0 = 0;
	ULONG_PTR dr1 = 0;
	ULONG_PTR dr2 = 0;
	ULONG_PTR dr3 = 0;
	ULONG_PTR dr6 = 0;
	ULONG_PTR dr7 = 0;
};

enum  class ExceptionType :int
{
	eBreakPoint,
	eHardWare,
	eMemoryVilation,
	eMax
};
enum class ExceptionState :int
{
	eValid,
	eRemove,
	eMax
};

struct RecordException
{
	ExceptionType nExceptionType;
	ExceptionState nState;
	ULONG_PTR nExceptionAddress;
};

namespace debugStruct
{
	bool InitDebugStruct(PVOID pThis);
	DebugStruct* GetDebugStructPointer();
	bool InitDebuggerInfo(HANDLE nPid);
	PVOID GetDebuggerProcessModuleBase(WCHAR* szModule);
	void FreeDebugStruct();
	void RecordExceptionMarkValid(ExceptionType nType, ULONG_PTR nAddress);
	void SetExceptionMarkRemove(ExceptionType nType, ULONG_PTR nAddress);
	BOOL FindRemoveStateRecordException(ExceptionType nType, ULONG_PTR nAddress);
}

