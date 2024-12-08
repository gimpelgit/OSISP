#include <windows.h>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>


CRITICAL_SECTION cs;
HANDLE hMutex;
HANDLE hSemaphoreReaders;
HANDLE hSemaphoreWriters;
std::vector<int> sharedData;
int numReaders = 5;
int numWriters = 2;
int maxReaders = 3;
int maxWriters = 1;

void WriterThread(int id) {
    while (true) {
        WaitForSingleObject(hSemaphoreWriters, INFINITE);
        EnterCriticalSection(&cs);
        
        sharedData.push_back(id);
        std::cout << "Writer " << id << " wrote data\n";
        LeaveCriticalSection(&cs);
        ReleaseSemaphore(hSemaphoreWriters, 1, NULL);
        Sleep(1000); 
    }
}


void ReaderThread(int id) {
    while (true) {
        WaitForSingleObject(hSemaphoreReaders, INFINITE);
        EnterCriticalSection(&cs);
        
        std::cout << "Reader " << id << " read data: ";
        for (int data : sharedData) {
            std::cout << data << " ";
        }
        std::cout << "\n";
        LeaveCriticalSection(&cs);
        ReleaseSemaphore(hSemaphoreReaders, 1, NULL);
        Sleep(1000); 
    }
}


int main() {
    InitializeCriticalSection(&cs);
    hMutex = CreateMutex(NULL, FALSE, NULL);
    hSemaphoreReaders = CreateSemaphore(NULL, maxReaders, maxReaders, NULL);
    hSemaphoreWriters = CreateSemaphore(NULL, maxWriters, maxWriters, NULL);

    std::vector<std::thread> writers;
    for (int i = 0; i < numWriters; ++i) {
        writers.push_back(std::thread(WriterThread, i));
    }

    std::vector<std::thread> readers;
    for (int i = 0; i < numReaders; ++i) {
        readers.push_back(std::thread(ReaderThread, i));
    }

    for (auto& writer : writers) {
        writer.join();
    }
    for (auto& reader : readers) {
        reader.join();
    }

    DeleteCriticalSection(&cs);
    CloseHandle(hMutex);
    CloseHandle(hSemaphoreReaders);
    CloseHandle(hSemaphoreWriters);

    return 0;
}