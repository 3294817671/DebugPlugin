#include "Function.h"
#include "stl.h"
#include "exapi.h"
#include "struct.h"
#include "symbol.h"
VOID GetPhysicalPageInfo(ULONG64 nCr3, ULONG64 nVirtualAddress, pphysical_memory_data pPhysicalPageInfo)
{
	NTSTATUS  status = STATUS_UNSUCCESSFUL;

	if (!nCr3 || !nVirtualAddress || !MmIsAddressValid(pPhysicalPageInfo)) return;


	int pml4index = 0;
	int pagedirptrindex = 0;
	int pagedirindex = 0;
	int pagetableindex = 0;

	pml4index = (nVirtualAddress >> 39) & 0x1ff;
	pagedirptrindex = (nVirtualAddress >> 30) & 0x1ff;
	pagedirindex = (nVirtualAddress >> 21) & 0x1ff;
	pagetableindex = (nVirtualAddress >> 12) & 0x1ff;


	PHYSICAL_ADDRESS ph = { 0 };
	ph.QuadPart = (LONGLONG)(nCr3 & 0x7FFFFFFFF000) + 0x8 * pml4index;
	MM_COPY_ADDRESS mca = { 0 };
	SIZE_T n = 0;
	mca.PhysicalAddress = ph;
	status = MmCopyMemory(&pPhysicalPageInfo->pxe, mca, sizeof(PHYSICAL_ADDRESS), MM_COPY_MEMORY_PHYSICAL, &n);

	if (NT_SUCCESS(status) && pPhysicalPageInfo->pxe.u.Hard.PageFrameNumber)
	{
		//��ȡPPE
		ph.QuadPart = pPhysicalPageInfo->pxe.u.Hard.PageFrameNumber * PAGE_SIZE + (0x8 * pagedirptrindex);
		mca.PhysicalAddress = ph;
		status = MmCopyMemory(&pPhysicalPageInfo->ppe, mca, sizeof(PHYSICAL_ADDRESS), MM_COPY_MEMORY_PHYSICAL, &n);
		if (NT_SUCCESS(status) && pPhysicalPageInfo->ppe.u.Hard.PageFrameNumber)
		{
			ph.QuadPart = pPhysicalPageInfo->ppe.u.Hard.PageFrameNumber * PAGE_SIZE + (0x8 * pagedirindex);
			mca.PhysicalAddress = ph;
			status = MmCopyMemory(&pPhysicalPageInfo->pde, mca, sizeof(PHYSICAL_ADDRESS), MM_COPY_MEMORY_PHYSICAL, &n);
			if (NT_SUCCESS(status) && pPhysicalPageInfo->pde.u.Hard.PageFrameNumber)
			{
				ph.QuadPart = pPhysicalPageInfo->pde.u.Hard.PageFrameNumber * PAGE_SIZE + (0x8 * pagetableindex);
				mca.PhysicalAddress = ph;
				status = MmCopyMemory(&pPhysicalPageInfo->pte, mca, sizeof(PHYSICAL_ADDRESS), MM_COPY_MEMORY_PHYSICAL, &n);
				if (!NT_SUCCESS(status)) {
					pPhysicalPageInfo->pte.u.Long = 0;
				}
			}
		}

	}

	return;
}

