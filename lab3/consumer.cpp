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

    
    hMapFile = OpenFileMappingW(
        FILE_MAP_ALL_ACCESS,
        FALSE,
        SHARED_MEMORY_NAME 
    );

    if (hMapFile == NULL) {
        printf("Не удалось открыть разделяемую память. Код ошибки: %d\n", GetLastError());
        return 1;
    }

    
    hMutex = OpenMutexW(
        MUTEX_ALL_ACCESS,
        FALSE,
        MUTEX_NAME
    );

    if (hMutex == NULL) {
        printf("Не удалось открыть мьютекс. Код ошибки: %d\n", GetLastError());
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

    printf("Ожидание данных (\"exit\" для выхода):\n");

    char buffer[BUFFER_SIZE];
    bool isWait = false;
    while (1) {
        WaitForSingleObject(hMutex, INFINITE);

        memcpy(buffer, pBuf, BUFFER_SIZE);
        memset(pBuf, 0, BUFFER_SIZE);

        ReleaseMutex(hMutex);

        bool isEmpty = strcmp(buffer, "") == 0;
        if (!isWait && isEmpty) {
            printf("Ждем...\n");
            isWait = true;
        }
        else if (!isEmpty) {
            printf("Прочитано: %s\n", buffer);
            isWait = false;
        }

        if (strcmp(buffer, "exit") == 0) {
            printf("Завершаю работу\n");
            break;
        }
    }

    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);
    CloseHandle(hMutex);

    return 0;
}
