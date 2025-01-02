#ifndef _HDPIDLIST_H_
#define _HDPIDLIST_H_

#include <fltKernel.h>

typedef struct _PID_ENTRY {
    LIST_ENTRY ListEntry;
    ULONG Pid;
} PID_ENTRY, * PPID_ENTRY;

NTSTATUS
HdAddPidToList(
    PLIST_ENTRY ListHead,
    ULONG Pid
);

NTSTATUS
HdRemovePidFromList(
    PLIST_ENTRY ListHead,
    ULONG Pid
);

VOID
HdFreePidList(
    PLIST_ENTRY ListHead
);

BOOLEAN
HdIsPidInList(
    PLIST_ENTRY ListHead,
    ULONG Pid
);

VOID
HdPrintPidList(
    PLIST_ENTRY ListHead
);

BOOLEAN
HdIsListEntryEmpty(
    PLIST_ENTRY ListHead
);

PPID_ENTRY
HdGetEntryPid(
    PLIST_ENTRY ListHead,
    ULONG Pid
);
#endif