namespace Function {
	NTSTATUS MmCopyMemoryEx(ULONG64 hProcess, ULONG64 lpBaseAddress, ULONG64 lpBuffer, ULONG64 nSize)
	{
		NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;

		if (nSize == 0 || lpBaseAddress <= PAGE_SIZE || nSize > 0x200000) return ntStatus;

		PVOID pProcess = NULL;
		if (NT_SUCCESS(ObReferenceObjectByHandle(
			(HANDLE)hProcess,
			0,
			*PsProcessType,
			KernelMode,
			&pProcess,
			NULL)))
		{
			pread_physical_data pTempBuffer = NULL;
			ULONG64 dwTempCR3 = 0;

			dwTempCR3 = *(PULONG_PTR)((ULONG_PTR)pProcess + 0x28);

			pTempBuffer = (pread_physical_data)ExAllocatePoolWithTag(NonPagedPool, sizeof(read_physical_data), 'phy');
			if (pTempBuffer)
			{

				ULONG64 nBaseAddress = 0;
				ULONG nPageOffset = 0;
				ULONG64 nEnsurePte = 0;
				ULONG64 nAllocateSize = 0; //����Ҫ ������ٵ�(NonPagedPool)�ڴ�
				BOOLEAN bLargePagePage = FALSE;
				PVOID pMemoryContextBuffer = NULL;
				SIZE_T nTempLength = 0;
				nPageOffset = lpBaseAddress & 0xfff;
				nBaseAddress =lpBaseAddress & (~0xfff);
				nEnsurePte = nPageOffset + nSize;
				RtlSecureZeroMemory(pTempBuffer, sizeof(read_physical_data));
				GetPhysicalPageInfo(dwTempCR3, nBaseAddress, &pTempBuffer->pma);

				if (pTempBuffer->pma.pde.u.Hard.LargePage && pTempBuffer->pma.pde.u.Hard.PageFrameNumber)
				{
					//pde��ҳ ֱ�ӹ��� 0x200 000��Χ���ڴ�
					nAllocateSize =nSize;
					bLargePagePage = TRUE;
				}
				else
				{

					if (pTempBuffer->pma.pte.u.Hard.PageFrameNumber && pTempBuffer->pma.pde.u.Hard.PageFrameNumber)
					{

						MM_COPY_ADDRESS mca = { 0 };
						int pagetableindex = 0;

						//pfn ����  #define PAGE_ERROR_UNKNOWN         0xffff00000  �����
						if (pTempBuffer->pma.pte.u.Hard.PageFrameNumber < PAGE_ERROR_UNKNOWN)
						{
							//���� page_noacess /page_guard�ڴ�
							if ((pTempBuffer->pma.pte.u.Hard.PageFrameNumber > PAGE_NOACESS_OR_PAGE_GUARD) && (pTempBuffer->pma.pte.u.Hard.PageFrameNumber < PAGE_ERROR_UNKNOWN))
							{
								pTempBuffer->pma.pte.u.Hard.PageFrameNumber = (UINT64)((UINT64)pTempBuffer->pma.pte.u.Hard.PageFrameNumber - PAGE_NOACESS_OR_PAGE_GUARD);
								if (pTempBuffer->pma.pte.u.Hard.PageFrameNumber > PAGE_NOACESS_OR_PAGE_GUARD) {
									nAllocateSize = 0;
									goto __MmCopyVirtualMemory;
								}

							}
							pTempBuffer->pte[0].u.Long = pTempBuffer->pma.pte.u.Hard.PageFrameNumber * PAGE_SIZE;
							nAllocateSize = PAGE_SIZE;
							//pte ����0x1000
							//�� nEnsurePte ���� PAGE_SIZE��ʱ�� ����Ҫ��ȡ ��nBaseAddress+PAGE_SIZE����PTE����Ȼ������� page_fault_in_nonpage_area ����
							if (nEnsurePte > PAGE_SIZE)
							{
								for (int i = 1; i <= (nEnsurePte / PAGE_SIZE); i++)
								{
									nBaseAddress += PAGE_SIZE;
									pagetableindex = (nBaseAddress >> 12) & 0x1ff;
									mca.PhysicalAddress.QuadPart = pTempBuffer->pma.pde.u.Hard.PageFrameNumber * PAGE_SIZE + (int)(0x8 * pagetableindex);
									ntStatus = MmCopyMemory(&pTempBuffer->pte[i], mca, sizeof(PHYSICAL_ADDRESS), MM_COPY_MEMORY_PHYSICAL, &nTempLength);
									if (!NT_SUCCESS(ntStatus))
									{
										nAllocateSize = 0;
										break;
									}

									if ((pTempBuffer->pte[i].u.Hard.PageFrameNumber > PAGE_NOACESS_OR_PAGE_GUARD) && (pTempBuffer->pte[i].u.Hard.PageFrameNumber < PAGE_ERROR_UNKNOWN))
									{

										pTempBuffer->pte[i].u.Hard.PageFrameNumber = (UINT64)((UINT64)pTempBuffer->pte[i].u.Hard.PageFrameNumber - PAGE_NOACESS_OR_PAGE_GUARD);
										if (pTempBuffer->pte[i].u.Hard.PageFrameNumber > PAGE_NOACESS_OR_PAGE_GUARD) {
											nAllocateSize = 0;
											goto __MmCopyVirtualMemory;
										}
									}


									nAllocateSize += PAGE_SIZE;
								}
							}

						}
						else
						{


							goto __MmCopyVirtualMemory;

						}



					}
					else
					{

					__MmCopyVirtualMemory:
						nAllocateSize = 0;
						ntStatus = STATUS_UNSUCCESSFUL;
						if (lpBaseAddress < SYSTEM_ADDRESS_START)
						{
							SIZE_T bytes = 0;
							PEPROCESS pSourceProc = NULL, pTargetProc = NULL;
							PVOID pSource = NULL, pTarget = NULL;
							pSourceProc = (PEPROCESS)pProcess;
							pTargetProc = PsGetCurrentProcess();
							pSource = (PVOID)lpBaseAddress;
							pTarget = (PVOID)lpBuffer;
							ntStatus = MmCopyVirtualMemory(pSourceProc, pSource, pTargetProc, pTarget, nSize, KernelMode, &bytes);

						}
					}


				}

				if (nAllocateSize)
				{
					ntStatus = STATUS_UNSUCCESSFUL;
					nAllocateSize += 0x100;//�����һ��� ��������� page_fault_in_nonpage_area ֱ�� nAllocateSize*2 
					pMemoryContextBuffer = ExAllocatePoolWithTag(NonPagedPool, nAllocateSize, 'mcb');
					if (pMemoryContextBuffer)
					{
						MM_COPY_ADDRESS mca2 = { 0 };

						RtlSecureZeroMemory(pMemoryContextBuffer, nAllocateSize);
						if (bLargePagePage)
						{
							//��ҳ ֱ�ӿ���

							if (pTempBuffer->pma.pde.u.Hard.PageFrameNumber > PAGE_NOACESS_OR_PAGE_GUARD && pTempBuffer->pma.pde.u.Hard.PageFrameNumber < PAGE_ERROR_UNKNOWN)
							{
								pTempBuffer->pma.pde.u.Hard.PageFrameNumber = (UINT64)((UINT64)pTempBuffer->pma.pde.u.Hard.PageFrameNumber - PAGE_NOACESS_OR_PAGE_GUARD);
							}
							//1FFFF
							mca2.PhysicalAddress.QuadPart = pTempBuffer->pma.pde.u.Hard.PageFrameNumber * PAGE_SIZE + (lpBaseAddress & 0x1FFFFF);

							ntStatus = MmCopyMemory(pMemoryContextBuffer, mca2, nSize, MM_COPY_MEMORY_PHYSICAL, &nTempLength);
						}
						else
						{

							mca2.PhysicalAddress.QuadPart = 0;
							PVOID pTempMemoryContextBuffer = NULL;
							pTempMemoryContextBuffer = pMemoryContextBuffer;
							//0x200����Դ��һ��pde(���Ǵ�ҳ) ����Χ��0x1000 ���� 0x1000/8 �� pte 
							for (int i = 0; i < 0x200; i++)
							{
								mca2.PhysicalAddress.QuadPart = pTempBuffer->pte[i].u.Hard.PageFrameNumber * PAGE_SIZE;
								if (mca2.PhysicalAddress.QuadPart == 0) {
									break;
								}

								ntStatus = MmCopyMemory(pTempMemoryContextBuffer, mca2, PAGE_SIZE, MM_COPY_MEMORY_PHYSICAL, &nTempLength);
								if (!NT_SUCCESS(ntStatus))
								{
									DbgPrintEx(0, 0, "hzw: ����%s �������ַʧ�� ��ַ:%p ����ҳ:%p  ntStatus:%x\n",
										PsGetProcessImageFileName((PEPROCESS)pProcess), lpBaseAddress, mca2, ntStatus);
									break;
								}

								pTempMemoryContextBuffer = (PVOID)((ULONG_PTR)pTempMemoryContextBuffer + PAGE_SIZE);

							}
						}

						if (NT_SUCCESS(ntStatus))
						{
							ntStatus = STATUS_UNSUCCESSFUL;
							PVOID pCopyAddress = NULL;
							if (!bLargePagePage) {
								pCopyAddress = (PVOID)((ULONG_PTR)pMemoryContextBuffer + nPageOffset);
							}
							else
							{
								pCopyAddress = pMemoryContextBuffer;
							}
							__try
							{
								ProbeForWrite((PVOID)lpBuffer, nSize, 1);
								RtlCopyMemory((PVOID)lpBuffer, pCopyAddress, nSize);


								ntStatus = STATUS_SUCCESS;
							}
							__except (1)
							{
								ntStatus = GetExceptionCode();
							}
						}
					}

				}


				if (pMemoryContextBuffer)
				{
					ExFreePoolWithTag(pMemoryContextBuffer, 0);
					pMemoryContextBuffer = NULL;
				}

				if (pTempBuffer)
				{
					ExFreePoolWithTag(pTempBuffer, 0);
					pTempBuffer = NULL;
				}
			}
			if (pProcess)
			{
				ObfDereferenceObject(pProcess);
				pProcess = NULL;
			}
		}


		return ntStatus;
	}

