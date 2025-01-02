#include "hdfilemap.h"

// Creates an entry for a File
NTSTATUS
HdInitializeFileEntry(
    PLIST_ENTRY ListHead,
    PUNICODE_STRING FileName
) {
    PHIDDEN_FILE_ENTRY NewEntry;

    // Verify that the map is not empty
    if (ListHead == NULL) {
        return STATUS_INVALID_PARAMETER_1;
    }

    // Check if already exists
    if (HdIsFileInMap(ListHead, FileName)) {
        return STATUS_ALREADY_REGISTERED;
    }

    // Allocate entry
    NewEntry = (PHIDDEN_FILE_ENTRY)ExAllocatePoolWithTag(NonPagedPool, sizeof(HIDDEN_FILE_ENTRY), 'HdfT');
    if (NewEntry == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Copy FileName
    RtlZeroMemory(NewEntry, sizeof(HIDDEN_FILE_ENTRY));
    RtlDuplicateUnicodeString(RTL_DUPLICATE_UNICODE_STRING_NULL_TERMINATE, FileName, &NewEntry->FileName);
    if (NewEntry->FileName.Buffer == NULL) {
        ExFreePoolWithTag(NewEntry, 'HdfT');
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Initialize PID list for new Entry
    InitializeListHead(&NewEntry->PidList);

    // Insert File into the List
    InsertTailList(ListHead, &NewEntry->ListEntry);

    return STATUS_SUCCESS;
}

// Inserts a Pid into a File's PID_LIST
NTSTATUS
HdAddPidToHiddenFile(
    PLIST_ENTRY ListHead,
    PCUNICODE_STRING FileName,
    ULONG Pid
){
    NTSTATUS Status;
    PHIDDEN_FILE_ENTRY FileEntry;

    // Verify that the map is valid
    if (ListHead == NULL) {
        return STATUS_INVALID_PARAMETER_1;
    }

    // Check if FileName exists
    FileEntry = HdGetEntryFromMap(ListHead, FileName);
    if (FileEntry == NULL) {
        return STATUS_INVALID_PARAMETER_2;
    }

    // Check if Pid is already hidden to the given FileName
    if (HdIsFileHiddenToPid(ListHead, FileName, Pid)) {
        return STATUS_ALREADY_REGISTERED;
    }

    // Finally, add PID to FileName's List 
    Status = HdAddPidToList(&FileEntry->PidList, Pid);

    return Status;
}

// Removes a Pid from a File's PID_LIST
NTSTATUS
HdRemovePidFromHiddenFile(
    PLIST_ENTRY ListHead,
    PCUNICODE_STRING FileName,
    ULONG Pid
){
    NTSTATUS Status;
    PHIDDEN_FILE_ENTRY FileEntry;

    // Check if FileName exists
    FileEntry = HdGetEntryFromMap(ListHead, FileName);
    if (FileEntry == NULL) {
        return STATUS_INVALID_PARAMETER_2;
    }

    // Check if Pid is currently hidden to the given FileName
    if (!HdIsFileHiddenToPid(ListHead, FileName, Pid)) {
        return STATUS_ATTRIBUTE_NOT_PRESENT;
    }

    // Finally, delete PID to FileName's List 
    Status = HdRemovePidFromList(&FileEntry->PidList, Pid);

    return Status;
}

// Removes a File from the MAP
NTSTATUS
HdDeleteHiddenFileFromMap(
    PLIST_ENTRY ListHead,
    PCUNICODE_STRING FileName
){
    PHIDDEN_FILE_ENTRY FileEntry;

    // Verify that the map is not empty
    if (HdIsListEntryEmpty(ListHead) || ListHead == NULL) {
        return STATUS_INVALID_PARAMETER_1;
    }

    // Check if FileName exists
    FileEntry = HdGetEntryFromMap(ListHead, FileName);
    if (FileEntry == NULL) {
        return STATUS_NOT_FOUND;
    }

    HdFreePidList(&FileEntry->PidList);
    RemoveEntryList(&FileEntry->ListEntry);
    ExFreePoolWithTag(FileEntry, 'HdfT');

    return STATUS_SUCCESS;
}

// Clears a Hidden File Map
VOID
HdFreeHiddenFilesMap(
    PLIST_ENTRY ListHead
){
    if (ListHead == NULL) return;

    PLIST_ENTRY Entry;
    PHIDDEN_FILE_ENTRY Current;

    if (HdIsListEntryEmpty(ListHead) || ListHead == NULL) {
        return;
    }

    for (Entry = ListHead->Flink; Entry != ListHead; Entry = Entry->Flink) {
        Current = CONTAINING_RECORD(Entry, HIDDEN_FILE_ENTRY, ListEntry);
        HdFreePidList(&Current->PidList);
        ExFreePoolWithTag(Entry, 'HdfT');
    }

    InitializeListHead(ListHead);

}

// Returns whether a file is currently hiding from any process (aka: the map has any key with FileName)
BOOLEAN
HdIsFileInMap(
    PLIST_ENTRY ListHead,
    PCUNICODE_STRING FileName
){
    PHIDDEN_FILE_ENTRY Entry;
    Entry = HdGetEntryFromMap(ListHead, FileName);

    return (Entry != NULL);
}

// Returns whether the given Pid is hidden to a given FileName
BOOLEAN
HdIsFileHiddenToPid(
    PLIST_ENTRY ListHead,
    PCUNICODE_STRING FileName,
    ULONG Pid
){
    PHIDDEN_FILE_ENTRY FileEntry;

    // Verify that the map is not empty
    if (HdIsListEntryEmpty(ListHead) || ListHead == NULL) {
        return FALSE;
    }

    // Check if FileName exists
    FileEntry = HdGetEntryFromMap(ListHead, FileName);
    if (FileEntry == NULL) {
        return FALSE;
    }

    return HdIsPidInList(&FileEntry->PidList, Pid);
}

// For each FileName, the whole list of PIDs is printed.
VOID
HdPrintHiddenFilesMap(
    PLIST_ENTRY ListHead
){
    PLIST_ENTRY Entry;
    PHIDDEN_FILE_ENTRY Current;
    int i = 1;

    DbgPrint("\r\n");
    DbgPrint("[#] Hidden Files List\r\n");

    for (Entry = ListHead->Flink; Entry != ListHead; Entry = Entry->Flink) {
        Current = CONTAINING_RECORD(Entry, HIDDEN_FILE_ENTRY, ListEntry);
        DbgPrint("  [%d] %wZ \r\n", i, &Current->FileName);
        HdPrintPidList(&Current->PidList);
        ++i;
    }

    DbgPrint("\r\n");
}

// Retrieves a Hidden File Entry with the given FileName
PHIDDEN_FILE_ENTRY
HdGetEntryFromMap(
    PLIST_ENTRY ListHead,
    PCUNICODE_STRING FileName
){
    if (HdIsListEntryEmpty(ListHead) || ListHead == NULL) {
        return NULL;
    }

    PLIST_ENTRY Entry;
    PHIDDEN_FILE_ENTRY Current;

    for (Entry = ListHead->Flink; Entry != ListHead; Entry = Entry->Flink) {
        Current = CONTAINING_RECORD(Entry, HIDDEN_FILE_ENTRY, ListEntry);

        if (RtlCompareUnicodeString(&Current->FileName, FileName, TRUE) == 0) {
            return Current;
        }
    }

    return NULL;
}

LONG
HdGetHiddenFilesListSize(
    PLIST_ENTRY ListHead
){
    INT Size = 0;

    if (ListHead == NULL) {
        return -1;
    }

    PLIST_ENTRY Entry = ListHead->Flink;
    while (Entry != ListHead) {
        ++Size;
        Entry = Entry->Flink;
    }

    return Size;
}