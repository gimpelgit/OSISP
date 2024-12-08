#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <chrono>
#include "thread_pool.h"


void processData(std::string& data) {
    std::sort(data.begin(), data.end()); // Сортировка символов
}


void asyncReadWriteImproved(HANDLE hInputFile, HANDLE hOutputFile, DWORD bufferSize, 
    int numOperations, DWORD numberStr, DWORD sizeStr)
{
    std::vector<OVERLAPPED> overlapped(numOperations);
    std::vector<char*> buffers(numOperations, nullptr);
    std::vector<HANDLE> events(numOperations);
    DWORD fileSize = GetFileSize(hInputFile, NULL);
    DWORD totalBytesProcessed = 0;

    for (int i = 0; i < numOperations; ++i) {
        buffers[i] = new char[bufferSize + 1] {};
				// нужно сбрасывать вручную, при создании событие не
				// сообщает о том, что произошло какое-то действие, у события
				// нет имени
        events[i] = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!events[i]) {
            std::cerr << "Failed to create event: " << GetLastError() << std::endl;
            return;
        }
    }

    DWORD startOffset = (numberStr - 1) * (sizeStr + 2);

    for (int i = 0; i < numOperations && totalBytesProcessed < fileSize; ++i) {
        ZeroMemory(&overlapped[i], sizeof(OVERLAPPED));
        overlapped[i].Offset = startOffset + totalBytesProcessed;
        overlapped[i].hEvent = events[i];

        DWORD bytesRead = 0;
        if (!ReadFile(hInputFile, buffers[i], bufferSize, NULL, &overlapped[i])) {
            DWORD error = GetLastError();
            if (error != ERROR_IO_PENDING) {
                std::cerr << "ReadFile failed: " << error << std::endl;
                break;
            }
        }

        totalBytesProcessed += bufferSize + 2;
    }

    for (int completedCount = 0; completedCount < numOperations; ++completedCount) {
        // Ожидание завершения операций
        DWORD waitResult = WaitForMultipleObjects(numOperations, events.data(), FALSE, INFINITE);
        int completedIndex = waitResult - WAIT_OBJECT_0;
        
        DWORD bytesTransferred = 0;
        if (!GetOverlappedResult(hInputFile, &overlapped[completedIndex], &bytesTransferred, FALSE)) {
            std::cerr << "GetOverlappedResult failed: " << GetLastError() << std::endl;
            break;
        }

        // Обработка данных
        std::string data(buffers[completedIndex], bytesTransferred);
        processData(data);
        data += "\r\n";


        if (!WriteFile(hOutputFile, data.c_str(), bytesTransferred + 2, NULL, &overlapped[completedIndex])) {
            DWORD error = GetLastError();
            if (error != ERROR_IO_PENDING) {
                std::cerr << "WriteFile failed: " << error << std::endl;
                break;
            }
        }

        if (!GetOverlappedResult(hOutputFile, &overlapped[completedIndex], &bytesTransferred, TRUE)) {
            std::cerr << "GetOverlappedResult (write) failed: " << GetLastError() << std::endl;
            break;
        }

        // Сброс события
        ResetEvent(events[completedIndex]);
    }

    for (int i = 0; i < numOperations; ++i) {
        delete[] buffers[i];
        CloseHandle(events[i]);
    }
}


int main() {
    const std::wstring inputFileName = L"input.dat";
    const std::wstring outputFileName = L"output.dat";
    const DWORD sizeStr = 256;
    const DWORD bufferSize = sizeStr;

    ThreadPool tpool(5);


    // Открытие файлов
    HANDLE hInputFile = CreateFileW(inputFileName.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    HANDLE hOutputFile = CreateFileW(outputFileName.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_OVERLAPPED, NULL);

    if (hInputFile == INVALID_HANDLE_VALUE || hOutputFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open files: " << GetLastError() << std::endl;
        return 1;
    }
    
    DWORD fileSize = GetFileSize(hInputFile, NULL);
    DWORD countStr = fileSize / (sizeStr + 2);

    auto start = std::chrono::high_resolution_clock::now();
    tpool.start();

    // здесь задачи добавляются в пул потоков
    for (int i = 1; i <= countStr / 2; ++i) {
        tpool.push_task(asyncReadWriteImproved, hInputFile, hOutputFile, bufferSize, 2, 2*i - 1, sizeStr);
    }
    if (countStr % 2 != 0) {
        tpool.push_task(asyncReadWriteImproved, hInputFile, hOutputFile, bufferSize, 1, countStr, sizeStr);
    }
    tpool.stop();
    auto end = std::chrono::high_resolution_clock::now();


    std::cout << "Processing time: "
        << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
        << " ms" << std::endl;

    CloseHandle(hInputFile);
    CloseHandle(hOutputFile);

    return 0;
}