	NTSTATUS BBCopyMemory(ULONG64 hProcess, ULONG64 lpBaseAddress, ULONG64 lpBuffer, ULONG64 nSize, ULONG64 write)
	{
		NTSTATUS status = STATUS_UNSUCCESSFUL;
		PVOID pProcess = NULL, pSourceProc = NULL, pTargetProc = NULL;
		PVOID pSource = NULL, pTarget = NULL;


		if (!hProcess || !lpBaseAddress || !lpBuffer || !nSize)
		{
			return STATUS_UNSUCCESSFUL;
		}

		status = ObReferenceObjectByHandle(
			(HANDLE)hProcess,
			0,
			*PsProcessType,
			KernelMode,
			&pProcess,
			NULL);
		if (NT_SUCCESS(status))
		{
			SIZE_T bytes = 0;

			// Write
			if (write != FALSE)
			{

				pSourceProc = PsGetCurrentProcess();
				pTargetProc = pProcess;
				pSource = (PVOID)lpBuffer;
				pTarget = (PVOID)lpBaseAddress;
			}
			// Read
			else
			{
				pSourceProc = pProcess;
				pTargetProc = PsGetCurrentProcess();
				pSource = (PVOID)lpBaseAddress;
				pTarget = (PVOID)lpBuffer;


			}

			status = MmCopyVirtualMemory((PEPROCESS)pSourceProc, pSource, (PEPROCESS)pTargetProc, pTarget, nSize, KernelMode, &bytes);


		}

		if (pProcess) {
			ObfDereferenceObject(pProcess);
		}

		return status;
	}


