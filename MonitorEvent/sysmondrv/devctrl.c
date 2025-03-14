#include "public.h"
#include "devctrl.h"
#include "process.h"
#include "thread.h"
#include "imagemod.h"
#include "register.h"
#include "syswmi.h"
#include "sysfile.h"
#include "syssession.h"

#include "sysssdt.h"
#include "sysidt.h"
#include "sysdpctimer.h"
#include "sysenumnotify.h"
#include "sysfsd.h"
#include "sysmousekeyboard.h"
#include "sysnetwork.h"
#include "sysprocessinfo.h"
#include "sysdriverinfo.h"

#include "rProcess.h"
#include "rRegister.h"
#include "rDirectory.h"
#include "rThread.h"

#include "minifilter.h"
#include "kflt.h"
#include "utiltools.h"

#include <ntddk.h>
#include <stdlib.h>


#define NF_TCP_PACKET_BUF_SIZE 8192
#define NF_UDP_PACKET_BUF_SIZE 2 * 65536

static UNICODE_STRING g_devicename;
static UNICODE_STRING g_devicesyslink;
static PDEVICE_OBJECT g_deviceControl;
static BOOLEAN		  g_version = FALSE;

typedef struct _SHARED_MEMORY
{
    PMDL					mdl;
    PVOID					userVa;
    PVOID					kernelVa;
    UINT64					bufferLength;
} SHARED_MEMORY, * PSHARED_MEMORY;

static LIST_ENTRY               g_IoQueryHead;
static KSPIN_LOCK               g_IoQueryLock;
static NPAGED_LOOKASIDE_LIST    g_IoQueryList;
static PVOID			        g_ioThreadObject = NULL;
static KEVENT					g_ioThreadEvent;
static LIST_ENTRY				g_pendedIoRequests;
static BOOLEAN					g_shutdown = FALSE;
static BOOLEAN					g_monitorflag = FALSE;

static SHARED_MEMORY g_inBuf;
static SHARED_MEMORY g_outBuf;

typedef struct _NF_QUEUE_ENTRY
{
    LIST_ENTRY		entry;		// Linkage
    int				code;		// IO code
} NF_QUEUE_ENTRY, * PNF_QUEUE_ENTRY;

// ☆ Ark Data Collection
NTSTATUS devctrl_InitSsdtBase(PDEVICE_OBJECT DeviceObject, PIRP irp, PIO_STACK_LOCATION irpSp)
{
	NTSTATUS nStatus = STATUS_SUCCESS;
	PVOID pOutBuffer = NULL;
	pOutBuffer = irp->AssociatedIrp.SystemBuffer;
	if (!pOutBuffer)
	{
		pOutBuffer = irp->UserBuffer;
	}

	ULONG outputBufferLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

	do {

		if (!pOutBuffer && (outputBufferLength < sizeof(DWORD)))
			break;

		if (Sstd_Init())
		{
			DWORD flag = 1;
			RtlCopyMemory(pOutBuffer, &flag, sizeof(DWORD));
		}
		else
		{
			DWORD flag = 0;
			RtlCopyMemory(pOutBuffer, &flag, sizeof(DWORD));
			break;
		}

		irp->IoStatus.Status = STATUS_SUCCESS;
		irp->IoStatus.Information = sizeof(DWORD);
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;

	} while (FALSE);

	irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_UNSUCCESSFUL;
}
NTSTATUS devctrl_GetSysSsdtInfo(PDEVICE_OBJECT DeviceObject, PIRP irp, PIO_STACK_LOCATION irpSp)
{
	PVOID pOutBuffer = NULL;
	pOutBuffer = irp->AssociatedIrp.SystemBuffer;

	do
	{
		if (!pOutBuffer)
		{
			pOutBuffer = irp->UserBuffer;
		}
		if (MmIsAddressValid(pOutBuffer) == FALSE)
			break;

		ULONG outputBufferLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

		if (!pOutBuffer && (outputBufferLength < 0x2000))
			break;

		if (!Sstd_GetTableInfo(pOutBuffer))
			break;

		irp->IoStatus.Status = STATUS_SUCCESS;
		irp->IoStatus.Information = outputBufferLength;
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;

	} while (FALSE);

	irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_UNSUCCESSFUL;
}
NTSTATUS devctrl_InitIdtBase(PDEVICE_OBJECT DeviceObject, PIRP irp, PIO_STACK_LOCATION irpSp)
{
	NTSTATUS nStatus = STATUS_SUCCESS;
	PVOID pOutBuffer = NULL;
	pOutBuffer = irp->AssociatedIrp.SystemBuffer;
	if (!pOutBuffer)
	{
		pOutBuffer = irp->UserBuffer;
	}

	ULONG outputBufferLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

	do {

		if (!pOutBuffer && (outputBufferLength < sizeof(DWORD)))
			break;

		if (nf_IdtInit())
		{
			DWORD flag = 1;
			RtlCopyMemory(pOutBuffer, &flag, sizeof(DWORD));
		}
		else
		{
			DWORD flag = 0;
			RtlCopyMemory(pOutBuffer, &flag, sizeof(DWORD));
			break;
		}

		irp->IoStatus.Status = STATUS_SUCCESS;
		irp->IoStatus.Information = sizeof(DWORD);
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;

	} while (FALSE);

	irp->IoStatus.Status = STATUS_SUCCESS;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}
