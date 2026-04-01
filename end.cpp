// zad14-sis.cpp : Этот файл содержит функцию "main". Здесь начинается и заканчивается выполнение программы.
//

#include <iostream>
#include <Windows.h>
#include <ctime>

#define MAX_CLIENTS 20
#define CLUB_CAPACITY 4

/**
* Структура записи о посетителе
*/

struct ClientRecord {
    DWORD threadId; // Идентификатор потока
    DWORD arriveTick; // Время прихода посетителя
    DWORD startTick; // Время начала обслуживания
    DWORD endTick; // Время завершения обслуживания
    BOOL served; // Был ли обслужен
    BOOL timeout; // Ушел ли по таймауту
};
/**
* Состояние компьютерного клуба
*/
struct ClubState {
    ClientRecord clients[MAX_CLIENTS]; // Информация о посетителях
    LONG currentVisitors; // Текущее число занятых мест
    LONG maxVisitors; // Максимум одновременно занятых мест
    LONG servedCount;
    LONG timeoutCount; // Количество ушедших по таймауту
};
ClubState club;
CRITICAL_SECTION cs;
bool Semaphore = false;
HANDLE hSemaphore;
ClientRecord clients[MAX_CLIENTS];
HANDLE hThreads[MAX_CLIENTS];

DWORD WINAPI ClientThread(LPVOID _client) {
    srand(GetTickCount());

    ClientRecord* client = (ClientRecord*)_client;

    client->arriveTick = GetTickCount();

    EnterCriticalSection(&cs);
    club.clients[client->threadId - 1] = *client;
    LeaveCriticalSection(&cs);

    DWORD waitResult;
    
    if (Semaphore) {
        waitResult = WaitForSingleObject(hSemaphore, 3000);
    }
    else {
        waitResult = WAIT_OBJECT_0;
    }

    if (waitResult == WAIT_OBJECT_0) {
        client->startTick = GetTickCount();
        
        EnterCriticalSection(&cs);
        club.currentVisitors++;
        if (club.maxVisitors < club.currentVisitors) {
            club.maxVisitors = club.currentVisitors;
        }
        LeaveCriticalSection(&cs);

        int workTime = (rand() % (5000 - 2000 + 1)) + 2000;
        Sleep(workTime);

        if (Semaphore) {
            ReleaseSemaphore(hSemaphore, 1, NULL);
        }

        client->endTick = GetTickCount();

        EnterCriticalSection(&cs);
        client->served = TRUE;
        client->timeout = FALSE;
        club.servedCount++;
        club.currentVisitors--;
        club.clients[client->threadId - 1] = *client;
        LeaveCriticalSection(&cs);
    }
    else {
        EnterCriticalSection(&cs);
        client->served = FALSE;
        client->timeout = TRUE;
        club.timeoutCount++;
        club.clients[client->threadId - 1] = *client;
        LeaveCriticalSection(&cs);
    }
    
    return 0;
}

DWORD WINAPI ObserverThread(LPVOID _client) {
    HANDLE* ClientshTreads = (HANDLE*)_client;
    
    while (WaitForMultipleObjects(MAX_CLIENTS, ClientshTreads, TRUE, 500) == WAIT_TIMEOUT) {
        EnterCriticalSection(&cs);
        std::cout << "\rЗанято: " << club.currentVisitors
            << " | Обслужено: " << club.servedCount
            << " | Таймауты: " << club.timeoutCount << "   " << std::flush;
        LeaveCriticalSection(&cs);
        Sleep(500);
    }
    return 0;
}

int main()
{
    setlocale(0, "rus");
    std::cout << "Использовать семафоры?\n1 использовать / 2 не использовать\n";
    int otvet; 
    std::cin >> otvet;
    
    if (otvet == 1) {
        hSemaphore = CreateSemaphore(NULL, CLUB_CAPACITY, CLUB_CAPACITY, L"ClubSemaphore");
        if (hSemaphore == NULL) {
            return GetLastError();
        }
        Semaphore = true;
        std::cout << "Семафор создан" << std::endl;
    }
    else {
        Semaphore = false;
        std::cout << "Работа без семафора" << std::endl;
    }
    
    InitializeCriticalSection(&cs);
    
    // Инициализация
    club.currentVisitors = 0;
    club.maxVisitors = 0;
    club.servedCount = 0;
    club.timeoutCount = 0;
    
    DWORD IDThreads[MAX_CLIENTS];
    ClientRecord clientsArray[MAX_CLIENTS];
    
    // Создаем потоки посетителей
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clientsArray[i].threadId = i + 1;
        clientsArray[i].arriveTick = 0;
        clientsArray[i].startTick = 0;
        clientsArray[i].endTick = 0;
        clientsArray[i].served = FALSE;
        clientsArray[i].timeout = FALSE;
        
        hThreads[i] = CreateThread(NULL, 0, ClientThread, &clientsArray[i], NULL, &IDThreads[i]);
        if (hThreads[i] == NULL)
            return GetLastError();
            
        if (i >= 0 && i <= 7) SetThreadPriority(hThreads[i], THREAD_PRIORITY_NORMAL);
        else if (i >= 8 && i <= 15) SetThreadPriority(hThreads[i], THREAD_PRIORITY_BELOW_NORMAL);
        else if (i >= 16 && i <= 19) SetThreadPriority(hThreads[i], THREAD_PRIORITY_HIGHEST);
    }
    
    // Создаем поток наблюдателя
    HANDLE hObsThread = CreateThread(NULL, 0, ObserverThread, hThreads, NULL, NULL);
    if (hObsThread == NULL)
        return GetLastError();
    
    SetThreadPriority(hObsThread, THREAD_PRIORITY_LOWEST);
    
    // Ждем завершения всех потоков
    WaitForMultipleObjects(MAX_CLIENTS, hThreads, TRUE, INFINITE);
    WaitForSingleObject(hObsThread, INFINITE);
    
    // Выводим итоги
    std::cout << "\n\n=== ИТОГИ ===" << std::endl;
    std::cout << "Максимум занятых мест: " << club.maxVisitors << std::endl;
    std::cout << "Обслужено: " << club.servedCount << std::endl;
    std::cout << "Таймаутов: " << club.timeoutCount << std::endl;
    
    // Считаем среднее время
    double avgWait = 0;
    double avgService = 0;
    int served = 0;
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (club.clients[i].served) {
            avgWait += (club.clients[i].startTick - club.clients[i].arriveTick) / 1000.0;
            avgService += (club.clients[i].endTick - club.clients[i].startTick) / 1000.0;
            served++;
        }
        else if (club.clients[i].timeout) {
            avgWait += 3.0;
        }
    }
    
    if (served > 0) {
        std::cout << "Среднее время ожидания: " << avgWait / MAX_CLIENTS << " сек" << std::endl;
        std::cout << "Среднее время обслуживания: " << avgService / served << " сек" << std::endl;
    }
    
    // Закрываем дескрипторы
    for (int i = 0; i < MAX_CLIENTS; i++) {
        CloseHandle(hThreads[i]);
    }
    CloseHandle(hObsThread);
    
    if (Semaphore) {
        CloseHandle(hSemaphore);
    }
    
    DeleteCriticalSection(&cs);
    
    system("pause");
    return 0;
}