	NTSTATUS MdlForR3(ULONG ProcessHandle, ULONG nSize, ULONG64 dwVitruallAddress, ULONG64 pBuffer)
	{
		NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;

		PVOID pProcess = NULL;
		if (NT_SUCCESS(ObReferenceObjectByHandle(
			(HANDLE)ProcessHandle,
			0,
			*PsProcessType,
			KernelMode,
			&pProcess,
			NULL)))
		{
			PVOID pTempBuffer = NULL;
			pTempBuffer = ExAllocatePoolWithTag(NonPagedPool, nSize, 0);
			if (pTempBuffer)
			{
				KAPC_STATE kpc = { 0 };
				__try
				{
					ProbeForRead((PVOID)pBuffer, nSize, 1);
					RtlCopyMemory(pTempBuffer, (PVOID)pBuffer, nSize);
					KeStackAttachProcess((PEPROCESS)pProcess, &kpc);
					__try
					{
						ntStatus = mdlWrite(dwVitruallAddress, (ULONG_PTR)pTempBuffer, nSize);
					}
					__finally
					{
						KeUnstackDetachProcess(&kpc);
					}
				}
				__except (1)
				{

				}


				ExFreePoolWithTag(pTempBuffer, 0);
			}
			ObfDereferenceObject(pProcess);
		}

		return ntStatus;
	}

