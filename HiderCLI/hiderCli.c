#include <windows.h>
#include <stdio.h>

#define IOCTL_ENABLE_DRIVER         CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DISABLE_DRIVER        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_ADD_FILE              CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_REMOVE_FILE           CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_ADD_PID               CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_REMOVE_PID            CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define DEVICE_NAME "\\\\.\\HiderDosDevice"

typedef struct _FILE_PID_DATA {
    WCHAR FileName[260];
    ULONG Pid;
} FILE_PID_DATA, * PFILE_PID_DATA;

void PrintMenu() {
    printf("\r\n");
    printf("    + -------------------------------- +\r\n");
    printf("    +            Hider Menu            +\r\n");
    printf("    + -------------------------------- +\r\n");
    printf("    +   1. Enable Driver               +\r\n");
    printf("    +   2. Disable Driver              +\r\n");
    printf("    +                                  +\r\n");
    printf("    +   3. Add File                    +\r\n");
    printf("    +   4. Remove File                 +\r\n");
    printf("    +                                  +\r\n");
    printf("    +   5. Add PID                     +\r\n");
    printf("    +   6. Remove PID                  +\r\n");
    printf("    +                                  +\r\n");
    printf("    +   0. Exit                        +\r\n");
    printf("    + -------------------------------- +\r\n");
    printf("\r\n");
    printf("    [>] Select an option: ");
}

void SendIoctl(HANDLE DeviceHandle, DWORD IoctlCode, LPCVOID InputBuffer, DWORD InputBufferSize) {
    DWORD bytesReturned;
    BOOL result = DeviceIoControl(
        DeviceHandle,
        IoctlCode,
        (LPVOID)InputBuffer, InputBufferSize,
        NULL, 0,
        &bytesReturned,
        NULL
    );

    if (result) {
        printf("    [*] IOCTL 0x%X sent successfully.\r\n\r\n", IoctlCode);
    } else {
        printf("    [!] IOCTL 0x%X failed. Error: %d\r\n\r\n", IoctlCode, GetLastError());
    }
}

int main() {
    int choice;
    WCHAR fileName[260];
    DWORD pid;
    FILE_PID_DATA filePidData;

    HANDLE deviceHandle = CreateFileA(
        DEVICE_NAME,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (deviceHandle == INVALID_HANDLE_VALUE) {
        printf("    [!] Failed to open device. Error: %d\n", GetLastError());
        CloseHandle(deviceHandle);
        return 1;
    }

    system("cls");
    printf("\r\n    [*] Device opened successfully.\r\n");

    do {
        PrintMenu();
        scanf("%d", &choice);

        switch (choice) {
            case 1:
                SendIoctl(deviceHandle, IOCTL_ENABLE_DRIVER, NULL, 0);
                break;

            case 2:
                SendIoctl(deviceHandle, IOCTL_DISABLE_DRIVER, NULL, 0);
                break;

            case 3:
                wprintf(L"    [+] Enter file name to add: ");
                wscanf(L"%259ls", fileName);
                SendIoctl(deviceHandle, IOCTL_ADD_FILE, fileName, (wcslen(fileName) + 1) * sizeof(WCHAR));
                break;

            case 4:
                wprintf(L"    [-] Enter file name to remove: ");
                wscanf(L"%259ls", fileName);
                SendIoctl(deviceHandle, IOCTL_REMOVE_FILE, fileName, (wcslen(fileName) + 1) * sizeof(WCHAR));
                break;

            case 5:
                wprintf(L"    [+] Enter file name to associate PID with: ");
                wscanf(L"%259ls", fileName);
                wprintf(L"    [+] Enter PID to add: ");
                scanf("%lu", &pid);

                wcscpy_s(filePidData.FileName, _countof(filePidData.FileName), fileName);
                filePidData.Pid = pid;

                SendIoctl(deviceHandle, IOCTL_ADD_PID, &filePidData, sizeof(filePidData));
                break;

            case 6:
                wprintf(L"    [+] Enter file name to disassociate PID from: ");
                wscanf(L"%259ls", fileName);
                wprintf(L"    [+] Enter PID to remove: ");
                scanf("%lu", &pid);

                wcscpy_s(filePidData.FileName, _countof(filePidData.FileName), fileName);
                filePidData.Pid = pid;

                SendIoctl(deviceHandle, IOCTL_REMOVE_PID, &filePidData, sizeof(filePidData));
                break;

            case 0:
                printf("    [/] Exiting...\r\n");
                break;

            default:
                printf("    [!] Invalid option. Please try again.\r\n");
                break;
        }

    } while (choice != 0);

    CloseHandle(deviceHandle);
    printf("    [/] Device handle closed.\r\n\r\n");

    return 0;
}