NTSTATUS devctrl_GetSysIdtInfo(PDEVICE_OBJECT DeviceObject, PIRP irp, PIO_STACK_LOCATION irpSp)
{
	PVOID pOutBuffer = NULL;
	pOutBuffer = irp->AssociatedIrp.SystemBuffer;

	do {

		if (!pOutBuffer)
		{
			pOutBuffer = irp->UserBuffer;
		}
		if (MmIsAddressValid(pOutBuffer) == FALSE)
			break;

		ULONG outputBufferLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

		if (!pOutBuffer && (outputBufferLength < (sizeof(IDTINFO) * 0x100)))
			break;

#ifdef _WIN64
		if (!nf_GetIdtTableInfo(pOutBuffer))
			break;
#else

#endif

		irp->IoStatus.Status = STATUS_SUCCESS;
		irp->IoStatus.Information = outputBufferLength;
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;

	} while (FALSE);

	irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_UNSUCCESSFUL;
}
NTSTATUS devctrl_GetDpcTimerInfo(PDEVICE_OBJECT DeviceObject, PIRP irp, PIO_STACK_LOCATION irpSp)
{
	PVOID pOutBuffer = NULL;
	pOutBuffer = irp->AssociatedIrp.SystemBuffer;

	do {

		if (!pOutBuffer)
		{
			pOutBuffer = irp->UserBuffer;
		}
		if (MmIsAddressValid(pOutBuffer) == FALSE)
			break;

		ULONG outputBufferLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

		if (!pOutBuffer && (outputBufferLength < (sizeof(DPC_TIMERINFO) * 0x100)))
			break;


		nf_GetDpcTimerInfoData(pOutBuffer);

		irp->IoStatus.Status = STATUS_SUCCESS;
		irp->IoStatus.Information = outputBufferLength;
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;

	} while (FALSE);

	irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_UNSUCCESSFUL;
}
NTSTATUS devctrl_GetSysNotify(PDEVICE_OBJECT DeviceObject, PIRP irp, PIO_STACK_LOCATION irpSp)
{
	PVOID pOutBuffer = NULL;
	pOutBuffer = irp->AssociatedIrp.SystemBuffer;

	do {

		if (!pOutBuffer)
		{
			pOutBuffer = irp->UserBuffer;
		}
		if (MmIsAddressValid(pOutBuffer) == FALSE)
			break;

		ULONG outputBufferLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

		if (!pOutBuffer && (outputBufferLength <= 0))
			break;

		Enum_ProcessNotify(pOutBuffer);
		Enum_ThreadNotify((ULONG64)pOutBuffer + (ULONG64)(sizeof(PNOTIFY_INFO) * 64));
		Enum_ImageModNotify((ULONG64)pOutBuffer + (ULONG64)(sizeof(PNOTIFY_INFO) * 72));
		// xxxxx((ULONG64)pOutBuffer + (ULONG64)(sizeof(PNOTIFY_INFO) * 80));

		irp->IoStatus.Status = STATUS_SUCCESS;
		irp->IoStatus.Information = outputBufferLength;
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;
	} while (FALSE);

	irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_UNSUCCESSFUL;
}
NTSTATUS devctrl_GetSysFsdInfo(PDEVICE_OBJECT DeviceObject, PIRP irp, PIO_STACK_LOCATION irpSp)
{
	PVOID pOutBuffer = NULL;
	pOutBuffer = irp->AssociatedIrp.SystemBuffer;

	do {

		if (!pOutBuffer)
		{
			pOutBuffer = irp->UserBuffer;
		}
		if (MmIsAddressValid(pOutBuffer) == FALSE)
			break;

		ULONG outputBufferLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

		if (!pOutBuffer && (outputBufferLength < 0x1b * sizeof(ULONGLONG)))
			break;

		if (nf_fsdinit())
		{
			nf_GetfsdData(pOutBuffer);
			nf_fsdfree();
		}
		else
			break;
		
		irp->IoStatus.Status = STATUS_SUCCESS;
		irp->IoStatus.Information = outputBufferLength;
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;
	} while (FALSE);

	irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_UNSUCCESSFUL;
}
NTSTATUS devctrl_GetSysMouseKeyBoardInfo(PDEVICE_OBJECT DeviceObject, PIRP irp, PIO_STACK_LOCATION irpSp)
{
	PVOID pOutBuffer = NULL;
	pOutBuffer = irp->AssociatedIrp.SystemBuffer;

	do {

		if (!pOutBuffer)
		{
			pOutBuffer = irp->UserBuffer;
		}
		if (MmIsAddressValid(pOutBuffer) == FALSE)
			break;

		ULONG outputBufferLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

		if (!pOutBuffer && (outputBufferLength < 0x1b * sizeof(ULONGLONG)))
			break;

		if (nf_mousKeyboardInit())
		{
			nf_GetmousKeyboardInfoData(pOutBuffer);
			nf_mouskeyboardfree();
		}
		else
			break;

		irp->IoStatus.Status = STATUS_SUCCESS;
		irp->IoStatus.Information = outputBufferLength;
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;

	} while (FALSE);

	irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_UNSUCCESSFUL;

}
NTSTATUS devctrl_GetNetworkProcessInfo(PDEVICE_OBJECT DeviceObject, PIRP irp, PIO_STACK_LOCATION irpSp)
{
	PVOID pOutBuffer = NULL;
	pOutBuffer = irp->AssociatedIrp.SystemBuffer;

	do {

		if (!pOutBuffer)
		{
			pOutBuffer = irp->UserBuffer;
		}
		if (MmIsAddressValid(pOutBuffer) == FALSE)
			break;

		ULONG outputBufferLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

		if (!pOutBuffer == FALSE && (outputBufferLength <= 0))
			break;

		nf_GetNetworkIpProcessInfo(pOutBuffer);

		irp->IoStatus.Status = STATUS_SUCCESS;
		irp->IoStatus.Information = outputBufferLength;
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;

	} while (FALSE);

	irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_UNSUCCESSFUL;
}
NTSTATUS devctrl_EnumProcessInfo(PDEVICE_OBJECT DeviceObject, PIRP irp, PIO_STACK_LOCATION irpSp)
{
	PVOID pOutBuffer = NULL;
	pOutBuffer = irp->AssociatedIrp.SystemBuffer;

	do {

		if (!pOutBuffer)
		{
			pOutBuffer = irp->UserBuffer;
		}
		if (MmIsAddressValid(pOutBuffer) == FALSE)
			break;

		ULONG outputBufferLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

		if (!pOutBuffer == FALSE && (outputBufferLength <= 0))
			break;

		DWORD Pids = 0;
		switch (irpSp->Parameters.DeviceIoControl.IoControlCode)
		{
		case CTL_DEVCTRL_ARK_PROCESSINFO:
		{
			Pids = *(DWORD*)pOutBuffer;
			if (Pids > 4 && Pids < 65532)
				nf_GetProcessInfo(0, (HANDLE)Pids, (PHANDLE_INFO)pOutBuffer);
		}
		break;
		case CTL_DEVCTRL_ARK_PROCESSENUM:
			nf_GetProcessInfo(1, (HANDLE)0, (PHANDLE_INFO)pOutBuffer);
			break;
		}
		irp->IoStatus.Status = STATUS_SUCCESS;
		irp->IoStatus.Information = outputBufferLength;
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;

	} while (FALSE);

	irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_UNSUCCESSFUL;
}
NTSTATUS devctrl_GetProcessMod(PDEVICE_OBJECT DeviceObject, PIRP irp, PIO_STACK_LOCATION irpSp)
{
	PVOID pOutBuffer = NULL;
	pOutBuffer = irp->AssociatedIrp.SystemBuffer;

	do {

		if (!pOutBuffer)
		{
			pOutBuffer = irp->UserBuffer;
		}
		if (MmIsAddressValid(pOutBuffer) == FALSE)
			break;

		ULONG outputBufferLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

		if (!pOutBuffer == FALSE && (outputBufferLength <= 0))
			break;

		DWORD Pids = *(DWORD*)pOutBuffer;
		if (Pids > 4 && Pids < 65532)
			nf_EnumModuleByPid(Pids, pOutBuffer);
		else
			break;

		irp->IoStatus.Status = STATUS_SUCCESS;
		irp->IoStatus.Information = outputBufferLength;
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;

	} while (FALSE);

	irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_UNSUCCESSFUL;
}
NTSTATUS devctrl_GetKillProcess(PDEVICE_OBJECT DeviceObject, PIRP irp, PIO_STACK_LOCATION irpSp)
{
	PVOID pOutBuffer = NULL;
	pOutBuffer = irp->AssociatedIrp.SystemBuffer;

	do {

		if (!pOutBuffer)
		{
			pOutBuffer = irp->UserBuffer;
		}
		if (MmIsAddressValid(pOutBuffer) == FALSE)
			break;

		ULONG outputBufferLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

		if (!pOutBuffer == FALSE && (outputBufferLength <= 0))
			break;

		DWORD Pids = *(DWORD*)pOutBuffer;
		if (Pids > 4 && Pids < 65532)
			nf_KillProcess(Pids);
		else
			break;

		irp->IoStatus.Status = STATUS_SUCCESS;
		irp->IoStatus.Information = outputBufferLength;
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;

	} while (FALSE);

	irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_UNSUCCESSFUL;

}
NTSTATUS devctrl_DumpProcessMemory(PDEVICE_OBJECT DeviceObject, PIRP irp, PIO_STACK_LOCATION irpSp)
{
	PVOID pOutBuffer = NULL;
	pOutBuffer = irp->AssociatedIrp.SystemBuffer;

	do {

		if (!pOutBuffer)
		{
			pOutBuffer = irp->UserBuffer;
		}
		if (MmIsAddressValid(pOutBuffer) == FALSE)
			break;

		ULONG outputBufferLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

		if (!pOutBuffer == FALSE && (outputBufferLength <= 0))
			break;

		nf_DumpProcess(pOutBuffer);

		irp->IoStatus.Status = STATUS_SUCCESS;
		irp->IoStatus.Information = outputBufferLength;
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;

	} while (FALSE);

	irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_UNSUCCESSFUL;
}
NTSTATUS devctrl_DrDevEnum(PDEVICE_OBJECT DeviceObject, PIRP irp, PIO_STACK_LOCATION irpSp)
{
	PVOID pOutBuffer = NULL;
	pOutBuffer = irp->AssociatedIrp.SystemBuffer;

	do {

		if (!pOutBuffer)
		{
			pOutBuffer = irp->UserBuffer;
		}
		if (MmIsAddressValid(pOutBuffer) == FALSE)
			break;

		ULONG outputBufferLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

		if (!pOutBuffer == FALSE && (outputBufferLength <= 0))
			break;
		memset(pOutBuffer, 0, sizeof(PROCESS_MOD) * 1024 * 2);
		nf_EnumSysDriver(DeviceObject, pOutBuffer);

		irp->IoStatus.Status = STATUS_SUCCESS;
		irp->IoStatus.Information = outputBufferLength;
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;

	} while (FALSE);

	irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_UNSUCCESSFUL;
}

