#pragma once
#include <Windows.h>
#include <list>
#include <string>
#include <vector>
#define windows_Width 750 
#define windows_Height 600
#define PAGE_SIZE 0x1000

enum FunctionName :int
{
	ePsSuspendThread,
	ePsResumeThread,
	eZwProtectVirtualMemory,
	eMiLocateAddress,
	eZwCreateThreadEx,
	eMax,
};
struct HookData
{
	bool bSelect;  //imgui �Ƿ�ѡ��
	bool bHook;    //�Ƿ��Ѿ� Hook���������
	PVOID pOrgFunc; // ������ԭʼ��ַ
	char sz[100];   //�ҹ��ĺ�������
};


class gui
{
public:
	gui() {

	}
	~gui()
	{

	}
	bool CreateGui();
	void FreeGui();
	int GetDebugModels();
	int GetEnumModuleTypes();
	bool IsMapInject();
	bool IsEnablePrivateHandleTable();
private:
	static DWORD guiThread(gui* pThis);
	BOOL InitPtrForSymBool();
	void setHook();
private:
	std::list<std::string> m_RecordLogList;
	std::vector<HookData> m_hook;
	HANDLE m_hImGuiThreadHanle = 0;
	HWND m_hwnd = 0;
	bool m_done = false;
	bool m_bInit = 0;
	bool m_show = false;
	bool bClear = false;
	bool m_bEnableDebug = true;
	int nDebugModel = 0;    //����ģʽ
	int nEnumModuleTypes = 0; //����ģ��ķ�ʽ
	bool nPrivateHandleTable;
	bool nInjectEx;

};

extern ULONG64 nFunRva[eMax];