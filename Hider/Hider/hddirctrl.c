#include "hider.h"
/*
*  + ---------------------------------------------------------------------------------- +
*  |              PreOperation Callback for IRP_MJ_DIRECTORY_CONTROL                    |
*  + ---------------------------------------------------------------------------------- +
*  |                                                                                    |
*  |    a) We only treat Minor Function Query Directory.                                |
*  |                                                                                    |
*  |                                                                                    |
*  + ---------------------------------------------------------------------------------- +
*/

FLT_PREOP_CALLBACK_STATUS
HdPreDirCtrl(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID* CompletionContext
){
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);

	if (Data->Iopb->MinorFunction != IRP_MN_QUERY_DIRECTORY) {
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	// Check if driver is enabled.
	if (!DriverEnabled) {
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	switch (Data->Iopb->Parameters.DirectoryControl.QueryDirectory.FileInformationClass)
	{
		case FileIdFullDirectoryInformation:
		case FileIdBothDirectoryInformation:
		case FileBothDirectoryInformation:
		case FileDirectoryInformation:
		case FileFullDirectoryInformation:
		case FileNamesInformation:
			break;
		default:
			return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

FLT_POSTOP_CALLBACK_STATUS
HdPostDirCtrl(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
){
	PFLT_PARAMETERS params = &Data->Iopb->Parameters;
	PFLT_FILE_NAME_INFORMATION fltName;
	FILE_OBJECT FileObject = *FltObjects->FileObject;
	NTSTATUS status;
	
	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(Flags);

	if (!NT_SUCCESS(Data->IoStatus.Status))
		return FLT_POSTOP_FINISHED_PROCESSING;

	status = HdGetFileNameInformation(
		NULL,
		&FileObject,
		NULL,
		FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT | FLT_FILE_NAME_DO_NOT_CACHE,
		&fltName
	);

	if (!NT_SUCCESS(status)) {
		return FLT_POSTOP_FINISHED_PROCESSING;
	}

	__try
	{
		status = STATUS_SUCCESS;

		switch (params->DirectoryControl.QueryDirectory.FileInformationClass)
		{
			case FileFullDirectoryInformation:
				status = CleanFileFullDirectoryInformation(
					(PFILE_FULL_DIR_INFORMATION)params->DirectoryControl.QueryDirectory.DirectoryBuffer, 
					Data, fltName
				);
				break;
			case FileBothDirectoryInformation:
				status = CleanFileBothDirectoryInformation(
					(PFILE_BOTH_DIR_INFORMATION)params->DirectoryControl.QueryDirectory.DirectoryBuffer, 
					Data, fltName
				);
				break;
			case FileDirectoryInformation:
				status = CleanFileDirectoryInformation(
					(PFILE_DIRECTORY_INFORMATION)params->DirectoryControl.QueryDirectory.DirectoryBuffer, 
					Data, fltName
				);
				break;
			case FileIdFullDirectoryInformation:
				status = CleanFileIdFullDirectoryInformation(
					(PFILE_ID_FULL_DIR_INFORMATION)params->DirectoryControl.QueryDirectory.DirectoryBuffer,
					Data, fltName
				);
				break;
			case FileIdBothDirectoryInformation:
				status = CleanFileIdBothDirectoryInformation(
					(PFILE_ID_BOTH_DIR_INFORMATION)params->DirectoryControl.QueryDirectory.DirectoryBuffer,
					Data, fltName
				);
				break;
			case FileNamesInformation:
				status = CleanFileNamesInformation(
					(PFILE_NAMES_INFORMATION)params->DirectoryControl.QueryDirectory.DirectoryBuffer,
					Data, fltName
				);
				break;
		}

		Data->IoStatus.Status = status;
	}
	__finally
	{
		FltReleaseFileNameInformation(fltName);
	}

	return FLT_POSTOP_FINISHED_PROCESSING;
}

NTSTATUS CleanFileFullDirectoryInformation(
	PFILE_FULL_DIR_INFORMATION Info,
	PFLT_CALLBACK_DATA Data,
	PFLT_FILE_NAME_INFORMATION DirInfo
) {
	PFILE_FULL_DIR_INFORMATION NextInfo = NULL;
	PFILE_FULL_DIR_INFORMATION PrevInfo = NULL;
	UNICODE_STRING FileName;
	UNICODE_STRING FullFileName;
	WCHAR Buffer[256];
	UINT32 Offset;
	UINT32 MoveLength;
	BOOLEAN Matched, Search;
	NTSTATUS Status = STATUS_SUCCESS;
	ULONG RequestorPid;

	RequestorPid = FltGetRequestorProcessId(Data);
	
	FullFileName.Buffer = Buffer;
	FullFileName.Length = 0;
	FullFileName.MaximumLength = sizeof(Buffer);

	Offset = 0;
	Search = TRUE;

	do {
		FileName.Buffer = Info->FileName;
		FileName.Length = (USHORT)Info->FileNameLength;
		FileName.MaximumLength = (USHORT)Info->FileNameLength;

		RtlCopyUnicodeString(&FullFileName, &DirInfo->Name);
		Status = HdBuildFullFileName(&FullFileName, &FileName);
		if (!NT_SUCCESS(Status)) {
			DbgPrint("[!] Failed to append parent dir name to filename.\r\n");
			return Status;
		}

		Matched = HdIsFileHiddenToPid(&HiddenFilesMap, &FullFileName, RequestorPid);

		if (Matched) {
			BOOLEAN retn = FALSE;

			if (PrevInfo != NULL) {
				if (Info->NextEntryOffset != 0) {
					PrevInfo->NextEntryOffset += Info->NextEntryOffset;
					Offset = Info->NextEntryOffset;
				}
				else {
					PrevInfo->NextEntryOffset = 0;
					Status = STATUS_SUCCESS;
					retn = TRUE;
				}
				RtlFillMemory(Info, sizeof(FILE_FULL_DIR_INFORMATION), 0);
			}
			else {
				if (Info->NextEntryOffset != 0) {
					NextInfo = (PFILE_FULL_DIR_INFORMATION)((PUCHAR)Info + Info->NextEntryOffset);
					MoveLength = 0;

					while (NextInfo->NextEntryOffset != 0) {
						MoveLength += NextInfo->NextEntryOffset;
						NextInfo = (PFILE_FULL_DIR_INFORMATION)((PUCHAR)NextInfo + NextInfo->NextEntryOffset);
					}

					MoveLength += FIELD_OFFSET(FILE_FULL_DIR_INFORMATION, FileName) + NextInfo->FileNameLength;
					RtlMoveMemory(Info, (PUCHAR)Info + Info->NextEntryOffset, MoveLength);
				}
				else {
					Status = STATUS_NO_MORE_ENTRIES;
					retn = TRUE;
				}
			}

			if (retn) {
				return Status;
			}

			Info = (PFILE_FULL_DIR_INFORMATION)((PCHAR)Info + Offset);
			continue;
		}

		Offset = Info->NextEntryOffset;
		PrevInfo = Info;
		Info = (PFILE_FULL_DIR_INFORMATION)((PCHAR)Info + Offset);

		if (Offset == 0) {
			Search = FALSE;
		}

	} while (Search);

	return STATUS_SUCCESS;
}

NTSTATUS CleanFileBothDirectoryInformation(
	PFILE_BOTH_DIR_INFORMATION Info,
	PFLT_CALLBACK_DATA Data,
	PFLT_FILE_NAME_INFORMATION DirInfo
) {
	PFILE_BOTH_DIR_INFORMATION NextInfo = NULL;
	PFILE_BOTH_DIR_INFORMATION PrevInfo = NULL;
	UNICODE_STRING FileName;
	UNICODE_STRING FullFileName;
	WCHAR Buffer[256];
	UINT32 Offset;
	UINT32 MoveLength;
	BOOLEAN Matched;
	BOOLEAN Search;
	BOOLEAN Retn = FALSE;
	NTSTATUS Status = STATUS_SUCCESS;
	ULONG RequestorPid;

	RequestorPid = FltGetRequestorProcessId(Data);
	
	FullFileName.Buffer = Buffer;
	FullFileName.Length = 0;
	FullFileName.MaximumLength = sizeof(Buffer);

	Offset = 0;
	Search = TRUE;

	do {
		FileName.Buffer = Info->FileName;
		FileName.Length = (USHORT)Info->FileNameLength;
		FileName.MaximumLength = (USHORT)Info->FileNameLength;

		RtlCopyUnicodeString(&FullFileName, &DirInfo->Name);
		Status = HdBuildFullFileName(&FullFileName, &FileName);
		if (!NT_SUCCESS(Status)) {
			DbgPrint("[!] Failed to append parent dir name to filename.\r\n");
			return Status;
		}

		Matched = HdIsFileHiddenToPid(&HiddenFilesMap, &FullFileName, RequestorPid);

		if (Matched) {
			Retn = FALSE;

			if (PrevInfo != NULL) {
				if (Info->NextEntryOffset != 0) {
					PrevInfo->NextEntryOffset += Info->NextEntryOffset;
					Offset = Info->NextEntryOffset;
				}
				else {
					PrevInfo->NextEntryOffset = 0;
					Status = STATUS_SUCCESS;
					Retn = TRUE;
				}

				RtlFillMemory(Info, sizeof(FILE_BOTH_DIR_INFORMATION), 0);
			}
			else {
				if (Info->NextEntryOffset != 0) {
					NextInfo = (PFILE_BOTH_DIR_INFORMATION)((PUCHAR)Info + Info->NextEntryOffset);
					MoveLength = 0;
					while (NextInfo->NextEntryOffset != 0) {
						MoveLength += NextInfo->NextEntryOffset;
						NextInfo = (PFILE_BOTH_DIR_INFORMATION)((PUCHAR)NextInfo + NextInfo->NextEntryOffset);
					}

					MoveLength += FIELD_OFFSET(FILE_BOTH_DIR_INFORMATION, FileName) + NextInfo->FileNameLength;
					RtlMoveMemory(Info, (PUCHAR)Info + Info->NextEntryOffset, MoveLength);
				}
				else {
					Status = STATUS_NO_MORE_ENTRIES;
					Retn = TRUE;
				}
			}

			if (Retn) {
				return Status;
			}

			Info = (PFILE_BOTH_DIR_INFORMATION)((PCHAR)Info + Offset);
			continue;
		}

		Offset = Info->NextEntryOffset;
		PrevInfo = Info;
		Info = (PFILE_BOTH_DIR_INFORMATION)((PCHAR)Info + Offset);

		if (Offset == 0) {
			Search = FALSE;
		}

	} while (Search);

	return STATUS_SUCCESS;
}

NTSTATUS CleanFileDirectoryInformation(
	PFILE_DIRECTORY_INFORMATION Info,
	PFLT_CALLBACK_DATA Data,
	PFLT_FILE_NAME_INFORMATION DirInfo
) {
	PFILE_DIRECTORY_INFORMATION NextInfo = NULL;
	PFILE_DIRECTORY_INFORMATION PrevInfo = NULL;
	UNICODE_STRING FileName;
	UNICODE_STRING FullFileName;
	WCHAR Buffer[256];
	UINT32 Offset;
	UINT32 MoveLength;
	BOOLEAN Matched;
	BOOLEAN Search;
	BOOLEAN Retn = FALSE;
	NTSTATUS Status = STATUS_SUCCESS;
	ULONG RequestorPid;

	RequestorPid = FltGetRequestorProcessId(Data);
	
	FullFileName.Buffer = Buffer;
	FullFileName.Length = 0;
	FullFileName.MaximumLength = sizeof(Buffer);

	Offset = 0;
	Search = TRUE;

	do {
		FileName.Buffer = Info->FileName;
		FileName.Length = (USHORT)Info->FileNameLength;
		FileName.MaximumLength = (USHORT)Info->FileNameLength;

		RtlCopyUnicodeString(&FullFileName, &DirInfo->Name);
		Status = HdBuildFullFileName(&FullFileName, &FileName);
		if (!NT_SUCCESS(Status)) {
			DbgPrint("[!] Failed to append parent dir name to filename.\r\n");
			return Status;
		}

		Matched = HdIsFileHiddenToPid(&HiddenFilesMap, &FullFileName, RequestorPid);

		if (Matched) {
			Retn = FALSE;

			if (PrevInfo != NULL) {
				if (Info->NextEntryOffset != 0) {
					PrevInfo->NextEntryOffset += Info->NextEntryOffset;
					Offset = Info->NextEntryOffset;
				}
				else {
					PrevInfo->NextEntryOffset = 0;
					Status = STATUS_SUCCESS;
					Retn = TRUE;
				}

				RtlFillMemory(Info, sizeof(FILE_DIRECTORY_INFORMATION), 0);
			}
			else {
				if (Info->NextEntryOffset != 0) {
					NextInfo = (PFILE_DIRECTORY_INFORMATION)((PUCHAR)Info + Info->NextEntryOffset);
					MoveLength = 0;
					while (NextInfo->NextEntryOffset != 0) {
						MoveLength += NextInfo->NextEntryOffset;
						NextInfo = (PFILE_DIRECTORY_INFORMATION)((PUCHAR)NextInfo + NextInfo->NextEntryOffset);
					}

					MoveLength += FIELD_OFFSET(FILE_DIRECTORY_INFORMATION, FileName) + NextInfo->FileNameLength;
					RtlMoveMemory(Info, (PUCHAR)Info + Info->NextEntryOffset, MoveLength);
				}
				else {
					Status = STATUS_NO_MORE_ENTRIES;
					Retn = TRUE;
				}
			}

			if (Retn) {
				return Status;
			}

			Info = (PFILE_DIRECTORY_INFORMATION)((PCHAR)Info + Offset);
			continue;
		}

		Offset = Info->NextEntryOffset;
		PrevInfo = Info;
		Info = (PFILE_DIRECTORY_INFORMATION)((PCHAR)Info + Offset);

		if (Offset == 0) {
			Search = FALSE;
		}

	} while (Search);

	return STATUS_SUCCESS;
}

NTSTATUS CleanFileIdFullDirectoryInformation(
	PFILE_ID_FULL_DIR_INFORMATION Info,
	PFLT_CALLBACK_DATA Data,
	PFLT_FILE_NAME_INFORMATION DirInfo
) {
	PFILE_ID_FULL_DIR_INFORMATION NextInfo = NULL;
	PFILE_ID_FULL_DIR_INFORMATION PrevInfo = NULL;
	UNICODE_STRING FileName;
	UNICODE_STRING FullFileName;
	WCHAR Buffer[256];
	UINT32 Offset;
	UINT32 MoveLength;
	BOOLEAN Matched;
	BOOLEAN Search;
	BOOLEAN Retn = FALSE;
	NTSTATUS Status = STATUS_SUCCESS;
	ULONG RequestorPid;

	RequestorPid = FltGetRequestorProcessId(Data);

	FullFileName.Buffer = Buffer;
	FullFileName.Length = 0;
	FullFileName.MaximumLength = sizeof(Buffer);

	Offset = 0;
	Search = TRUE;

	do {
		FileName.Buffer = Info->FileName;
		FileName.Length = (USHORT)Info->FileNameLength;
		FileName.MaximumLength = (USHORT)Info->FileNameLength;

		RtlCopyUnicodeString(&FullFileName, &DirInfo->Name);
		Status = HdBuildFullFileName(&FullFileName, &FileName);
		if (!NT_SUCCESS(Status)) {
			DbgPrint("[!] Failed to append parent dir name to filename.\r\n");
			return Status;
		}

		Matched = HdIsFileHiddenToPid(&HiddenFilesMap, &FullFileName, RequestorPid);

		if (Matched) {
			Retn = FALSE;

			if (PrevInfo != NULL) {
				if (Info->NextEntryOffset != 0) {
					PrevInfo->NextEntryOffset += Info->NextEntryOffset;
					Offset = Info->NextEntryOffset;
				}
				else {
					PrevInfo->NextEntryOffset = 0;
					Status = STATUS_SUCCESS;
					Retn = TRUE;
				}

				RtlFillMemory(Info, sizeof(FILE_ID_FULL_DIR_INFORMATION), 0);
			}
			else {
				if (Info->NextEntryOffset != 0) {
					NextInfo = (PFILE_ID_FULL_DIR_INFORMATION)((PUCHAR)Info + Info->NextEntryOffset);
					MoveLength = 0;
					while (NextInfo->NextEntryOffset != 0) {
						MoveLength += NextInfo->NextEntryOffset;
						NextInfo = (PFILE_ID_FULL_DIR_INFORMATION)((PUCHAR)NextInfo + NextInfo->NextEntryOffset);
					}

					MoveLength += FIELD_OFFSET(FILE_ID_FULL_DIR_INFORMATION, FileName) + NextInfo->FileNameLength;
					RtlMoveMemory(Info, (PUCHAR)Info + Info->NextEntryOffset, MoveLength);
				}
				else {
					Status = STATUS_NO_MORE_ENTRIES;
					Retn = TRUE;
				}
			}

			if (Retn) {
				return Status;
			}

			Info = (PFILE_ID_FULL_DIR_INFORMATION)((PCHAR)Info + Offset);
			continue;
		}

		Offset = Info->NextEntryOffset;
		PrevInfo = Info;
		Info = (PFILE_ID_FULL_DIR_INFORMATION)((PCHAR)Info + Offset);

		if (Offset == 0) {
			Search = FALSE;
		}

	} while (Search);

	return STATUS_SUCCESS;
}

NTSTATUS CleanFileIdBothDirectoryInformation(
	PFILE_ID_BOTH_DIR_INFORMATION Info,
	PFLT_CALLBACK_DATA Data,
	PFLT_FILE_NAME_INFORMATION DirInfo
) {
	PFILE_ID_BOTH_DIR_INFORMATION NextInfo = NULL;
	PFILE_ID_BOTH_DIR_INFORMATION PrevInfo = NULL;
	UNICODE_STRING FileName;
	UNICODE_STRING FullFileName;
	WCHAR Buffer[256];
	UINT32 Offset;
	UINT32 MoveLength;
	BOOLEAN Matched;
	BOOLEAN Search;
	BOOLEAN Retn = FALSE;
	NTSTATUS Status = STATUS_SUCCESS;
	ULONG RequestorPid;

	RequestorPid = FltGetRequestorProcessId(Data);

	FullFileName.Buffer = Buffer;
	FullFileName.Length = 0;
	FullFileName.MaximumLength = sizeof(Buffer);

	Offset = 0;
	Search = TRUE;

	do {
		FileName.Buffer = Info->FileName;
		FileName.Length = (USHORT)Info->FileNameLength;
		FileName.MaximumLength = (USHORT)Info->FileNameLength;

		RtlCopyUnicodeString(&FullFileName, &DirInfo->Name);
		Status = HdBuildFullFileName(&FullFileName, &FileName);
		if (!NT_SUCCESS(Status)) {
			DbgPrint("[!] Failed to append parent dir name to filename.\r\n");
			return Status;
		}

		Matched = HdIsFileHiddenToPid(&HiddenFilesMap, &FullFileName, RequestorPid);

		if (Matched) {
			Retn = FALSE;

			if (PrevInfo != NULL) {
				if (Info->NextEntryOffset != 0) {
					PrevInfo->NextEntryOffset += Info->NextEntryOffset;
					Offset = Info->NextEntryOffset;
				}
				else {
					PrevInfo->NextEntryOffset = 0;
					Status = STATUS_SUCCESS;
					Retn = TRUE;
				}

				RtlFillMemory(Info, sizeof(FILE_ID_BOTH_DIR_INFORMATION), 0);
			}
			else {
				if (Info->NextEntryOffset != 0) {
					NextInfo = (PFILE_ID_BOTH_DIR_INFORMATION)((PUCHAR)Info + Info->NextEntryOffset);
					MoveLength = 0;
					while (NextInfo->NextEntryOffset != 0) {
						MoveLength += NextInfo->NextEntryOffset;
						NextInfo = (PFILE_ID_BOTH_DIR_INFORMATION)((PUCHAR)NextInfo + NextInfo->NextEntryOffset);
					}

					MoveLength += FIELD_OFFSET(FILE_ID_BOTH_DIR_INFORMATION, FileName) + NextInfo->FileNameLength;
					RtlMoveMemory(Info, (PUCHAR)Info + Info->NextEntryOffset, MoveLength);
				}
				else {
					Status = STATUS_NO_MORE_ENTRIES;
					Retn = TRUE;
				}
			}

			if (Retn) {
				return Status;
			}

			Info = (PFILE_ID_BOTH_DIR_INFORMATION)((PCHAR)Info + Offset);
			continue;
		}

		Offset = Info->NextEntryOffset;
		PrevInfo = Info;
		Info = (PFILE_ID_BOTH_DIR_INFORMATION)((PCHAR)Info + Offset);

		if (Offset == 0) {
			Search = FALSE;
		}

	} while (Search);

	return STATUS_SUCCESS;
}

NTSTATUS CleanFileNamesInformation(
	PFILE_NAMES_INFORMATION Info,
	PFLT_CALLBACK_DATA Data,
	PFLT_FILE_NAME_INFORMATION DirInfo
) {
	PFILE_NAMES_INFORMATION NextInfo = NULL;
	PFILE_NAMES_INFORMATION PrevInfo = NULL;
	UNICODE_STRING FileName;
	UNICODE_STRING FullFileName;
	WCHAR Buffer[256];
	UINT32 Offset;
	UINT32 MoveLength;
	BOOLEAN Search;
	BOOLEAN Retn = FALSE;
	NTSTATUS Status = STATUS_SUCCESS;
	ULONG RequestorPid;

	RequestorPid = FltGetRequestorProcessId(Data);

	FullFileName.Buffer = Buffer;
	FullFileName.Length = 0;
	FullFileName.MaximumLength = sizeof(Buffer);

	Offset = 0;
	Search = TRUE;

	do {
		FileName.Buffer = Info->FileName;
		FileName.Length = (USHORT)Info->FileNameLength;
		FileName.MaximumLength = (USHORT)Info->FileNameLength;

		RtlCopyUnicodeString(&FullFileName, &DirInfo->Name);
		Status = HdBuildFullFileName(&FullFileName, &FileName);
		if (!NT_SUCCESS(Status)) {
			DbgPrint("[!] Failed to append parent dir name to filename.\r\n");
			return Status;
		}

		if(HdIsFileHiddenToPid(&HiddenFilesMap, &FullFileName, RequestorPid)){
			Retn = FALSE;

			if (PrevInfo != NULL) {
				if (Info->NextEntryOffset != 0) {
					PrevInfo->NextEntryOffset += Info->NextEntryOffset;
					Offset = Info->NextEntryOffset;
				}
				else {
					PrevInfo->NextEntryOffset = 0;
					Status = STATUS_SUCCESS;
					Retn = TRUE;
				}

				RtlFillMemory(Info, sizeof(FILE_NAMES_INFORMATION), 0);
			}
			else {
				if (Info->NextEntryOffset != 0) {
					NextInfo = (PFILE_NAMES_INFORMATION)((PUCHAR)Info + Info->NextEntryOffset);
					MoveLength = 0;
					while (NextInfo->NextEntryOffset != 0) {
						MoveLength += NextInfo->NextEntryOffset;
						NextInfo = (PFILE_NAMES_INFORMATION)((PUCHAR)NextInfo + NextInfo->NextEntryOffset);
					}

					MoveLength += FIELD_OFFSET(FILE_NAMES_INFORMATION, FileName) + NextInfo->FileNameLength;
					RtlMoveMemory(Info, (PUCHAR)Info + Info->NextEntryOffset, MoveLength);
				}
				else {
					Status = STATUS_NO_MORE_ENTRIES;
					Retn = TRUE;
				}
			}

			if (Retn) {
				return Status;
			}

			Info = (PFILE_NAMES_INFORMATION)((PCHAR)Info + Offset);
			continue;
		}

		Offset = Info->NextEntryOffset;
		PrevInfo = Info;
		Info = (PFILE_NAMES_INFORMATION)((PCHAR)Info + Offset);

		if (Offset == 0) {
			Search = FALSE;
		}

	} while (Search);

	return STATUS_SUCCESS;
}