// ☆ Share Memory MDL
VOID devctrl_freeSharedMemoryThread(_In_ PVOID Parameter)
{
	PSHARED_MEMORY pSharedMemory = (PSHARED_MEMORY)Parameter;
	if (pSharedMemory) {
		if (pSharedMemory->mdl)
		{
			__try
			{
				if (pSharedMemory->userVa)
				{
					MmUnmapLockedPages(pSharedMemory->userVa, pSharedMemory->mdl);
				}
				if (pSharedMemory->kernelVa)
				{
					MmUnmapLockedPages(pSharedMemory->kernelVa, pSharedMemory->mdl);
				}
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
			}

			MmFreePagesFromMdl(pSharedMemory->mdl);
			IoFreeMdl(pSharedMemory->mdl);
			pSharedMemory->mdl = NULL;
			memset(pSharedMemory, 0, sizeof(SHARED_MEMORY));
		}
	}
	PsTerminateSystemThread(STATUS_SUCCESS);
}
void devctrl_freeSharedMemory(PSHARED_MEMORY pSharedMemory)
{
	if (KeGetCurrentIrql() > APC_LEVEL)
	{
		HANDLE threadHandle = NULL;
		NTSTATUS status = STATUS_SUCCESS;
		status = PsCreateSystemThread(
			&threadHandle,
			THREAD_ALL_ACCESS,
			NULL,
			NULL,
			NULL,
			devctrl_freeSharedMemoryThread,
			pSharedMemory
		);

		if (NT_SUCCESS(status) && threadHandle)
		{
			KPRIORITY priority = HIGH_PRIORITY;
			ZwSetInformationThread(threadHandle, ThreadPriority, &priority, sizeof(priority));
			ZwClose(threadHandle);
		}
		return;
	}
	if (pSharedMemory->mdl)
	{
		__try
		{
			if (pSharedMemory->userVa)
			{
				MmUnmapLockedPages(pSharedMemory->userVa, pSharedMemory->mdl);
			}
			if (pSharedMemory->kernelVa)
			{
				MmUnmapLockedPages(pSharedMemory->kernelVa, pSharedMemory->mdl);
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}

		MmFreePagesFromMdl(pSharedMemory->mdl);
		IoFreeMdl(pSharedMemory->mdl);

		memset(pSharedMemory, 0, sizeof(SHARED_MEMORY));
	}
}
NTSTATUS devctrl_createSharedMemory(PSHARED_MEMORY pSharedMemory, UINT64 len)
{
	PMDL  pMdl = NULL;
	PVOID userVa = NULL;
	PVOID kernelVa = NULL;
	PHYSICAL_ADDRESS lowAddress;
	PHYSICAL_ADDRESS highAddress;

	memset(pSharedMemory, 0, sizeof(SHARED_MEMORY));

	lowAddress.QuadPart = 0;
	highAddress.QuadPart = 0xFFFFFFFFFFFFFFFF;

	pMdl = MmAllocatePagesForMdl(lowAddress, highAddress, lowAddress, (SIZE_T)len);
	if (pMdl == NULL || (!pMdl))
		return STATUS_INSUFFICIENT_RESOURCES;

	__try
	{
		kernelVa = VerifiMmGetSystemAddressForMdlSafe(pMdl, HighPagePriority);
		if (!kernelVa)
		{
			MmFreePagesFromMdl(pMdl);
			IoFreeMdl(pMdl);
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		//
		// The preferred way to map the buffer into user space
		//
#if (NTDDI_VERSION >= NTDDI_WIN8)
		userVa = MmMapLockedPagesSpecifyCache(pMdl,          // MDL
			UserMode,     // Mode
			MmCached,     // Caching
			NULL,         // Address
			FALSE,        // Bugcheck?
			HighPagePriority | MdlMappingNoExecute); // Priority
#else
		userVa = MmMapLockedPagesSpecifyCache(pMdl,          // MDL
			UserMode,     // Mode
			MmCached,     // Caching
			NULL,         // Address
			FALSE,        // Bugcheck?
			HighPagePriority); // Priority
#endif
		if (!userVa)
		{
			MmUnmapLockedPages(kernelVa, pMdl);
			MmFreePagesFromMdl(pMdl);
			IoFreeMdl(pMdl);
			return STATUS_INSUFFICIENT_RESOURCES;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}

	//
	// If we get NULL back, the request didn't work.
	// I'm thinkin' that's better than a bug check anyday.
	//
	if (!userVa || !kernelVa)
	{
		if (userVa)
		{
			MmUnmapLockedPages(userVa, pMdl);
		}
		if (kernelVa)
		{
			MmUnmapLockedPages(kernelVa, pMdl);
		}
		MmFreePagesFromMdl(pMdl);
		IoFreeMdl(pMdl);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//
	// Return the allocated pointers
	//
	pSharedMemory->mdl = pMdl;
	pSharedMemory->userVa = userVa;
	pSharedMemory->kernelVa = kernelVa;
	pSharedMemory->bufferLength = MmGetMdlByteCount(pMdl);

	return STATUS_SUCCESS;
}
NTSTATUS devctrl_openMem(PDEVICE_OBJECT DeviceObject, PIRP irp, PIO_STACK_LOCATION irpSp)
{
	PVOID ioBuffer = NULL;
	ioBuffer = irp->AssociatedIrp.SystemBuffer;
	if (ioBuffer == NULL || (!ioBuffer))
		ioBuffer = irp->UserBuffer;

	ULONG outputBufferLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

	if (ioBuffer && (outputBufferLength >= sizeof(NF_BUFFERS)))
	{
		NTSTATUS 	status;

		for (;;)
		{
			if (!g_inBuf.mdl)
			{
				status = devctrl_createSharedMemory(&g_inBuf, NF_UDP_PACKET_BUF_SIZE * 50);
				if (!NT_SUCCESS(status))
				{
					break;
				}
			}

			if (!g_outBuf.mdl)
			{
				status = devctrl_createSharedMemory(&g_outBuf, NF_UDP_PACKET_BUF_SIZE * 2);
				if (!NT_SUCCESS(status))
				{
					break;
				}
			}

			status = STATUS_SUCCESS;

			break;
		}

		if (!NT_SUCCESS(status))
		{
			devctrl_freeSharedMemory(&g_inBuf);
			devctrl_freeSharedMemory(&g_outBuf);
		}
		else
		{
			PNF_BUFFERS pBuffers = (PNF_BUFFERS)ioBuffer;

			pBuffers->inBuf = (UINT64)g_inBuf.userVa;
			pBuffers->inBufLen = g_inBuf.bufferLength;
			pBuffers->outBuf = (UINT64)g_outBuf.userVa;
			pBuffers->outBufLen = g_outBuf.bufferLength;

			irp->IoStatus.Status = STATUS_SUCCESS;
			irp->IoStatus.Information = sizeof(NF_BUFFERS);
			IoCompleteRequest(irp, IO_NO_INCREMENT);

			return STATUS_SUCCESS;
		}
	}

	irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_UNSUCCESSFUL;
}

// ☆ IOCTL
NTSTATUS devctrl_create(PIRP irp, PIO_STACK_LOCATION irpSp)
{
	KLOCK_QUEUE_HANDLE lh;
	NTSTATUS 	status = STATUS_SUCCESS;
	HANDLE		pid = PsGetCurrentProcessId();

	UNREFERENCED_PARAMETER(irpSp);

	irp->IoStatus.Information = 0;
	irp->IoStatus.Status = status;
	IoCompleteRequest(irp, IO_NO_INCREMENT);

	return status;
}
VOID devctrl_cancelRead(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
{
	KLOCK_QUEUE_HANDLE lh;

	UNREFERENCED_PARAMETER(deviceObject);

	IoReleaseCancelSpinLock(irp->CancelIrql);

	sl_lock(&g_IoQueryLock, &lh);

	RemoveEntryList(&irp->Tail.Overlay.ListEntry);

	sl_unlock(&lh);

	irp->IoStatus.Status = STATUS_CANCELLED;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
}
ULONG devctrl_processRequest(ULONG bufferSize)
{
	PNF_DATA pData = (PNF_DATA)g_outBuf.kernelVa;

	if (bufferSize < (sizeof(NF_DATA) + pData->bufferSize - 1))
	{
		return 0;
	}

	switch (pData->code)
	{
	default:
		break;
	}
	return 0;
}
NTSTATUS devctrl_read(PIRP irp, PIO_STACK_LOCATION irpSp)
{
	NTSTATUS status = STATUS_SUCCESS;
	KLOCK_QUEUE_HANDLE lh;

	for (;;)
	{
		if (irp->MdlAddress == NULL)
		{
			KdPrint((DPREFIX"devctrl_read: NULL MDL address\n"));
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		if (VerifiMmGetSystemAddressForMdlSafe(irp->MdlAddress, NormalPagePriority) == NULL ||
			irpSp->Parameters.Read.Length < sizeof(NF_READ_RESULT))
		{
			KdPrint((DPREFIX"devctrl_read: Invalid request\n"));
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		sl_lock(&g_IoQueryLock, &lh);

		IoSetCancelRoutine(irp, devctrl_cancelRead);

		if (irp->Cancel &&
			IoSetCancelRoutine(irp, NULL))
		{
			status = STATUS_CANCELLED;
		}
		else
		{
			// pending请求
			IoMarkIrpPending(irp);
			InsertTailList(&g_pendedIoRequests, &irp->Tail.Overlay.ListEntry);
			status = STATUS_PENDING;
		}

		sl_unlock(&lh);

		// 激活处理事件
		KeSetEvent(&g_ioThreadEvent, IO_NO_INCREMENT, FALSE);

		break;
	}

	if (status != STATUS_PENDING)
	{
		irp->IoStatus.Information = 0;
		irp->IoStatus.Status = status;
		IoCompleteRequest(irp, IO_NO_INCREMENT);
	}

	return status;
}
NTSTATUS devctrl_write(PIRP irp, PIO_STACK_LOCATION irpSp)
{
	PNF_READ_RESULT pRes;
	ULONG bufferLength = irpSp->Parameters.Write.Length;

	pRes = (PNF_READ_RESULT)VerifiMmGetSystemAddressForMdlSafe(irp->MdlAddress, NormalPagePriority);
	if (!pRes || bufferLength < sizeof(NF_READ_RESULT))
	{
		irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		KdPrint((DPREFIX"devctrl_write invalid irp\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	irp->IoStatus.Information = devctrl_processRequest((ULONG)pRes->length);
	irp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}
NTSTATUS devctrl_close(PIRP irp, PIO_STACK_LOCATION irpSp)
{
	KLOCK_QUEUE_HANDLE lh;
	NTSTATUS 	status = STATUS_SUCCESS;
	HANDLE		pid = PsGetCurrentProcessId();

	UNREFERENCED_PARAMETER(irpSp);
	devctrl_setMonitor(FALSE);
	devctrl_setIpsMonitor(FALSE);
	utiltools_sleep(1000);
	Process_Clean();
	Thread_Clean();
	Imagemod_Clean();
	Register_Clean();
	if (g_version)
		File_Clean();
	Wmi_Clean();
	Session_Clean();

	devctrl_clean();
	devctrl_freeSharedMemory(&g_inBuf);
	devctrl_freeSharedMemory(&g_outBuf);

	irp->IoStatus.Information = 0;
	irp->IoStatus.Status = status;
	IoCompleteRequest(irp, IO_NO_INCREMENT);

	return status;
}

void devctrl_cancelPendingReads()
{
	PIRP                irp;
	PLIST_ENTRY         pIrpEntry;
	KLOCK_QUEUE_HANDLE lh;

	sl_lock(&g_IoQueryLock, &lh);

	while (!IsListEmpty(&g_pendedIoRequests))
	{
		//
		//  Get the first pended Read IRP
		//
		pIrpEntry = g_pendedIoRequests.Flink;
		irp = CONTAINING_RECORD(pIrpEntry, IRP, Tail.Overlay.ListEntry);

		//
		//  Check to see if it is being cancelled.
		//
		if (IoSetCancelRoutine(irp, NULL))
		{
			//
			//  It isn't being cancelled, and can't be cancelled henceforth.
			//
			RemoveEntryList(pIrpEntry);

			sl_unlock(&lh);

			//
			//  Complete the IRP.
			//
			irp->IoStatus.Status = STATUS_CANCELLED;
			irp->IoStatus.Information = 0;
			IoCompleteRequest(irp, IO_NO_INCREMENT);

			sl_lock(&g_IoQueryLock, &lh);
		}
		else
		{
			//
			//  It is being cancelled, let the cancel routine handle it.
			//
			sl_unlock(&lh);

			//
			//  Give the cancel routine some breathing space, otherwise
			//  we might end up examining the same (cancelled) IRP over
			//  and over again.
			//
			utiltools_sleep(1000);

			sl_lock(&g_IoQueryLock, &lh);
		}
	}

	sl_unlock(&lh);
}
VOID devctrl_clean()
{
	PNF_QUEUE_ENTRY pQuery = NULL;
	KLOCK_QUEUE_HANDLE lh;
	sl_lock(&g_IoQueryLock, &lh);
	while (!IsListEmpty(&g_IoQueryHead))
	{
		pQuery = RemoveHeadList(&g_IoQueryHead);
		sl_unlock(&lh);

		ExFreeToNPagedLookasideList(&g_IoQueryList, pQuery);
		pQuery = NULL;
		sl_lock(&g_IoQueryLock, &lh);
	}
	sl_unlock(&lh);

	// clearn pennding read i/o
	devctrl_cancelPendingReads();
}
VOID devctrl_free()
{
	if (g_deviceControl)
	{
		IoDeleteDevice(g_deviceControl);
		g_deviceControl = NULL;
		IoDeleteSymbolicLink(&g_devicesyslink);
	}
	devctrl_setShutdown();
	devctrl_setMonitor(FALSE);
	Process_Free();
	Thread_Free();
	Imagemod_Free();
	Register_Free();
	if(g_version)
		File_Free();
	Wmi_Free();
	Session_Free();
}
VOID devctrl_setShutdown()
{
	KLOCK_QUEUE_HANDLE lh;

	sl_lock(&g_IoQueryLock, &lh);
	g_shutdown = TRUE;
	sl_unlock(&lh);
}
BOOLEAN	devctrl_isShutdown()
{
	BOOLEAN		res;
	KLOCK_QUEUE_HANDLE lh;

	sl_lock(&g_IoQueryLock, &lh);
	res = g_shutdown;
	sl_unlock(&lh);

	return res;
}
VOID devctrl_setMonitor(BOOLEAN code)
{
	KLOCK_QUEUE_HANDLE lh;

	sl_lock(&g_IoQueryLock, &lh);
	// estable monitor
	Process_SetMonitor(code);
	Thread_SetMonitor(code);
	Imagemod_SetMonitor(code);
	Register_SetMonitor(code);
	Wmi_SetMonitor(code);
	File_SetMonitor(code);
	Session_SetMonitor(code);
	sl_unlock(&lh);
	
	// clearn pennding read i/o
	if (FALSE == code)
		utiltools_sleep(1000);

	devctrl_cancelPendingReads();
}
VOID devctrl_setIpsMonitor(BOOLEAN code)
{
	KLOCK_QUEUE_HANDLE lh;

	sl_lock(&g_IoQueryLock, &lh);
	// estable Ips monitor
	Process_SetIpsMonitor(code);
	Thread_SetIpsMonitor(code);
	Imagemod_SetIpsMonitor(code);
	Register_SetIpsMonitor(code);
	Wmi_SetIpsMonitor(code);
	Session_SetIpsMonitor(code);
	// minifilter Ips monitor
	FsFlt_SetDirectoryIpsMonitor(code);
	sl_unlock(&lh);

	devctrl_cancelPendingReads();
}
NTSTATUS devctrl_dispatch(IN PDEVICE_OBJECT DeviceObject, IN PIRP irp)
{
	NTSTATUS status = STATUS_SUCCESS;
	PIO_STACK_LOCATION irpSp;
	irpSp = IoGetCurrentIrpStackLocation(irp);
	ASSERT(irpSp);

	switch (irpSp->MajorFunction)
	{
	case IRP_MJ_CREATE:
		return devctrl_create(irp, irpSp);

	case IRP_MJ_READ:
	{
		return devctrl_read(irp, irpSp);
	}

	case IRP_MJ_WRITE:
		return devctrl_write(irp, irpSp);

	case IRP_MJ_CLOSE:
	{
		return devctrl_close(irp, irpSp);
	}

	case IRP_MJ_DEVICE_CONTROL:
		switch (irpSp->Parameters.DeviceIoControl.IoControlCode)
		{
		// 共享内存
		case CTL_DEVCTRL_OPEN_SHAREMEM:
			return devctrl_openMem(DeviceObject, irp, irpSp);
		// 内核监控开
		case CTL_DEVCTRL_ENABLE_MONITOR:
			devctrl_setMonitor(TRUE);
			break;
		// 内核监控关
		case CTL_DEVCTRL_DISENTABLE_MONITOR:
			devctrl_setMonitor(FALSE);
			break;
		// Ips内核监控开
		case CTL_DEVCTRL_ENABLE_IPS_MONITOR:
			devctrl_setIpsMonitor(TRUE);
			break;
		// Ips内核监控开
		case CTL_DEVCTRL_DISENTABLE_IPS_MONITOR:
			devctrl_setIpsMonitor(FALSE);
			break;
		// Process Ips: 进程名列表
		case CTL_DEVCTRL_IPS_SETPROCESSNAME:
		{
			Process_SetIpsMonitor(FALSE);
			Process_SetIpsModEx(0);
			utiltools_sleep(500);
			return rProcess_SetIpsProcessName(irp, irpSp);
		}
		// Process Ips: 模式
		case CTL_DEVCTRL_IPS_SETPROCESSFILTERMOD:
			return Process_SetIpsMod(irp, irpSp);
		// Register Ips: 进程名列表
		case CTL_DEVCTRL_IPS_SETREGISTERNAME:
		{
			Register_SetIpsMonitor(FALSE);
			utiltools_sleep(500);
			return rRegister_SetIpsProcessName(irp, irpSp);
		}
		// Directory Ips: 进程名/目录列表
		case CTL_DEVCTRL_IPS_SETDIRECTORYRULE:
		{
			FsFlt_SetDirectoryIpsMonitor(FALSE);
			utiltools_sleep(500);
			return rDirectory_SetIpsDirectRule(irp, irpSp);
		}
		// ThreadInject Ips: 进程名
		case CTL_DEVCTRL_IPS_SETTHREADINJECTNAME:
		{			
			Thread_SetIpsMonitor(FALSE);
			utiltools_sleep(500);
			return rThrInject_SetIpsProcessName(irp, irpSp);
		}
		// Rootkit Data
		case CTL_DEVCTRL_ARK_INITSSDT:
			return devctrl_InitSsdtBase(DeviceObject, irp, irpSp);
		case CTL_DEVCTRL_ARK_GETSSDTDATA:
			return devctrl_GetSysSsdtInfo(DeviceObject, irp, irpSp);

		case CTL_DEVCTRL_ARK_INITIDT:
			return devctrl_InitIdtBase(DeviceObject, irp, irpSp);
		case CTL_DEVCTRL_ARK_GETIDTDATA:
			return devctrl_GetSysIdtInfo(DeviceObject, irp, irpSp);

		case CTL_DEVCTRL_ARK_GETDPCTIMERDATA:
			return devctrl_GetDpcTimerInfo(DeviceObject, irp, irpSp);
		case CTL_DEVCTRL_ARK_GETSYSENUMNOTIFYDATA:
			return devctrl_GetSysNotify(DeviceObject, irp, irpSp);

		case CTL_DEVCTRL_ARK_GETSYSFSDDATA:
			return devctrl_GetSysFsdInfo(DeviceObject, irp, irpSp);

		case CTL_DEVCTRL_ARK_GETSYSMOUSEKEYBOARDDATA:
			return devctrl_GetSysMouseKeyBoardInfo(DeviceObject, irp, irpSp);

		case CTL_DEVCTRL_ARK_GETSYNETWORKDDATA:
			return devctrl_GetNetworkProcessInfo(DeviceObject, irp, irpSp);

		case CTL_DEVCTRL_ARK_PROCESSENUM:
		case CTL_DEVCTRL_ARK_PROCESSINFO:
			return devctrl_EnumProcessInfo(DeviceObject, irp, irpSp);
		case CTL_DEVCTRL_ARK_PROCESSMOD:
			return devctrl_GetProcessMod(DeviceObject, irp, irpSp);
		case CTL_DEVCTRL_ARK_PROCESSKILL:
			return devctrl_GetKillProcess(DeviceObject, irp, irpSp);
		case CTL_DEVCTRL_ARK_PROCESSDUMP:
			return devctrl_DumpProcessMemory(DeviceObject, irp, irpSp);

		case CTL_DEVCTRL_ARK_DRIVERDEVENUM:
			return devctrl_DrDevEnum(DeviceObject, irp, irpSp);
		}
		break;
	}

	irp->IoStatus.Status = STATUS_SUCCESS;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}
NTSTATUS devctrl_ioInit(PDRIVER_OBJECT DriverObject) {
	NTSTATUS status = STATUS_SUCCESS;

	// Create Device
	RtlInitUnicodeString(&g_devicename, L"\\Device\\Sysmondrv_hades");
	RtlInitUnicodeString(&g_devicesyslink, L"\\DosDevices\\Sysmondrv_hades");
	status = IoCreateDevice(
		DriverObject,
		0,
		&g_devicename,
		FILE_DEVICE_UNKNOWN,
		FILE_DEVICE_SECURE_OPEN,
		FALSE,
		&g_deviceControl);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	g_deviceControl->Flags &= ~DO_DEVICE_INITIALIZING;

	status = IoCreateSymbolicLink(&g_devicesyslink, &g_devicename);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	g_deviceControl->Flags &= ~DO_DEVICE_INITIALIZING;
	g_deviceControl->Flags |= DO_DIRECT_IO;

	InitializeListHead(&g_pendedIoRequests);
	InitializeListHead(&g_IoQueryHead);
	KeInitializeSpinLock(&g_IoQueryLock);

	VerifiExInitializeNPagedLookasideList(
		&g_IoQueryList,
		NULL,
		NULL,
		0,
		sizeof(NF_QUEUE_ENTRY),
		'IOMM',
		0
	);

	HANDLE threadHandle;
	KeInitializeEvent(
		&g_ioThreadEvent,
		SynchronizationEvent,
		FALSE
	);

	status = PsCreateSystemThread(
		&threadHandle,
		THREAD_ALL_ACCESS,
		NULL,
		NULL,
		NULL,
		devctrl_ioThread,
		NULL
	);

	// if success callback
	if (NT_SUCCESS(status))
	{
		KPRIORITY priority = HIGH_PRIORITY;

		ZwSetInformationThread(threadHandle, ThreadPriority, &priority, sizeof(priority));

		status = ObReferenceObjectByHandle(
			threadHandle,
			0,
			NULL,
			KernelMode,
			&g_ioThreadObject,
			NULL
		);
		ASSERT(NT_SUCCESS(status));
	}
	return status;
}
void devctrl_ioThreadFree() {

	devctrl_clean();
	ExDeleteNPagedLookasideList(&g_IoQueryList);

	// clsoe process callback
	if (g_ioThreadObject) {
		// 标记卸载驱动-跳出循环
		KeSetEvent(&g_ioThreadEvent, IO_NO_INCREMENT, FALSE);

		KeWaitForSingleObject(
			g_ioThreadObject,
			Executive,
			KernelMode,
			FALSE,
			NULL
		);

		ObDereferenceObject(g_ioThreadObject);
		g_ioThreadObject = NULL;
	}
}

// ☆ System Active Monitor
NTSTATUS devctrl_popprocessinfo(UINT64* pOffset)
{
	NTSTATUS			status = STATUS_SUCCESS;
	KLOCK_QUEUE_HANDLE	lh;
	PROCESSBUFFER		*processbuffer = NULL;
	PROCESSDATA			*processdata = NULL;
	PNF_DATA			pData;
	UINT64				dataSize = 0;
	ULONG				pPacketlens = 0;

	processdata = (PROCESSDATA*)processctx_get();
	if (!processdata)
		return STATUS_UNSUCCESSFUL;

	sl_lock(&processdata->process_lock, &lh);

	while (!IsListEmpty(&processdata->process_pending))
	{
		processbuffer = (PROCESSBUFFER*)RemoveHeadList(&processdata->process_pending);

		pPacketlens = processbuffer->dataLength;

		dataSize = sizeof(NF_DATA) - 1 + pPacketlens;

		if ((g_inBuf.bufferLength - *pOffset - 1) < dataSize)
		{
			status = STATUS_NO_MEMORY;
			break;
		}

		pData = (PNF_DATA)((char*)g_inBuf.kernelVa + *pOffset);

		pData->code = NF_PROCESS_INFO;
		pData->id = 1;
		pData->bufferSize = processbuffer->dataLength;

		if (processbuffer->dataBuffer) {
			memcpy(pData->buffer, processbuffer->dataBuffer, processbuffer->dataLength);
		}

		*pOffset += dataSize;

		break;
	}

	sl_unlock(&lh);

	if (processbuffer)
	{
		if (NT_SUCCESS(status))
		{
			Process_PacketFree(processbuffer);
			processbuffer = NULL;
		}
		else
		{
			sl_lock(&processdata->process_lock, &lh);
			InsertHeadList(&processdata->process_pending, &processbuffer->pEntry);
			sl_unlock(&lh);
		}
	}
	return status;
}
NTSTATUS devctrl_popthreadinfo(UINT64* pOffset)
{
	NTSTATUS			status = STATUS_SUCCESS;
	KLOCK_QUEUE_HANDLE	lh;
	THREADBUFFER		*threadbuffer = NULL;
	THREADDATA			*threaddata = NULL;
	PNF_DATA			pData;
	UINT64				dataSize = 0;
	ULONG				pPacketlens = 0;

	threaddata = (THREADDATA*)threadctx_get();
	if (!threaddata)
		return STATUS_UNSUCCESSFUL;

	sl_lock(&threaddata->thread_lock, &lh);
	
	while (!IsListEmpty(&threaddata->thread_pending))
	{
		threadbuffer = (THREADBUFFER*)RemoveHeadList(&threaddata->thread_pending);

		pPacketlens = threadbuffer->dataLength;

		dataSize = sizeof(NF_DATA) - 1 + pPacketlens;
		
		if ((g_inBuf.bufferLength - *pOffset - 1) < dataSize)
		{
			status = STATUS_NO_MEMORY;
			break;
		}

		pData = (PNF_DATA)((char*)g_inBuf.kernelVa + *pOffset);

		pData->code = NF_THREAD_INFO;
		pData->id = 1;
		pData->bufferSize = threadbuffer->dataLength;

		if (threadbuffer->dataBuffer) {
			memcpy(pData->buffer, threadbuffer->dataBuffer, threadbuffer->dataLength);
		}

		*pOffset += dataSize;

		break;
	}

	sl_unlock(&lh);

	if (threadbuffer)
	{
		if (NT_SUCCESS(status))
		{
			Thread_PacketFree(threadbuffer);
		}
		else
		{
			sl_lock(&threaddata->thread_lock, &lh);
			InsertHeadList(&threaddata->thread_pending, &threadbuffer->pEntry);
			sl_unlock(&lh);
		}
	}
	return status;
}
NTSTATUS devctrl_popimagemodinfo(UINT64* pOffset)
{
	NTSTATUS			status = STATUS_SUCCESS;
	KLOCK_QUEUE_HANDLE	lh;
	IMAGEMODBUFFER*		imageBufEntry = NULL;
	IMAGEMODDATA*		imagedata = NULL;
	PNF_DATA			pData;
	UINT64				dataSize = 0;
	ULONG				pPacketlens = 0;


	imagedata = (IMAGEMODDATA*)imagemodctx_get();
	if (!imagedata)
		return STATUS_UNSUCCESSFUL;

	sl_lock(&imagedata->imagemod_lock, &lh);

	do
	{
		imageBufEntry = (IMAGEMODBUFFER*)RemoveHeadList(&imagedata->imagemod_pending);

		pPacketlens = imageBufEntry->dataLength;

		dataSize = sizeof(NF_DATA) - 1 + pPacketlens;

		if ((g_inBuf.bufferLength - *pOffset - 1) < dataSize)
		{
			status = STATUS_NO_MEMORY;
			break;
		}

		pData = (PNF_DATA)((char*)g_inBuf.kernelVa + *pOffset);
		if (!pData)
		{
			status = STATUS_UNSUCCESSFUL;
			break;
		}

		pData->code = NF_IMAGEMODE_INFO;
		pData->id = 0;
		pData->bufferSize = imageBufEntry->dataLength;
		memcpy(pData->buffer, imageBufEntry->dataBuffer, imageBufEntry->dataLength);

		*pOffset += dataSize;

	} while (FALSE);

	sl_unlock(&lh);

	if (imageBufEntry)
	{
		if (NT_SUCCESS(status))
		{
			Imagemod_PacketFree(imageBufEntry);
		}
		else
		{
			sl_lock(&imagedata->imagemod_lock, &lh);
			InsertHeadList(&imagedata->imagemod_pending, &imageBufEntry->pEntry);
			sl_unlock(&lh);
		}
	}

	return status;
}
NTSTATUS devctrl_popregisterinfo(UINT64* pOffset)
{
	NTSTATUS			status = STATUS_SUCCESS;
	KLOCK_QUEUE_HANDLE	lh;
	REGISTERBUFFER*		registerBufEntry = NULL;
	REGISTERDATA*		registerdata = NULL;
	PNF_DATA			pData;
	UINT64				dataSize = 0;
	ULONG				pPacketlens = 0;

	registerdata = (REGISTERDATA*)registerctx_get();
	if (!registerdata)
		return STATUS_UNSUCCESSFUL;

	sl_lock(&registerdata->register_lock, &lh);

	do
	{

		registerBufEntry = (REGISTERBUFFER*)RemoveHeadList(&registerdata->register_pending);

		pPacketlens = registerBufEntry->dataLength;

		dataSize = sizeof(NF_DATA) - 1 + pPacketlens;

		if ((g_inBuf.bufferLength - *pOffset - 1) < dataSize)
		{
			status = STATUS_NO_MEMORY;
			break;
		}

		pData = (PNF_DATA)((char*)g_inBuf.kernelVa + *pOffset);
		if (!pData)
		{
			status = STATUS_UNSUCCESSFUL;
			break;
		}

		pData->code = NF_REGISTERTAB_INFO;
		pData->id = 0;
		pData->bufferSize = registerBufEntry->dataLength;
		memcpy(pData->buffer, registerBufEntry->dataBuffer, registerBufEntry->dataLength);

		*pOffset += dataSize;

	} while (FALSE);

	sl_unlock(&lh);

	if (registerBufEntry)
	{
		if (NT_SUCCESS(status))
		{
			Register_PacketFree(registerBufEntry);
		}
		else
		{
			sl_lock(&registerdata->register_lock, &lh);
			InsertHeadList(&registerdata->register_pending, &registerBufEntry->pEntry);
			sl_unlock(&lh);
		}
	}

	return status;
}
NTSTATUS devctrl_popfileinfo(UINT64* pOffset)
{
	NTSTATUS			status = STATUS_SUCCESS;
	KLOCK_QUEUE_HANDLE	lh;
	FILEBUFFER			*fileBufEntry = NULL;
	FILEDATA			*filedata = NULL;
	PNF_DATA			pData;
	UINT64				dataSize = 0;
	ULONG				pPacketlens = 0;

	filedata = (FILEDATA*)filectx_get();
	if (!filedata)
		return STATUS_UNSUCCESSFUL;

	sl_lock(&filedata->file_lock, &lh);

	do
	{

		fileBufEntry = (FILEBUFFER*)RemoveHeadList(&filedata->file_pending);

		pPacketlens = fileBufEntry->dataLength;

		dataSize = sizeof(NF_DATA) - 1 + pPacketlens;

		if ((g_inBuf.bufferLength - *pOffset - 1) < dataSize)
		{
			status = STATUS_NO_MEMORY;
			break;
		}

		pData = (PNF_DATA)((char*)g_inBuf.kernelVa + *pOffset);
		if (!pData)
		{
			status = STATUS_UNSUCCESSFUL;
			break;
		}

		pData->code = NF_FILE_INFO;
		pData->id = 0;
		pData->bufferSize = fileBufEntry->dataLength;
		memcpy(pData->buffer, fileBufEntry->dataBuffer, fileBufEntry->dataLength);

		*pOffset += dataSize;

	} while (FALSE);

	sl_unlock(&lh);

	if (fileBufEntry)
	{
		if (NT_SUCCESS(status))
		{
			File_PacketFree(fileBufEntry);
		}
		else
		{
			sl_lock(&filedata->file_lock, &lh);
			InsertHeadList(&filedata->file_pending, &fileBufEntry->pEntry);
			sl_unlock(&lh);
		}
	}

	return status;

}
NTSTATUS devctrl_popsessioninfo(UINT64* pOffset)
{
	NTSTATUS			status = STATUS_SUCCESS;
	KLOCK_QUEUE_HANDLE	lh;
	SESSIONBUFFER*		sessionbufentry = NULL;
	SESSIONDATA*		sessiondata = NULL;
	PNF_DATA			pData;
	UINT64				dataSize = 0;
	ULONG				pPacketlens = 0;

	sessiondata = (SESSIONDATA*)sessionctx_get();
	if (!sessiondata)
		return STATUS_UNSUCCESSFUL;

	sl_lock(&sessiondata->session_lock, &lh);

	do
	{

		sessionbufentry = (SESSIONBUFFER*)RemoveHeadList(&sessiondata->session_pending);

		pPacketlens = sessionbufentry->dataLength;

		dataSize = sizeof(NF_DATA) - 1 + pPacketlens;

		if ((g_inBuf.bufferLength - *pOffset - 1) < dataSize)
		{
			status = STATUS_NO_MEMORY;
			break;
		}

		pData = (PNF_DATA)((char*)g_inBuf.kernelVa + *pOffset);
		if (!pData)
		{
			status = STATUS_UNSUCCESSFUL;
			break;
		}

		pData->code = NF_SESSION_INFO;
		pData->id = 0;
		pData->bufferSize = sessionbufentry->dataLength;
		memcpy(pData->buffer, sessionbufentry->dataBuffer, sessionbufentry->dataLength);

		*pOffset += dataSize;

	} while (FALSE);

	sl_unlock(&lh);

	if (sessionbufentry)
	{
		if (NT_SUCCESS(status))
		{
			Session_PacketFree(sessionbufentry);
		}
		else
		{
			sl_lock(&sessiondata->session_lock, &lh);
			InsertHeadList(&sessiondata->session_pending, &sessionbufentry->pEntry);
			sl_unlock(&lh);
		}
	}

	return status;

}

// ☆ Dispatch- Handle
UINT64 devctrl_fillBuffer()
{
	PNF_QUEUE_ENTRY	pEntry;
	UINT64		offset = 0;
	NTSTATUS	status = STATUS_SUCCESS;
	KLOCK_QUEUE_HANDLE lh;

	sl_lock(&g_IoQueryLock, &lh);

	while (!IsListEmpty(&g_IoQueryHead))
	{
		pEntry = (PNF_QUEUE_ENTRY)RemoveHeadList(&g_IoQueryHead);

		sl_unlock(&lh);

		switch (pEntry->code)
		{
		case NF_PROCESS_INFO:
		{
			status = devctrl_popprocessinfo(&offset);
		}
		break;
		case NF_THREAD_INFO:
		{
			status = devctrl_popthreadinfo(&offset);
		}
		break;
		case NF_IMAGEMODE_INFO:
		{
			status = devctrl_popimagemodinfo(&offset);
		}
		break;
		case NF_REGISTERTAB_INFO:
		{
			status = devctrl_popregisterinfo(&offset);
		}
		break;
		case NF_FILE_INFO:
		{
			status = devctrl_popfileinfo(&offset);
		}
		break;
		case NF_SESSION_INFO:
		{
			status = devctrl_popsessioninfo(&offset);
		}
		break;
		default:
			ASSERT(0);
			status = STATUS_SUCCESS;
		}

		sl_lock(&g_IoQueryLock, &lh);

		if (!NT_SUCCESS(status))
		{
			InsertHeadList(&g_IoQueryHead, &pEntry->entry);
			break;
		}

		ExFreeToNPagedLookasideList(&g_IoQueryList, pEntry);
	}

	sl_unlock(&lh);
	return offset;
}
void devctrl_serviceReads()
{
	PIRP                irp = NULL;
	PLIST_ENTRY         pIrpEntry;
	BOOLEAN             foundPendingIrp = FALSE;
	PNF_READ_RESULT		pResult;
	KLOCK_QUEUE_HANDLE lh;

	sl_lock(&g_IoQueryLock, &lh);

	if (IsListEmpty(&g_pendedIoRequests) || IsListEmpty(&g_IoQueryHead))
	{
		sl_unlock(&lh);
		return;
	}

	pIrpEntry = g_pendedIoRequests.Flink;
	while (pIrpEntry != &g_pendedIoRequests)
	{
		irp = CONTAINING_RECORD(pIrpEntry, IRP, Tail.Overlay.ListEntry);

		if (IoSetCancelRoutine(irp, NULL))
		{
			// 移除
			RemoveEntryList(pIrpEntry);
			foundPendingIrp = TRUE;
			break;
		}
		else
		{
			KdPrint((DPREFIX"devctrl_serviceReads: skipping cancelled IRP\n"));
			pIrpEntry = pIrpEntry->Flink;
		}
	}

	sl_unlock(&lh);

	if (!foundPendingIrp)
	{
		return;
	}

	pResult = (PNF_READ_RESULT)VerifiMmGetSystemAddressForMdlSafe(irp->MdlAddress, HighPagePriority);
	if (!pResult)
	{
		irp->IoStatus.Information = 0;
		irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return;
	}

	pResult->length = devctrl_fillBuffer();

	irp->IoStatus.Status = STATUS_SUCCESS;
	irp->IoStatus.Information = sizeof(NF_READ_RESULT);
	IoCompleteRequest(irp, IO_NO_INCREMENT);
}
void devctrl_ioThread(void* StartContext)
{
	KLOCK_QUEUE_HANDLE lh;
	PLIST_ENTRY	pEntry;

	for (;;)
	{
		// handler io packter
		KeWaitForSingleObject(
			&g_ioThreadEvent,
			Executive,
			KernelMode,
			FALSE,
			NULL
		);

		// if shutdown
		if (devctrl_isShutdown())
		{
			break;
		}

		// dispathMsghandle
		devctrl_serviceReads();

	}
	PsTerminateSystemThread(STATUS_SUCCESS);
}
void devctrl_pushversion(BOOLEAN code)
{
	g_version = code;
}
void devctrl_pushinfo(int code)
{
	NTSTATUS status = STATUS_SUCCESS;
	PNF_QUEUE_ENTRY pQuery = NULL;
	KLOCK_QUEUE_HANDLE lh;

	switch (code)
	{
	case NF_PROCESS_INFO:
	case NF_THREAD_INFO:
	case NF_IMAGEMODE_INFO:
	case NF_REGISTERTAB_INFO:
	case NF_FILE_INFO:
	case NF_SESSION_INFO:
	{
		pQuery = (PNF_QUEUE_ENTRY)ExAllocateFromNPagedLookasideList(&g_IoQueryList);
		if (!pQuery)
		{
			status = STATUS_UNSUCCESSFUL;
			break;
		}
		pQuery->code = code;
		sl_lock(&g_IoQueryLock, &lh);
		InsertHeadList(&g_IoQueryHead, &pQuery->entry);
		sl_unlock(&lh);
	}
	break;
	default:
		return;
	}
	// keSetEvent
	KeSetEvent(&g_ioThreadEvent, IO_NO_INCREMENT, FALSE);
}