	NTSTATUS ReWriteZwProtectVirtualMemory(HANDLE ProcessHandle, PVOID lpAddress, SIZE_T dwSize, ULONG flNewProtect, PULONG flOldProtect)
	{
		NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
		t_ZwProtectVirtualMemory ZwProtectVirtualMemory = (t_ZwProtectVirtualMemory)symbol::MmGetSymbolRoutineAddress(FunctionType::eZwProtectVirtualMemory);
		if (!MmIsAddressValid(ZwProtectVirtualMemory)) return ntStatus;

		PVOID pTempProcess = NULL;
		KAPC_STATE kpc = { 0 };

		SIZE_T NumberOfBytesToProtect = dwSize;
		PVOID BaseAddress = lpAddress;
		ntStatus = ObReferenceObjectByHandle(
			(HANDLE)ProcessHandle,
			0,
			*PsProcessType,
			KernelMode,
			&pTempProcess,
			NULL);

		if (NT_SUCCESS(ntStatus))
		{
			__try
			{
				__try
				{
					KeStackAttachProcess((PEPROCESS)pTempProcess, &kpc);

					ntStatus = ZwProtectVirtualMemory(NtCurrentProcess(), &BaseAddress, &NumberOfBytesToProtect,
						flNewProtect, flOldProtect);

				}
				__finally
				{
					KeUnstackDetachProcess(&kpc);
				}

			}
			__except (1)
			{
				ntStatus = GetExceptionCode();
			}

		}


		if (pTempProcess)
		{
			ObfDereferenceObject(pTempProcess);
			pTempProcess = NULL;
		}

		return ntStatus;
	}

	NTSTATUS AllcoateMemory(HANDLE hProcess, PVOID lpAddress, ULONG dwSize, ULONG flAllocationType, ULONG flProtect, PVOID* pRetAllocateAddress)
	{
		NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
		PVOID pProcess = NULL;
		if (NT_SUCCESS(ObReferenceObjectByHandle(
			(HANDLE)hProcess,
			0,
			*PsProcessType,
			KernelMode,
			&pProcess,
			NULL)))
		{
			PVOID BaseAddress = lpAddress;
			KAPC_STATE kpc = { 0 };
			SIZE_T n = dwSize;
			__try
			{
				KeStackAttachProcess((PEPROCESS)pProcess, &kpc);
				__try
				{
					ntStatus = ZwAllocateVirtualMemory(NtCurrentProcess(), &BaseAddress, 0, &n, flAllocationType, flProtect);
					if (NT_SUCCESS(ntStatus))
					{
						RtlSecureZeroMemory(BaseAddress, n);
						MmSecureVirtualMemory(BaseAddress, n, PAGE_READWRITE);
						*pRetAllocateAddress = BaseAddress;
					}
				}
				__finally
				{
					KeUnstackDetachProcess(&kpc);
				}
			}
			__except (1)
			{

			}

			ObfDereferenceObject(pProcess);
		}

		return ntStatus;
	}

	NTSTATUS mdlWrite(ULONG_PTR SrcAddr, ULONG_PTR DstAddr, ULONG Size)
	{

		NTSTATUS status = STATUS_UNSUCCESSFUL;

		if (!SrcAddr ||
			!DstAddr ||
			!Size)
		{
			return status;
		}

		PMDL pSrcMdl = NULL;
		PVOID pMappedSrc = NULL;


		pSrcMdl = IoAllocateMdl((PVOID)SrcAddr, Size, FALSE, FALSE, NULL);
		//pSrcMdl = IoAllocateMdl_((PVOID)SrcAddr, Size, FALSE, FALSE, NULL);
		if (pSrcMdl)
		{

			__try
			{
				MmProbeAndLockPages(pSrcMdl, UserMode, IoReadAccess);
				pMappedSrc = MmGetSystemAddressForMdlSafe(pSrcMdl, NormalPagePriority);
			}
			__except (1)
			{

			}
			if (pMappedSrc)
			{
				__try
				{
					RtlCopyMemory((PVOID)pMappedSrc, (PVOID)DstAddr, Size);
					status = STATUS_SUCCESS;
				}
				__except (1)
				{
					status = GetExceptionCode();
				}
			}

			MmUnlockPages(pSrcMdl);
			IoFreeMdl(pSrcMdl);

		}

		return status;
	}

