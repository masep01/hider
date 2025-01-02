#include "hider.h"
#include <ntstrsafe.h>

typedef struct _FILE_PID_USER_DATA {
    WCHAR FileName[260];
    ULONG Pid;
} FILE_PID_USER_DATA, * PFILE_PID_USER_DATA;

NTSTATUS
FilterDriverCreate(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
) {
    UNREFERENCED_PARAMETER(DeviceObject);

    PAGED_CODE();

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

NTSTATUS
FilterDeviceControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
) {
    UNREFERENCED_PARAMETER(DeviceObject);

    NTSTATUS Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION Stack;
    ULONG ControlCode;
    ULONG_PTR Info = 0;
    PVOID InputBuffer;
    ULONG InputBufferLength;

    Stack = IoGetCurrentIrpStackLocation(Irp);
    ControlCode = Stack->Parameters.DeviceIoControl.IoControlCode;

    InputBuffer = Irp->AssociatedIrp.SystemBuffer;
    InputBufferLength = Stack->Parameters.DeviceIoControl.InputBufferLength;

    switch (ControlCode) {
        case IOCTL_ENABLE_DRIVER:
            DriverEnabled = TRUE;
            DbgPrint("[USER]: User enabled driver.\r\n");

            break;

        case IOCTL_DISABLE_DRIVER:
            DriverEnabled = FALSE;
            DbgPrint("[USER]: User disabled driver.\r\n");
            break;

        case IOCTL_ADD_FILE:
            if (InputBuffer && InputBufferLength > 0) {
                UNICODE_STRING FileName;
                RtlInitUnicodeString(&FileName, (PCWSTR)InputBuffer);

                Status = HdInitializeFileEntry(&HiddenFilesMap, &FileName);
                if (NT_SUCCESS(Status)) {
                    DbgPrint("[USER]: File added to hidden map: %wZ\r\n", &FileName);
                }
                else {
                    DbgPrint("[USER]: Failed to add file to hidden map. Status: 0x%X\r\n", Status);
                }
            }
            else {
                Status = STATUS_INVALID_PARAMETER;
                DbgPrint("[USER]: Invalid input for Add File.\r\n");
            }

            HdPrintHiddenFilesMap(&HiddenFilesMap);
            break;

        case IOCTL_REMOVE_FILE:
            if (InputBuffer && InputBufferLength > 0) {
                UNICODE_STRING FileName;
                RtlInitUnicodeString(&FileName, (PCWSTR)InputBuffer);

                Status = HdDeleteHiddenFileFromMap(&HiddenFilesMap, &FileName);
                if (NT_SUCCESS(Status)) {
                    DbgPrint("[USER]: File removed from hidden map: %wZ\r\n", &FileName);
                }
                else {
                    DbgPrint("[USER]: Failed to remove file from hidden map. Status: 0x%X\r\n", Status);
                }
            }
            else {
                Status = STATUS_INVALID_PARAMETER;
                DbgPrint("[USER]: Invalid input for Remove File.\r\n" );
            }

            HdPrintHiddenFilesMap(&HiddenFilesMap);
            break;

        case IOCTL_ADD_PID:
            if (InputBuffer && InputBufferLength >= sizeof(FILE_PID_USER_DATA)) {
                PFILE_PID_USER_DATA Data = (PFILE_PID_USER_DATA)InputBuffer;
                UNICODE_STRING FileName;

                RtlInitUnicodeString(&FileName, Data->FileName);

                if (!HdIsFileInMap(&HiddenFilesMap, &FileName)) {
                    Status = HdInitializeFileEntry(&HiddenFilesMap, &FileName);
                    if (!NT_SUCCESS(Status)) {
                        DbgPrint("[USER]: Failed to add file to hidden map. Status: 0x%X\r\n", Status);
                        break;
                    }
                }

                Status = HdAddPidToHiddenFile(&HiddenFilesMap, &FileName, Data->Pid);
                if (NT_SUCCESS(Status)) {
                    DbgPrint("[USER]: PID %lu added to file: %wZ\r\n", Data->Pid, &FileName);
                
                } 
                else {
                    DbgPrint("[USER]: Failed to add PID to file. Status: 0x%X\r\n", Status);
                }
            }
            else {
                Status = STATUS_INVALID_PARAMETER;
                DbgPrint("[USER]: Invalid input for Add PID.\r\n");
            }

            HdPrintHiddenFilesMap(&HiddenFilesMap);
            break;

        case IOCTL_REMOVE_PID:
            if (InputBuffer && InputBufferLength >= sizeof(FILE_PID_USER_DATA)) {
                PFILE_PID_USER_DATA Data = (PFILE_PID_USER_DATA)InputBuffer;
                UNICODE_STRING FileName;

                RtlInitUnicodeString(&FileName, Data->FileName);

                if (!HdIsFileInMap(&HiddenFilesMap, &FileName)) {
                    DbgPrint("[USER]: Failed to remove PID from file: File not existing.\r\n");
                    break;
                }

                Status = HdRemovePidFromHiddenFile(&HiddenFilesMap, &FileName, Data->Pid);
                if (NT_SUCCESS(Status)) {
                    DbgPrint("[USER]: PID %lu removed from file: %wZ\r\n", Data->Pid, &FileName);
                
                } 
                else {
                    DbgPrint("[USER]: Failed to remove PID from file. Status: 0x%X\r\n", Status);
                }
            }
            else {
                Status = STATUS_INVALID_PARAMETER;
                DbgPrint("[USER]: Invalid input for Remove PID.\r\n");
            }

            HdPrintHiddenFilesMap(&HiddenFilesMap);
            break;

        default:
            Status = STATUS_INVALID_DEVICE_REQUEST;
            DbgPrint("[USER]: Unsupported IOCTL: 0x%X\r\n", ControlCode);
            break;
    }

    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = Info;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}