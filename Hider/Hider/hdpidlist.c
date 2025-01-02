#include "hdpidlist.h"

NTSTATUS
HdAddPidToList(
    PLIST_ENTRY ListHead,
    ULONG Pid
){
    PPID_ENTRY NewEntry;

    if (ListHead == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    if (HdIsPidInList(ListHead, Pid)) {
        return STATUS_ALREADY_REGISTERED;
    }

    NewEntry = (PPID_ENTRY)ExAllocatePoolWithTag(NonPagedPool, sizeof(PID_ENTRY), 'PidT');
    if (NewEntry == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(NewEntry, sizeof(PID_ENTRY));
    NewEntry->Pid = Pid;

    InsertTailList(ListHead, &NewEntry->ListEntry);

    return STATUS_SUCCESS;
}

NTSTATUS
HdRemovePidFromList(
    PLIST_ENTRY ListHead,
    ULONG Pid
){
    PPID_ENTRY PidEntry;

    if (HdIsListEntryEmpty(ListHead) || ListHead == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    PidEntry = HdGetEntryPid(ListHead, Pid);
    if (PidEntry == NULL) {
        return STATUS_NOT_FOUND;
    }
    
    RemoveEntryList(&PidEntry->ListEntry);
    ExFreePoolWithTag(PidEntry, 'PidT');

    return STATUS_SUCCESS;
}

VOID
HdFreePidList(
    PLIST_ENTRY ListHead
){
    PLIST_ENTRY Entry;
    PPID_ENTRY Current;

    if (HdIsListEntryEmpty(ListHead) || ListHead == NULL) {
        return;
    }

    for (Entry = ListHead->Flink; Entry != ListHead; Entry = Entry->Flink) {
        Current = CONTAINING_RECORD(Entry, PID_ENTRY, ListEntry);
        ExFreePoolWithTag(Current, 'PidT');
    }

    InitializeListHead(ListHead);
}

BOOLEAN
HdIsPidInList(
    PLIST_ENTRY ListHead,
    ULONG Pid
){
    PPID_ENTRY  Entry;
    Entry = HdGetEntryPid(ListHead, Pid);

    return (Entry != NULL);
}

VOID
HdPrintPidList(
    PLIST_ENTRY ListHead
){
    PLIST_ENTRY Entry;
    PPID_ENTRY Current;
    int i = 1;

    DbgPrint("\r\n");
    DbgPrint("      [#] PID List\r\n");
   
    for (Entry = ListHead->Flink; Entry != ListHead; Entry = Entry->Flink) {
        Current = CONTAINING_RECORD(Entry, PID_ENTRY, ListEntry);
        DbgPrint("      [%d] %u \r\n", i, Current->Pid);
        ++i;
    }

    DbgPrint("\r\n");
}

BOOLEAN
HdIsListEntryEmpty(
    PLIST_ENTRY ListHead
) {
    return (ListHead->Flink == ListHead && ListHead->Blink == ListHead);
}

PPID_ENTRY
HdGetEntryPid(
    PLIST_ENTRY ListHead,
    ULONG Pid
) {
    if (HdIsListEntryEmpty(ListHead) || ListHead == NULL) {
        return NULL;
    }

    PLIST_ENTRY Entry;
    PPID_ENTRY Current;

    for (Entry = ListHead->Flink; Entry != ListHead; Entry = Entry->Flink) {
        Current = CONTAINING_RECORD(Entry, PID_ENTRY, ListEntry);

        if (Current->Pid == Pid) {
            return Current;
        }
    }

    return NULL;
}