	NTSTATUS ReWriteOpenProcess(ULONG dwDesiredAccess, ULONG dwProcessId, PHANDLE outHandle)
	{
		NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
		HANDLE hTempHanle = 0;
		PEPROCESS pProcess = NULL;
		if (NT_SUCCESS(PsLookupProcessByProcessId((HANDLE)dwProcessId, &pProcess)))
		{
			ntStatus = ObOpenObjectByPointer(pProcess, 0, 0, dwDesiredAccess, *PsProcessType, KernelMode, &hTempHanle);
			if (NT_SUCCESS(ntStatus))
			{
				*outHandle = hTempHanle;
			}

			ObDereferenceObject(pProcess);
		}

		return ntStatus;
	}
	NTSTATUS ReWriteOpenThread(ULONG dwDesiredAccess, ULONG dwThreadId, PHANDLE outHandle)
	{
		NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
		HANDLE hTempHanle = 0;
		PETHREAD Thread = NULL;
		if (NT_SUCCESS(PsLookupThreadByThreadId((HANDLE)dwThreadId, &Thread)))
		{
			ntStatus = ObOpenObjectByPointer(Thread, 0, 0, dwDesiredAccess, *PsThreadType, KernelMode, &hTempHanle);
			if (NT_SUCCESS(ntStatus))
			{
				*outHandle = hTempHanle;
			}

			ObDereferenceObject(Thread);
		}


		return ntStatus;
	}

	NTSTATUS SuspendOrResumeThread(HANDLE hThread, ULONG bSuspend, PULONG nCnt)
	{
		NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
		PVOID pTemp = NULL;
		ULONG n = 0;
		t_PsSuspendThread PsSuspendThread = (t_PsSuspendThread)symbol::MmGetSymbolRoutineAddress(FunctionType::ePsSuspendThread);
		t_PsResumeThread PsResumeThread = (t_PsResumeThread)symbol::MmGetSymbolRoutineAddress(FunctionType::ePsResumeThread);
		if (!MmIsAddressValid(PsSuspendThread) || !MmIsAddressValid(PsResumeThread)) return ntStatus;
		//������� �߳̾��
		ntStatus = ObReferenceObjectByHandle(
			hThread,
			0,
			*PsThreadType,
			KernelMode,
			&pTemp,
			NULL);
		if (NT_SUCCESS(ntStatus))
		{
			ntStatus = STATUS_UNSUCCESSFUL;
			if (bSuspend)
			{
				ntStatus = PsSuspendThread((PETHREAD)pTemp, &n);
			}
			else
			{
				ntStatus = PsResumeThread((PETHREAD)pTemp, &n);
			}
			if (NT_SUCCESS(ntStatus))
			{
				if (nCnt) {
					*nCnt = n;
				}
			}
		}
		if (pTemp)
		{
			ObfDereferenceObject(pTemp);
			pTemp = NULL;
		}
		return ntStatus;
	}

	NTSTATUS GetOrSetThreadContext(HANDLE hThread, PCONTEXT lpContext, BOOLEAN bGet)
	{
		NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
		PVOID pTemp = NULL;
		//������� �߳̾��
		ntStatus = ObReferenceObjectByHandle(
			hThread,
			0,
			*PsThreadType,
			KernelMode,
			&pTemp,
			NULL);
		if (NT_SUCCESS(ntStatus))
		{

			if (bGet)
			{
				ntStatus = PsGetContextThread((PKTHREAD)pTemp, lpContext, UserMode);
			}
			else
			{
				ntStatus = PsSetContextThread((PKTHREAD)pTemp, lpContext, UserMode);
			}

		}
		if (pTemp)
		{
			ObfDereferenceObject(pTemp);
			pTemp = NULL;
		}
		return ntStatus;
	}

	NTSTATUS ObDuplicateObjectD(HANDLE SourceProcessHandle, HANDLE SourceHandle, HANDLE TargetProcessHandle, PHANDLE pTargetHandle, ACCESS_MASK DesiredAccess, ULONG HandleAttributes, ULONG Options)
	{
		NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
		PVOID pSourceProcess = NULL;
		PVOID pTargetProcess = NULL;

		ntStatus = ObReferenceObjectByHandle(
			TargetProcessHandle,
			0,
			*PsProcessType,
			KernelMode,
			&pTargetProcess,
			NULL);
		ntStatus = ObReferenceObjectByHandle(
			SourceProcessHandle,
			0,
			*PsProcessType,
			KernelMode,
			&pSourceProcess,
			NULL);


		if (pSourceProcess && pTargetProcess)
		{
			HANDLE TargetHandle = NULL;

			ntStatus = ObDuplicateObject((PEPROCESS)pSourceProcess, SourceHandle, (PEPROCESS)pTargetProcess, &TargetHandle, DesiredAccess, HandleAttributes, Options, UserMode);

			if (NT_SUCCESS(ntStatus))
			{
				*pTargetHandle = TargetHandle;
			}

		}

		if (pTargetProcess)
		{
			ObfDereferenceObject(pTargetProcess);
			pTargetProcess = NULL;
		}
		if (pSourceProcess)
		{
			ObfDereferenceObject(pSourceProcess);
			pSourceProcess = NULL;
		}

		return ntStatus;
	}

