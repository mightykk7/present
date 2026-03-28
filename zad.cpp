// sp14_voroshilov.cpp : Компьютерный клуб с семафорами

#include <iostream>
#include <Windows.h>
#include <ctime>

#define MAX_CLIENTS 20
#define CLUB_CAPACITY 4
#define WAIT_TIMEOUT_MS 3000
#define OBSERVER_SLEEP_MS 200
#define MIN_WORK_TIME 2000
#define MAX_WORK_TIME 5000

struct ClientRecord {
    DWORD threadId;
    DWORD arriveTick;
    DWORD startTick;
    DWORD endTick;
    BOOL served;
    BOOL timeout;
};

struct ClubState {
    ClientRecord* clients[MAX_CLIENTS];
    LONG currentVisitors;
    LONG maxVisitors;
    LONG servedCount;
    LONG timeoutCount;
    CRITICAL_SECTION criticalSection;
};

ClubState g_club;

DWORD WINAPI ClientThread(LPVOID _client) {
    srand(GetTickCount());
    
    ClientRecord* client = (ClientRecord*)_client;
    
    client->arriveTick = GetTickCount();
    
    EnterCriticalSection(&g_club.criticalSection);
    g_club.clients[client->threadId - 1] = client;
    LeaveCriticalSection(&g_club.criticalSection);
    
    HANDLE hSemaphore = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, L"ClubSemaphore");
    if (hSemaphore == NULL) {
        std::cerr << "Ошибка открытия семафора в потоке " << client->threadId << std::endl;
        return GetLastError();
    }
    
    DWORD waitResult = WaitForSingleObject(hSemaphore, WAIT_TIMEOUT_MS);
    
    if (waitResult == WAIT_OBJECT_0) {
        client->startTick = GetTickCount();
        
        int workTime = (rand() % (MAX_WORK_TIME - MIN_WORK_TIME + 1)) + MIN_WORK_TIME;
        
        Sleep(workTime);
        
        ReleaseSemaphore(hSemaphore, 1, NULL);
        
        client->endTick = GetTickCount();
        
        EnterCriticalSection(&g_club.criticalSection);
        client->served = TRUE;
        client->timeout = FALSE;
        LeaveCriticalSection(&g_club.criticalSection);
    }
    else {
        EnterCriticalSection(&g_club.criticalSection);
        client->served = FALSE;
        client->timeout = TRUE;
        LeaveCriticalSection(&g_club.criticalSection);
    }
    
    CloseHandle(hSemaphore);
    return 0;
}

DWORD WINAPI ObserverThread(LPVOID lpParam) {
    BOOL allCompleted = FALSE;
    
    while (!allCompleted) {
        Sleep(OBSERVER_SLEEP_MS);
        
        EnterCriticalSection(&g_club.criticalSection);
        
        g_club.servedCount = 0;
        g_club.timeoutCount = 0;
        g_club.currentVisitors = 0;
        
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (g_club.clients[i] != NULL) {
                ClientRecord* client = g_club.clients[i];
                
                if (client->served) {
                    g_club.servedCount++;
                }
                else if (client->timeout) {
                    g_club.timeoutCount++;
                }
                else if (client->startTick > 0 && client->endTick == 0) {
                    g_club.currentVisitors++;
                }
            }
        }
        
        if (g_club.maxVisitors < g_club.currentVisitors) {
            g_club.maxVisitors = g_club.currentVisitors;
        }
        
        if (g_club.servedCount + g_club.timeoutCount == MAX_CLIENTS) {
            allCompleted = TRUE;
        }
        
        LeaveCriticalSection(&g_club.criticalSection);
        
        system("cls");
        std::cout << "========== КОМПЬЮТЕРНЫЙ КЛУБ ==========" << std::endl;
        std::cout << "Всего компьютеров: " << CLUB_CAPACITY << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        std::cout << "Занято мест: " << g_club.currentVisitors << std::endl;
        std::cout << "Обслужено посетителей: " << g_club.servedCount << std::endl;
        std::cout << "Ушло по таймауту: " << g_club.timeoutCount << std::endl;
        std::cout << "Осталось: " << (MAX_CLIENTS - g_club.servedCount - g_club.timeoutCount) << std::endl;
        std::cout << "========================================" << std::endl;
    }
    
    std::cout << "\n========== ИТОГОВАЯ СТАТИСТИКА ==========" << std::endl;
    std::cout << "Максимальное количество занятых мест: " << g_club.maxVisitors << std::endl;
    std::cout << "Всего обслужено: " << g_club.servedCount << std::endl;
    std::cout << "Всего ушло по таймауту: " << g_club.timeoutCount << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    double avgWaitingTime = 0;
    double avgServiceTime = 0;
    int servedClients = 0;
    
    std::cout << "\nДетальная информация о посетителях:" << std::endl;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_club.clients[i] != NULL) {
            ClientRecord* client = g_club.clients[i];
            
            if (client->served) {
                double waitingTime = (client->startTick - client->arriveTick) / 1000.0;
                double serviceTime = (client->endTick - client->startTick) / 1000.0;
                
                avgWaitingTime += waitingTime;
                avgServiceTime += serviceTime;
                servedClients++;
                
                std::cout << "Клиент #" << client->threadId 
                          << " (обслужен): ожидание=" << waitingTime 
                          << "с, работа=" << serviceTime << "с" << std::endl;
            }
            else if (client->timeout) {
                avgWaitingTime += WAIT_TIMEOUT_MS / 1000.0;
                std::cout << "Клиент #" << client->threadId 
                          << " (таймаут): ожидал " << WAIT_TIMEOUT_MS / 1000.0 
                          << "с и ушел" << std::endl;
            }
        }
    }
    
    if (servedClients > 0) {
        std::cout << "\n----------------------------------------" << std::endl;
        std::cout << "Среднее время ожидания: " << (avgWaitingTime / MAX_CLIENTS) << " секунд" << std::endl;
        std::cout << "Среднее время обслуживания: " << (avgServiceTime / servedClients) << " секунд" << std::endl;
    }
    else {
        std::cout << "\nНи один клиент не был обслужен!" << std::endl;
    }
    
    std::cout << "========================================" << std::endl;
    
    return 0;
}

