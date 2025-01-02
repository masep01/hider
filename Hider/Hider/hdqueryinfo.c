#include "hider.h"
/*
*  + ---------------------------------------------------------------------------------- +
*  |               PreOperation Callback for IRP_MJ_QUERY_INFORMATION                   |
*  + ---------------------------------------------------------------------------------- +
*  |                                                                                    |
*  |  What it does?                                                                     |
*  |    a) Retrieve FileName                                                            |
*  |                                                                                    |
*  |    b) Detect operations that involve information requests about files.             |
*  |		- FileBasicInformation           (04)										|
*  |		- FileStandardInformation        (05)									    |
*  |	    - FileNameInformation            (09)										|
*  |        - FileAllInformation             (18)                                       |
*  |		- FileAlternateNameInformation   (21)										|
*  |		- FileStreamInformation          (22)										|
*  |	    - FileNetworkOpenInformation     (34)      								    |
*  |		- FileAttributeTagInformation	 (35)										|
*  |		- FileNormalizedNameInformation  (48)					                    |
*  |		- FileIdInformation	             (59)										|
*  |																				    |
*  |	c) Send error if file needs to be hided and delete details.          			|
*  |																				    |
*  + ---------------------------------------------------------------------------------- +
*/

FLT_PREOP_CALLBACK_STATUS
HdPreQueryInfo(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
) {
    PAGED_CODE();
    
    UNREFERENCED_PARAMETER(CompletionContext);
    NTSTATUS Status = STATUS_SUCCESS;
    FLT_PREOP_CALLBACK_STATUS ReturnValue = FLT_PREOP_SUCCESS_NO_CALLBACK;
    PFLT_FILE_NAME_INFORMATION FileNameInfo = NULL;
    FILE_OBJECT FileObject = *FltObjects->FileObject;
    FILE_INFORMATION_CLASS FileInfoClass = Data->Iopb->Parameters.QueryFileInformation.FileInformationClass;
    ULONG RequestorPid;

    // Check if driver is enabled.
    if (!DriverEnabled) {
        ReturnValue = FLT_PREOP_SUCCESS_NO_CALLBACK;
        goto PreQueryExit;
    }


    switch (FileInfoClass) {
        // Fallthrough behavior
        case FileBasicInformation:
        case FileStandardInformation:
        case FileNameInformation:
        case FileAllInformation:
        case FileAlternateNameInformation:
        case FileStreamInformation:
        case FileNetworkOpenInformation:
        case FileAttributeTagInformation:
        case FileNormalizedNameInformation:
        case FileIdInformation:

        // Retrieve File Name
        Status = HdGetFileNameInformation(
            NULL,
            &FileObject,
            NULL,
            FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT | FLT_FILE_NAME_DO_NOT_CACHE,
            &FileNameInfo
        );

        if (!NT_SUCCESS(Status)) {
            ReturnValue = FLT_PREOP_COMPLETE;
            goto PreQueryExit;
        }

        // Parse it
        Status = FltParseFileNameInformation(FileNameInfo);
        if (!NT_SUCCESS(Status)) {
            ReturnValue = FLT_PREOP_COMPLETE;
            goto PreQueryExit;
        }

        RequestorPid = FltGetRequestorProcessId(Data);
        if (HdIsFileHiddenToPid(&HiddenFilesMap, &FileNameInfo->Name, RequestorPid)) {
            Status = STATUS_HIDDEN_FILE;
            ReturnValue = FLT_PREOP_COMPLETE;
            goto PreQueryExit;
        }

        break;
    }

PreQueryExit:
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