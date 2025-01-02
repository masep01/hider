#include "hider.h"
/*
*  + ---------------------------------------------------------------------------------- +
*  |               PreOperation Callback for IRP_MJ_SET_INFORMATION                     |
*  + ---------------------------------------------------------------------------------- +
*  |                                                                                    |
*  |  What it does?                                                                     |
*  |    a) Retrieve FileName                                                            |
*  |                                                                                    |
*  |    b) Detect operations that involve modification requests on files.               |
*  |		- FileBasicInformation            (04)									    |
*  |		- FileRenameInformation           (10)										|
*  |		- FileLinkInformation             (11)										|
*  |		- FileDispositionInformation      (13)										|
*  |		- FileAllocationInformation       (19)                                      |
*  |		- FileEndOfFileInformation        (20)									    |
*  |		- FileValidDataLengthInformation  (39)										|
*  |		- FileDispositionInformationEx    (64)									    |
*  |		- FileRenameInformationEx         (65)                                      |
*  |		- FileLinkInformationEx           (72)                                      |
*  |                                                                                    |
*  |    c) Send error if file needs to be hidden, preventing any modification.          |
*  |                                                                                    |
*  + ---------------------------------------------------------------------------------- +
*/

FLT_PREOP_CALLBACK_STATUS
HdPreSetInfo(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
) {
    PAGED_CODE();
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    NTSTATUS Status = STATUS_SUCCESS;
    FLT_PREOP_CALLBACK_STATUS ReturnValue = FLT_PREOP_SUCCESS_NO_CALLBACK;
    PFLT_FILE_NAME_INFORMATION FileNameInfo = NULL;
    FILE_INFORMATION_CLASS FileInfoClass = Data->Iopb->Parameters.QueryFileInformation.FileInformationClass;
    ULONG RequestorPid;

    // Check if driver is enabled.
    if (!DriverEnabled) {
        ReturnValue = FLT_PREOP_SUCCESS_NO_CALLBACK;
        goto PreSetExit;
    }


    switch (FileInfoClass) {
        // Fallthrough behavior
        case FileBasicInformation:
        case FileRenameInformation:
        case FileLinkInformation:
        case FileDispositionInformation:
        case FileAllocationInformation:
        case FileEndOfFileInformation:
        case FileValidDataLengthInformation:
        case FileDispositionInformationEx:
        case FileRenameInformationEx:
        case FileLinkInformationEx:

        // Retrieve File Name
        Status = HdGetFileNameInformation(
            Data,
            NULL,
            NULL,
            FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT | FLT_FILE_NAME_DO_NOT_CACHE,
            &FileNameInfo
        );
        if (!NT_SUCCESS(Status)) {
            ReturnValue = FLT_PREOP_COMPLETE;
            goto PreSetExit;
        }

        // Parse it
        Status = FltParseFileNameInformation(FileNameInfo);
        if (!NT_SUCCESS(Status)) {
            ReturnValue = FLT_PREOP_COMPLETE;
            goto PreSetExit;
        }

        // Hide it if needed
        RequestorPid = FltGetRequestorProcessId(Data);
        if (HdIsFileHiddenToPid(&HiddenFilesMap, &FileNameInfo->Name, RequestorPid)) {
            Status = STATUS_HIDDEN_FILE;
            ReturnValue = FLT_PREOP_COMPLETE;
            goto PreSetExit;
        }

        break;
    }

PreSetExit:
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