int main() {
    srand(static_cast<unsigned int>(time(NULL)));
    
    InitializeCriticalSection(&g_club.criticalSection);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        g_club.clients[i] = NULL;
    }
    
    HANDLE hSemaphore = CreateSemaphore(NULL, CLUB_CAPACITY, CLUB_CAPACITY, L"ClubSemaphore");
    if (hSemaphore == NULL) {
        std::cerr << "Ошибка создания семафора: " << GetLastError() << std::endl;
        DeleteCriticalSection(&g_club.criticalSection);
        return GetLastError();
    }
    
    HANDLE hThreads[MAX_CLIENTS];
    DWORD IDThreads[MAX_CLIENTS];
    
    ClientRecord clients[MAX_CLIENTS];
    
    std::cout << "Компьютерный клуб открывается!" << std::endl;
    std::cout << "Всего компьютеров: " << CLUB_CAPACITY << std::endl;
    std::cout << "Всего посетителей: " << MAX_CLIENTS << std::endl;
    std::cout << "Время ожидания: " << WAIT_TIMEOUT_MS / 1000.0 << " сек" << std::endl;
    std::cout << "Время работы: " << MIN_WORK_TIME / 1000.0 << "-" 
              << MAX_WORK_TIME / 1000.0 << " сек" << std::endl;
    std::cout << "\nНажмите любую клавишу для запуска симуляции..." << std::endl;
    system("pause");
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].threadId = i + 1;
        clients[i].arriveTick = 0;
        clients[i].startTick = 0;
        clients[i].endTick = 0;
        clients[i].served = FALSE;
        clients[i].timeout = FALSE;
        
        hThreads[i] = CreateThread(NULL, 0, ClientThread, &clients[i], 0, &IDThreads[i]);
        if (hThreads[i] == NULL) {
            std::cerr << "Ошибка создания потока " << i+1 << ": " << GetLastError() << std::endl;
            for (int j = 0; j < i; j++) {
                CloseHandle(hThreads[j]);
            }
            CloseHandle(hSemaphore);
            DeleteCriticalSection(&g_club.criticalSection);
            return GetLastError();
        }
        
        if (i >= 0 && i <= 7) {
            SetThreadPriority(hThreads[i], THREAD_PRIORITY_NORMAL);
        }
        else if (i >= 8 && i <= 15) {
            SetThreadPriority(hThreads[i], THREAD_PRIORITY_BELOW_NORMAL);
        }
        else if (i >= 16 && i <= 19) {
            SetThreadPriority(hThreads[i], THREAD_PRIORITY_HIGHEST);
        }
    }
    
    HANDLE hObserverThread;
    DWORD IDObserverThread;
    
    hObserverThread = CreateThread(NULL, 0, ObserverThread, NULL, 0, &IDObserverThread);
    if (hObserverThread == NULL) {
        std::cerr << "Ошибка создания потока-наблюдателя: " << GetLastError() << std::endl;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            CloseHandle(hThreads[i]);
        }
        CloseHandle(hSemaphore);
        DeleteCriticalSection(&g_club.criticalSection);
        return GetLastError();
    }
    
    SetThreadPriority(hObserverThread, THREAD_PRIORITY_LOWEST);
    
    DWORD waitResult = WaitForMultipleObjects(MAX_CLIENTS, hThreads, TRUE, INFINITE);
    if (waitResult == WAIT_FAILED) {
        std::cerr << "Ошибка ожидания потоков: " << GetLastError() << std::endl;
    }
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        CloseHandle(hThreads[i]);
    }
    
    WaitForSingleObject(hObserverThread, INFINITE);
    CloseHandle(hObserverThread);
    
    CloseHandle(hSemaphore);
    
    DeleteCriticalSection(&g_club.criticalSection);
    
    std::cout << "\nСимуляция завершена. Нажмите любую клавишу для выхода..." << std::endl;
    system("pause");
    
    return 0;
}
