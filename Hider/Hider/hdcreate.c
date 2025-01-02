#include "hider.h"
/*
*  + ---------------------------------------------------------------------------------- +
*  |                    PreOperation Callback for IRP_MJ_CREATE                         |
*  + ---------------------------------------------------------------------------------- +
*  |                                                                                    |
*  |  What it does?                                                                     |
*  |    a) Initial Checkings                                                            |
*  |        - Don't treat paging files opens.                                           |
*  |        - Don't treat volume opens.                                                 |
*  |        - Opens by file ID are name agnostic. Thus we do not care about this open.  |
*  |                                                                                    |
*  |    b) Check for permissions and other errors by calling FltCreate()                |
*  |        - If return status code is not sucessful --> return error to user.          |
*  |        - If not, continue.                                                         |
*  |                                                                                    |
*  |    c) Search filename in hidden files list.                                        |
*  |        - If found --> Return STATUS_ACCESS_DENIED.                                 |
*  |        - If not found --> Do nothing.                                              |
*  |                                                                                    |
*  + ---------------------------------------------------------------------------------- +
*/

FLT_PREOP_CALLBACK_STATUS HdPreCreateCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
){
    PAGED_CODE();

    UNREFERENCED_PARAMETER(CompletionContext);

    NTSTATUS Status = STATUS_SUCCESS;
    FLT_PREOP_CALLBACK_STATUS ReturnValue = FLT_PREOP_SUCCESS_NO_CALLBACK;
    PFLT_FILE_NAME_INFORMATION FileNameInfo = NULL;
    ULONG RequestorPid;

    PFLT_FILTER         Filter = FltObjects->Filter;
    PFLT_INSTANCE       Instance = FltObjects->Instance;
    HANDLE              FileHandle = 0;
    ACCESS_MASK         DesiredAccess = Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess;
    OBJECT_ATTRIBUTES   ObjectAttributes;
    IO_STATUS_BLOCK     IoStatusBlock;
    PLARGE_INTEGER      AllocationSize = &Data->Iopb->Parameters.Create.AllocationSize;
    ULONG               FileAttributes = Data->Iopb->Parameters.Create.FileAttributes;
    ULONG               ShareAccess = Data->Iopb->Parameters.Create.ShareAccess;
    ULONG               CreateDisposition = FILE_OPEN; //(Data->Iopb->Parameters.Create.Options >> 24) & 0xFF; 
    ULONG               CreateOptions = Data->Iopb->Parameters.Create.Options & 0xFFFFFF;
    PVOID               EaBuffer = NULL;                                                    // Data->Iopb->Parameters.Create.EaBuffer;
    ULONG               EaLength = 0;                                                       // Data->Iopb->Parameters.Create.EaLength;
    ULONG               Flags = 0;

    /* 
    *   a) Initial Checkings 
    */

    // Check if driver is enabled.
    if (!DriverEnabled) {
        ReturnValue = FLT_PREOP_SUCCESS_NO_CALLBACK;
        goto PreCreateExit;
    }

    // Don't treat paging files opens.
    if (FlagOn(Data->Iopb->OperationFlags, SL_OPEN_PAGING_FILE)) {
        ReturnValue = FLT_PREOP_SUCCESS_NO_CALLBACK;
        goto PreCreateExit;
    }

    // Don't treat volume opens.
    if (FlagOn(Data->Iopb->TargetFileObject->Flags, FO_VOLUME_OPEN)) {

        ReturnValue = FLT_PREOP_SUCCESS_NO_CALLBACK;
        goto PreCreateExit;
    }

    // Opens by file ID are name agnostic. Thus we do not care about this open.
    if (FlagOn(Data->Iopb->Parameters.Create.Options, FILE_OPEN_BY_FILE_ID)) {

        ReturnValue = FLT_PREOP_SUCCESS_NO_CALLBACK;
        goto PreCreateExit;
    }

    /*
    *   b) Check for permissions and other errors by calling FltCreate()
    */

    // Get opened name.
    Status = HdGetFileNameInformation(
        Data,
        NULL,
        NULL,
        FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT | FLT_FILE_NAME_DO_NOT_CACHE,
        &FileNameInfo
    );

    if (!NT_SUCCESS(Status)) {
        ReturnValue = FLT_PREOP_COMPLETE;
        goto PreCreateExit;
    }

    // Parse it.
    Status = FltParseFileNameInformation(FileNameInfo);
    if (!NT_SUCCESS(Status)) {
        ReturnValue = FLT_PREOP_COMPLETE;
        goto PreCreateExit;
    }

    // Simulate CreateFile
    InitializeObjectAttributes(
        &ObjectAttributes,
        &FileNameInfo->Name,
        OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
        NULL,
        NULL
    );

    Status = FltCreateFile(
        Filter,
        Instance,
        &FileHandle,
        DesiredAccess,
        &ObjectAttributes,
        &IoStatusBlock,
        AllocationSize,
        FileAttributes,
        ShareAccess,
        CreateDisposition,
        CreateOptions,
        EaBuffer,
        EaLength,
        Flags
    );

    if (!NT_SUCCESS(Status)) {
        ReturnValue = FLT_PREOP_COMPLETE;
        goto PreCreateExit;
    }

    FltClose(FileHandle);
    FileHandle = NULL;

    /*
    *   c) Search filename in hidden files list.
    */

    RequestorPid = FltGetRequestorProcessId(Data);
    if (HdIsFileHiddenToPid(&HiddenFilesMap, &FileNameInfo->Name, RequestorPid)) {
        Status = STATUS_HIDDEN_FILE;
        ReturnValue = FLT_PREOP_COMPLETE;
        goto PreCreateExit;
    }

PreCreateExit:

    if (ReturnValue == FLT_PREOP_COMPLETE) {
        //  If we get here, then Status must be a failure code.
        FLT_ASSERT(!NT_SUCCESS(Status)); 
        Data->IoStatus.Status = Status;
    }

    if (FileNameInfo != NULL) {
        FltReleaseFileNameInformation(FileNameInfo);
    }

    return ReturnValue;
}