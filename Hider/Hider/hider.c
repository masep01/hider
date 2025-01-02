#include "hider.h"

PFLT_FILTER FilterHandle = NULL;
LIST_ENTRY HiddenFilesMap;
PDEVICE_OBJECT gDeviceObject;
BOOLEAN DriverEnabled;

const FLT_OPERATION_REGISTRATION Callbacks[] = {
    {IRP_MJ_CREATE, 0,  HdPreCreateCallback, NULL},
    {IRP_MJ_NETWORK_QUERY_OPEN, 0,  HdPreNetworkQueryCallback, NULL},
    {IRP_MJ_FAST_IO_CHECK_IF_POSSIBLE, 0,  HdPreFastIOCheck, NULL},
    {IRP_MJ_QUERY_INFORMATION, 0,  HdPreQueryInfo, NULL},
    {IRP_MJ_SET_INFORMATION, 0,  HdPreSetInfo, NULL},
    {IRP_MJ_DIRECTORY_CONTROL, 0,  HdPreDirCtrl, HdPostDirCtrl},
    {IRP_MJ_OPERATION_END}
};

const FLT_REGISTRATION FilterRegistration = {   // ----------------------- //
    sizeof(FLT_REGISTRATION),                   // Size                    //
    FLT_REGISTRATION_VERSION,                   // Version                 //
    0,                                          // Flags                   //
    NULL,                                       // *ContextRegistration    //
    Callbacks,                                  // *OperationRegistration  //
    UnloadCallback,                             // FilterUnloadCallback    // 
    NULL,                                       // ----------------------- //
    NULL,                                       //                         //
    NULL,                                       //                         //
    NULL,                                       //                         //
    NULL,                                       //        NOT USED         //
    NULL,                                       //                         //
    NULL,                                       //                         //
    NULL                                        //                         //
};                                              // ----------------------- //

NTSTATUS UnloadCallback(
    FLT_FILTER_UNLOAD_FLAGS Flags
){
    UNREFERENCED_PARAMETER(Flags);

    NTSTATUS Status;
    UNICODE_STRING DosDeviceNameString = RTL_CONSTANT_STRING(DOS_DEVICE_NAME);
    
    Status = IoDeleteSymbolicLink(&DosDeviceNameString);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("[!] Failed to delete device symlink with code:%08x.\r\n\r\n", Status);
        return Status;
    }

    if (gDeviceObject != NULL) {
        IoDeleteDevice(gDeviceObject);
    }
    
    HdFreeHiddenFilesMap(&HiddenFilesMap);

    FltUnregisterFilter(FilterHandle);

    DbgPrint("\r\n[*] Driver successfully unloaded.\r\n\r\n");
    return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(
    PDRIVER_OBJECT DriverObject, 
    PUNICODE_STRING RegistryPath
){
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS        Status;
    UNICODE_STRING  NtDeviceNameString = RTL_CONSTANT_STRING(NT_DEVICE_NAME);
    UNICODE_STRING  DosDeviceNameString = RTL_CONSTANT_STRING(DOS_DEVICE_NAME);
    PDEVICE_OBJECT  DeviceObject = NULL;

    /*
    * Driver disabled by default.
    */
    DriverEnabled = FALSE;

    /*
    * Create Virtual Device
    */
    Status = IoCreateDevice(
        DriverObject,                   // Our Driver Object
        0,                              // We don't use a device extension
        &NtDeviceNameString,            // Device name "\Device\SIOCTL"
        FILE_DEVICE_UNKNOWN,            // Device type
        0,                              // Device characteristics
        FALSE,                          // Not an exclusive device
        &DeviceObject
    );

    if (!NT_SUCCESS(Status)) {
        DbgPrint("[!] Driver failed creating virtual device with code:%08x.\r\n\r\n", Status);
        return Status;
    }

    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = FilterDeviceControl;
    DriverObject->MajorFunction[IRP_MJ_CREATE]         = FilterDriverCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]          = FilterDriverCreate;

    Status = IoCreateSymbolicLink(&DosDeviceNameString, &NtDeviceNameString);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("[!] Driver failed creating symbolic link between Device Names with code:%08x.\r\n\r\n", Status);
        IoDeleteDevice(DeviceObject);
        return Status;
    }

    gDeviceObject = DeviceObject;

    DbgPrint("[*] Virtual device successfully created.\r\n");

    /* 
    * Initialize list
    */

    InitializeListHead(&HiddenFilesMap);
    DbgPrint("[*] Hidden Files Map successfully initialized.\r\n");
    HdPrintHiddenFilesMap(&HiddenFilesMap);

    /* 
    * Register filter to FltMgr 
    */
    Status = FltRegisterFilter(DriverObject, &FilterRegistration, &FilterHandle);
    if (!NT_SUCCESS(Status)) {
        HdFreeHiddenFilesMap(&HiddenFilesMap);
        return Status;
    }

    DbgPrint("[*] Driver successfully registered.\r\n");

   /* 
   * Start filtering 
   */
    Status = FltStartFiltering(FilterHandle);
    if (!NT_SUCCESS(Status)) {
        FltUnregisterFilter(FilterHandle);
        return Status;
    }

    DbgPrint("[*] Driver started filtering.\r\n");

    return Status;
}