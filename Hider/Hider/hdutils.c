#include "hider.h"

_When_(Data == NULL, _Pre_satisfies_(FileObject != NULL && Instance != NULL))
_When_(FileObject == NULL || Instance == NULL, _Pre_satisfies_(Data != NULL))
NTSTATUS HdGetFileNameInformation(
    _In_opt_ PFLT_CALLBACK_DATA Data,
    _In_opt_ PFILE_OBJECT FileObject,
    _In_opt_ PFLT_INSTANCE Instance,
    _In_ FLT_FILE_NAME_OPTIONS NameOptions,
    _Outptr_ PFLT_FILE_NAME_INFORMATION *FileNameInformation
){
    NTSTATUS Status;
    PAGED_CODE();
    FLT_ASSERT(Data || FileObject);
    *FileNameInformation = NULL;

    if (ARGUMENT_PRESENT(Data)) {
        Status = FltGetFileNameInformation(Data, NameOptions, FileNameInformation);
    }
    else if (ARGUMENT_PRESENT(FileObject)) {
        Status = FltGetFileNameInformationUnsafe(FileObject, Instance, NameOptions, FileNameInformation);
    }
    else {
        FLT_ASSERT(FALSE);
        Status = STATUS_INVALID_PARAMETER;
    }
    return Status;
}

NTSTATUS HdInsertTestFilesIntoMap(
    PLIST_ENTRY FileMap
){
    NTSTATUS Status = STATUS_SUCCESS;

    UNICODE_STRING TestFile1, TestFile2;
    ULONG TestPid1, TestPid2;

    TestPid1 = 4716;
    TestPid2 = 1476;

    InitializeListHead(FileMap);

    RtlInitUnicodeString(&TestFile1, L"\\Device\\HarddiskVolume3\\Users\\josep\\Desktop\\test.txt");
    RtlInitUnicodeString(&TestFile2, L"test.txt");

    /* INSERT TEST FILE 1*/
    Status = HdInitializeFileEntry(FileMap, &TestFile1);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("File entry 1 failed\r\n");
        HdFreeHiddenFilesMap(FileMap);
        return Status;
    }

    Status = HdAddPidToHiddenFile(FileMap, &TestFile1, TestPid1);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("AddPidTofile 1 failed\r\n");
        HdFreeHiddenFilesMap(FileMap);
        return Status;
    }

    /* INSERT TEST FILE 1*/
    Status = HdInitializeFileEntry(FileMap, &TestFile2);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("File entry 2 failed\r\n");
        HdFreeHiddenFilesMap(FileMap);
        return Status;
    }

    Status = HdAddPidToHiddenFile(FileMap, &TestFile2, TestPid2);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("AddPidTofile 2 failed\r\n");
        HdFreeHiddenFilesMap(FileMap);
        return Status;
    }

    /* ------ TEST 1 ------ */
    Status = HdAddPidToHiddenFile(FileMap, &TestFile1, TestPid2);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("[!] Test 1 failed at: AddPidTofile 3 failed.\r\n");
        HdFreeHiddenFilesMap(FileMap);
        return Status;
    }

    Status = HdRemovePidFromHiddenFile(FileMap, &TestFile1, TestPid1);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("[!] Test 1 failed at: RemovePidFromFile 1 failed.\r\n");
        HdFreeHiddenFilesMap(FileMap);
        return Status;
    }

    Status = HdRemovePidFromHiddenFile(FileMap, &TestFile2, TestPid2);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("[!] Test 1 failed at: RemovePidFromFile 2 failed.\r\n");
        HdFreeHiddenFilesMap(FileMap);
        return Status;
    }

    /* ------ TEST 2 ------ */
    Status = HdAddPidToHiddenFile(FileMap, &TestFile1, TestPid1);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("[!] Test 2 failed at: AddPidTofile 4 failed\r\n");
        HdFreeHiddenFilesMap(FileMap);
        return Status;
    }

    Status = HdAddPidToHiddenFile(FileMap, &TestFile1, TestPid1);
    if (Status != STATUS_ALREADY_REGISTERED && !NT_SUCCESS(Status)) {
        DbgPrint("[!] Test 2 failed at: AddPidTofile 5 failed\r\n");
        HdFreeHiddenFilesMap(FileMap);
        return Status;
    }

    /* ------ TEST 3 ------ */
    Status = HdDeleteHiddenFileFromMap(FileMap, &TestFile1);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("[!] Test 3 failed at: RemoveHiddenFile 1 failed\r\n");
        HdFreeHiddenFilesMap(FileMap);
        return Status;
    }

    Status = HdDeleteHiddenFileFromMap(FileMap, &TestFile2);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("[!] Test 3 failed at: RemoveHiddenFile 2 failed\r\n");
        HdFreeHiddenFilesMap(FileMap);
        return Status;
    }

    /* FINAL LIST */
    Status = HdInitializeFileEntry(FileMap, &TestFile1);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("Initialize long name failed\r\n");
        HdFreeHiddenFilesMap(FileMap);
        return Status;
    }

    Status = HdInitializeFileEntry(FileMap, &TestFile2);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("Initialize short name failed\r\n");
        HdFreeHiddenFilesMap(FileMap);
        return Status;
    }

    Status = HdAddPidToHiddenFile(FileMap, &TestFile1, TestPid1);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("AddPidTofile f1 failed\r\n");
        HdFreeHiddenFilesMap(FileMap);
        return Status;
    }

    Status = HdAddPidToHiddenFile(FileMap, &TestFile2, TestPid1);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("AddPidTofile f2 failed\r\n");
        HdFreeHiddenFilesMap(FileMap);
        return Status;
    }

    Status = HdAddPidToHiddenFile(FileMap, &TestFile1, TestPid2);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("AddPidTofile f3 failed\r\n");
        HdFreeHiddenFilesMap(FileMap);
        return Status;
    }

    Status = HdAddPidToHiddenFile(FileMap, &TestFile2, TestPid2);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("AddPidTofile f4 failed\r\n");
        HdFreeHiddenFilesMap(FileMap);
        return Status;
    }

    HdPrintHiddenFilesMap(FileMap);

    return Status;
}

NTSTATUS
HdInsertPidsIntoPidList(
    PLIST_ENTRY PidList
){
    NTSTATUS Status = STATUS_SUCCESS;
    ULONG TestPid;

    TestPid = 3156;

    InitializeListHead(PidList);

    Status = HdAddPidToList(PidList, TestPid);
    if (!NT_SUCCESS(Status)) {
        HdFreePidList(PidList);
        return Status;
    }

    HdPrintPidList(PidList);

    return Status;
}

NTSTATUS 
HdPrintFileName(
    PUNICODE_STRING FileName
){
    if (FileName != NULL) {
        DbgPrint("File: %wZ\r\n", FileName);
        return STATUS_SUCCESS;
    }
    return STATUS_INVALID_PARAMETER;
}

NTSTATUS
HdBuildFullFileName(
    PUNICODE_STRING FullFileName,
    PUNICODE_STRING FileName
) {
    NTSTATUS Status = STATUS_SUCCESS;
    UNICODE_STRING BackSlash;

    RtlInitUnicodeString(&BackSlash, L"\\");

    Status = RtlAppendUnicodeStringToString(FullFileName, &BackSlash);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    Status = RtlAppendUnicodeStringToString(FullFileName, FileName);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    return STATUS_SUCCESS;
}