	NTSTATUS SuspendProcessOrPsResumProcessByPid(ULONG nPid, ULONG bSuspend)
	{
		NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;

		PEPROCESS selectedprocess = NULL;

		ntStatus = PsLookupProcessByProcessId((HANDLE)nPid, &selectedprocess);
		if (NT_SUCCESS(ntStatus))
		{

			if (bSuspend)
			{
				ntStatus = PsSuspendProcess(selectedprocess);
			}
			else
			{
				ntStatus = PsResumeProcess(selectedprocess);
			}

			ObfDereferenceObject(selectedprocess);
		}

		return ntStatus;
	}

	NTSTATUS SuspendOrResumeThreadByPid(HANDLE nTid, ULONG bSuspend, PULONG nCnt)
	{
		NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
		PETHREAD pTemp = NULL;
		ULONG n = 0;
		t_PsSuspendThread PsSuspendThread = (t_PsSuspendThread)symbol::MmGetSymbolRoutineAddress(FunctionType::ePsSuspendThread);
		t_PsResumeThread PsResumeThread = (t_PsResumeThread)symbol::MmGetSymbolRoutineAddress(FunctionType::ePsResumeThread);
		if (!MmIsAddressValid(PsSuspendThread) || !MmIsAddressValid(PsResumeThread)) return ntStatus;
		//������� �߳̾��

		ntStatus = PsLookupThreadByThreadId(nTid, &pTemp);
		if (NT_SUCCESS(ntStatus))
		{
			ntStatus = STATUS_UNSUCCESSFUL;
			if (bSuspend)
			{
				ntStatus = PsSuspendThread(pTemp, &n);
			}
			else
			{
				ntStatus = PsResumeThread(pTemp, &n);
			}
			if (NT_SUCCESS(ntStatus))
			{
				if (nCnt) {
					*nCnt = n;
				}
			}
		}
		if (pTemp)
		{
			ObfDereferenceObject(pTemp);
			pTemp = NULL;
		}
		return ntStatus;
	}

	NTSTATUS RtlSuperCopyMemoryEx(IN VOID UNALIGNED* Destination, IN CONST VOID UNALIGNED* Source, IN ULONG Length)
	{

		if (!Destination || !Source || Length == 0)
		{
			return STATUS_UNSUCCESSFUL;
		}

		const KIRQL Irql = KeRaiseIrqlToDpcLevel();

		PMDL Mdl = IoAllocateMdl(Destination, Length, 0, 0, NULL);
		if (Mdl == NULL)
		{
			KeLowerIrql(Irql);
			return STATUS_NO_MEMORY;
		}

		MmBuildMdlForNonPagedPool(Mdl);

		// Hack: prevent bugcheck from Driver Verifier and possible future versions of Windows
#pragma prefast(push)
#pragma prefast(disable:__WARNING_MODIFYING_MDL, "Trust me I'm a scientist")
		const CSHORT OriginalMdlFlags = Mdl->MdlFlags;
		Mdl->MdlFlags |= MDL_PAGES_LOCKED;
		Mdl->MdlFlags &= ~MDL_SOURCE_IS_NONPAGED_POOL;

		// Map pages and do the copy
		const PVOID Mapped = MmMapLockedPagesSpecifyCache(Mdl, KernelMode, MmCached, NULL, FALSE, HighPagePriority);
		if (Mapped == NULL)
		{
			Mdl->MdlFlags = OriginalMdlFlags;
			IoFreeMdl(Mdl);
			KeLowerIrql(Irql);
			return STATUS_NONE_MAPPED;
		}

		RtlCopyMemory(Mapped, Source, Length);

		MmUnmapLockedPages(Mapped, Mdl);
		Mdl->MdlFlags = OriginalMdlFlags;
#pragma prefast(pop)
		IoFreeMdl(Mdl);
		KeLowerIrql(Irql);

		return STATUS_SUCCESS;
	}

