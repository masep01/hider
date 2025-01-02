#ifndef _HD_FILE_MAP_H_
#define _HD_FILE_MAP_H_

#include <fltKernel.h>
#include "hdpidlist.h"

/* Map <FileName, PidList> */
typedef struct _HIDDEN_FILE_ENTRY {
    LIST_ENTRY ListEntry;
    UNICODE_STRING FileName;    // Key   <String>
    LIST_ENTRY PidList;         // Value <Pointer to PID List>

} HIDDEN_FILE_ENTRY, * PHIDDEN_FILE_ENTRY;

// Creates an entry for a File
NTSTATUS
HdInitializeFileEntry(
    PLIST_ENTRY ListHead,
    PUNICODE_STRING FileName
);

// Inserts a Pid to a File's PID_LIST
NTSTATUS
HdAddPidToHiddenFile(
    PLIST_ENTRY ListHead,
    PCUNICODE_STRING FileName,
    ULONG Pid
);

// Removes a Pid from a File's PID_LIST
NTSTATUS
HdRemovePidFromHiddenFile(
    PLIST_ENTRY ListHead,
    PCUNICODE_STRING FileName,
    ULONG Pid
);

// Removes a File from the MAP
NTSTATUS
HdDeleteHiddenFileFromMap(
    PLIST_ENTRY ListHead,
    PCUNICODE_STRING FileName
);

// Clears a Hidden File Map
VOID
HdFreeHiddenFilesMap(
    PLIST_ENTRY ListHead
);

// Returns whether a file is currently hiding from any process (aka: the map has any key with FileName)
BOOLEAN
HdIsFileInMap(
    PLIST_ENTRY ListHead,
    PCUNICODE_STRING FileName
);

// Returns whether the given Pid is hidden to a given FileName
BOOLEAN
HdIsFileHiddenToPid(
    PLIST_ENTRY ListHead,
    PCUNICODE_STRING FileName,
    ULONG Pid
);

// For each FileName, the whole list of PIDs is printed.
VOID
HdPrintHiddenFilesMap(
    PLIST_ENTRY ListHead
);

// Retrieves a Hidden File Entry with the given FileName
PHIDDEN_FILE_ENTRY
HdGetEntryFromMap(
    PLIST_ENTRY ListHead,
    PCUNICODE_STRING FileName
);

LONG
HdGetHiddenFilesListSize(
    PLIST_ENTRY ListHead
);

#endif
