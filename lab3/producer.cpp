#include <windows.h>
#include <stdio.h>

#define SHARED_MEMORY_NAME L"SharedMemoryExample"
#define MUTEX_NAME L"SharedMemoryMutex"
#define BUFFER_SIZE 256

int main() {
    SetConsoleOutputCP(CP_UTF8);
    HANDLE hMapFile;
    HANDLE hMutex;
    LPVOID pBuf;

    hMapFile = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        BUFFER_SIZE,
        SHARED_MEMORY_NAME
    );

    if (hMapFile == NULL) {
        printf("Не удалось создать разделяемую память. Код ошибки: %d\n", GetLastError());
        return 1;
    }

    hMutex = CreateMutexW(NULL, FALSE, MUTEX_NAME);
    if (hMutex == NULL) {
        printf("Не удалось создать мьютекс. Код ошибки: %d\n", GetLastError());
        CloseHandle(hMapFile);
        return 1;
    }

    pBuf = MapViewOfFile(
        hMapFile,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        BUFFER_SIZE
    );

    if (pBuf == NULL) {
        printf("Не удалось отобразить разделяемую память. Код ошибки: %d\n", GetLastError());
        CloseHandle(hMapFile);
        CloseHandle(hMutex);
        return 1;
    }

    printf("Введите данные (\"exit\" для выхода):\n");
    char input[BUFFER_SIZE];

    while (1) {
        printf("> ");
        fgets(input, BUFFER_SIZE, stdin);

        input[strcspn(input, "\n")] = 0;

        WaitForSingleObject(hMutex, INFINITE);

        memcpy(pBuf, input, BUFFER_SIZE);

        ReleaseMutex(hMutex);
        
        if (strcmp(input, "exit") == 0) break;
    }

    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);
    CloseHandle(hMutex);

    return 0;
}
