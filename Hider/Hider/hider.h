#ifndef HIDER_H
#define HIDER_H

#include <fltKernel.h>
#include <ntddk.h>

#include "hdfilemap.h"

#define STATUS_HIDDEN_FILE STATUS_FILE_NOT_AVAILABLE

#define NT_DEVICE_NAME      L"\\Device\\HiderDevice"
#define DOS_DEVICE_NAME     L"\\DosDevices\\HiderDosDevice"

// The IOCTL function codes from 0x800 to 0xFFF are for customer use.
#define IOCTL_ENABLE_DRIVER         CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DISABLE_DRIVER        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_ADD_FILE              CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_REMOVE_FILE           CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_ADD_PID               CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_REMOVE_PID            CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)

extern PFLT_FILTER FilterHandle;
extern LIST_ENTRY HiddenFilesMap;
extern BOOLEAN DriverEnabled;

NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT DriverObject, 
    PUNICODE_STRING RegistryPath
);

NTSTATUS 
UnloadCallback(
    FLT_FILTER_UNLOAD_FLAGS Flags
);

NTSTATUS 
FilterDeviceControl(
    _In_ PDEVICE_OBJECT DeviceObject, 
    _In_ PIRP Irp
);

NTSTATUS
FilterDriverCreate(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
);

/* Callback functions */
FLT_PREOP_CALLBACK_STATUS
HdPreCreateCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
);

FLT_PREOP_CALLBACK_STATUS
HdPreNetworkQueryCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
);

FLT_PREOP_CALLBACK_STATUS
HdPreFastIOCheck(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
);

FLT_PREOP_CALLBACK_STATUS
HdPreQueryInfo(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
);

FLT_PREOP_CALLBACK_STATUS
HdPreSetInfo(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
);

FLT_PREOP_CALLBACK_STATUS
HdPreDirCtrl(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
);

FLT_POSTOP_CALLBACK_STATUS 
HdPostDirCtrl(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
);

NTSTATUS 
CleanFileFullDirectoryInformation(
    PFILE_FULL_DIR_INFORMATION Info,
    PFLT_CALLBACK_DATA Data,
    PFLT_FILE_NAME_INFORMATION DirInfo
);

NTSTATUS
CleanFileBothDirectoryInformation(
    PFILE_BOTH_DIR_INFORMATION Info,
    PFLT_CALLBACK_DATA Data,
    PFLT_FILE_NAME_INFORMATION DirInfo
);

NTSTATUS
CleanFileDirectoryInformation(
    PFILE_DIRECTORY_INFORMATION Info,
    PFLT_CALLBACK_DATA Data,
    PFLT_FILE_NAME_INFORMATION DirInfo
);

NTSTATUS
CleanFileIdFullDirectoryInformation(
    PFILE_ID_FULL_DIR_INFORMATION Info,
    PFLT_CALLBACK_DATA Data,
    PFLT_FILE_NAME_INFORMATION DirInfo
);

NTSTATUS
CleanFileIdBothDirectoryInformation(
    PFILE_ID_BOTH_DIR_INFORMATION Info,
    PFLT_CALLBACK_DATA Data,
    PFLT_FILE_NAME_INFORMATION DirInfo
);

NTSTATUS
CleanFileNamesInformation(
    PFILE_NAMES_INFORMATION Info,
    PFLT_CALLBACK_DATA Data,
    PFLT_FILE_NAME_INFORMATION DirInfo
);

/* ------------ UTILS ------------ */
_When_(Data == NULL, _Pre_satisfies_(FileObject != NULL && Instance != NULL))
_When_(FileObject == NULL || Instance == NULL, _Pre_satisfies_(Data != NULL))
NTSTATUS 
HdGetFileNameInformation(
    _In_opt_ PFLT_CALLBACK_DATA Data,
    _In_opt_ PFILE_OBJECT FileObject,
    _In_opt_ PFLT_INSTANCE Instance,
    _In_ FLT_FILE_NAME_OPTIONS NameOptions,
    _Outptr_ PFLT_FILE_NAME_INFORMATION* FileNameInformation
);

NTSTATUS
HdInsertTestFilesIntoMap(
    PLIST_ENTRY FileMap
);

NTSTATUS
HdInsertPidsIntoPidList(
    PLIST_ENTRY PidList
);

NTSTATUS 
HdPrintFileName(
    PUNICODE_STRING FileName
);

NTSTATUS
HdBuildFullFileName(
    PUNICODE_STRING FullFileName,
    PUNICODE_STRING FileName
);

#endif