	NTSTATUS FD_SetFileCompletion(
		IN PDEVICE_OBJECT DeviceObject,
		IN PIRP Irp,
		IN PVOID Context
	)
	{
		UNREFERENCED_PARAMETER(DeviceObject);
		UNREFERENCED_PARAMETER(Context);
		Irp->UserIosb->Status = Irp->IoStatus.Status;
		Irp->UserIosb->Information = Irp->IoStatus.Information;

		KeSetEvent(Irp->UserEvent, IO_NO_INCREMENT, FALSE);

		IoFreeIrp(Irp);
		return STATUS_MORE_PROCESSING_REQUIRED;
	}
	NTSTATUS DeleteFile(HANDLE FileHandle)
	{
		NTSTATUS          ntStatus = STATUS_UNSUCCESSFUL;
		PFILE_OBJECT      fileObject;
		PDEVICE_OBJECT    DeviceObject;
		PIRP              Irp;
		KEVENT            SycEvent;
		FILE_DISPOSITION_INFORMATION    FileInformation;
		IO_STATUS_BLOCK                 ioStatus;
		PIO_STACK_LOCATION              irpSp;
		PSECTION_OBJECT_POINTERS        pSectionObjectPointer;

		// ��ȡ�ļ����� 
		ntStatus = ObReferenceObjectByHandle(FileHandle, DELETE,
			*IoFileObjectType, KernelMode, (PVOID*)&fileObject, NULL);
		if (!NT_SUCCESS(ntStatus))
		{

			return ntStatus;
		}

		// ��ȡ��ָ���ļ�������������豸���� 
		DeviceObject = IoGetRelatedDeviceObject(fileObject);

		// ����IRP 
		Irp = IoAllocateIrp(DeviceObject->StackSize, TRUE);
		if (Irp == NULL)
		{
			ObDereferenceObject(fileObject);
			return ntStatus;
		}

		// ��ʼ��ͬ���¼����� 
		KeInitializeEvent(&SycEvent, SynchronizationEvent, FALSE);

		FileInformation.DeleteFile = TRUE;

		// ��ʼ��IRP 
		Irp->AssociatedIrp.SystemBuffer = &FileInformation;
		Irp->UserEvent = &SycEvent;
		Irp->UserIosb = &ioStatus;
		Irp->Tail.Overlay.OriginalFileObject = fileObject;
		Irp->Tail.Overlay.Thread = (PETHREAD)KeGetCurrentThread();
		Irp->RequestorMode = KernelMode;

		// ����IRP��ջ 
		irpSp = IoGetNextIrpStackLocation(Irp);
		irpSp->MajorFunction = IRP_MJ_SET_INFORMATION;
		irpSp->DeviceObject = DeviceObject;
		irpSp->FileObject = fileObject;
		irpSp->Parameters.SetFile.Length = sizeof(FILE_DISPOSITION_INFORMATION);
		irpSp->Parameters.SetFile.FileInformationClass = FileDispositionInformation;
		irpSp->Parameters.SetFile.FileObject = fileObject;

		// ����������� 
		IoSetCompletionRoutine(Irp, FD_SetFileCompletion, NULL, TRUE, TRUE, TRUE);

		// ���û����3�У����޷�ɾ���������е��ļ� 
		__try {
			pSectionObjectPointer = fileObject->SectionObjectPointer;
			if (MmIsAddressValid(pSectionObjectPointer)) {
				pSectionObjectPointer->ImageSectionObject = 0;
				pSectionObjectPointer->DataSectionObject = 0;
			}
		}
		__except (1) {
			ntStatus = GetExceptionCode();
		}

		// �ɷ�IRP 
		IoCallDriver(DeviceObject, Irp);

		// �ȴ�IRP��� 
		KeWaitForSingleObject(&SycEvent, Executive, KernelMode, TRUE, NULL);

		// �ݼ����ü��� 
		ObDereferenceObject(fileObject);

		return ntStatus;
	}

}