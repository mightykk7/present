// zad14-sis.cpp : Этот файл содержит функцию "main". Здесь начинается и заканчивается выполнение программы.
//

#include <iostream>
#include <Windows.h>

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
bool Obsfinish = false;

DWORD WINAPI ClientThread(LPVOID lpParam) {
    srand(GetTickCount());

    ClientRecord* client = (ClientRecord*)lpParam;

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

        int workTime = (rand() % 3001 + 2000);
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

DWORD WINAPI ObserverThread(LPVOID lpParam) {
    while (!Obsfinish) {
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
        std::cout << "Работа без семафора" << std::endl;
    }

    InitializeCriticalSection(&cs);

    club.currentVisitors = 0;
    club.maxVisitors = 0;
    club.servedCount = 0;
    club.timeoutCount = 0;

    DWORD IDThreads[MAX_CLIENTS];
    ClientRecord clientsArray[MAX_CLIENTS];

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
    hThreads[MAX_CLIENTS] = CreateThread(NULL, 0, ObserverThread, NULL, 0, NULL);

    SetThreadPriority(hThreads[MAX_CLIENTS+1], THREAD_PRIORITY_LOWEST);

    WaitForMultipleObjects(MAX_CLIENTS, hThreads, TRUE, INFINITE);

    Obsfinish = TRUE;
    
    WaitForSingleObject(hThreads[MAX_CLIENTS], INFINITE);

    std::cout << "\n\n=== ИТОГИ ===" << std::endl;
    std::cout << "Максимум занятых мест: " << club.maxVisitors << std::endl;
    std::cout << "Обслужено: " << club.servedCount << std::endl;
    std::cout << "Таймаутов: " << club.timeoutCount << std::endl;

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

    for (int i = 0; i < MAX_CLIENTS; i++) {
        CloseHandle(hThreads[i]);
    }

    if (Semaphore) {
        CloseHandle(hSemaphore);
    }

    DeleteCriticalSection(&cs);

    return 0;
}
Практическая работа № 14. Семафоры
Задание
Реализовать консольное приложение, которое моделирует работу компьютерного клуба с
ограниченным числом рабочих мест. Информация о посетителях и состоянии клуба
хранится в оперативной памяти.
Описание архитектуры разрабатываемого приложения
Для синхронизации доступа к ограниченному ресурсу необходимо использовать объект
типа Semaphore .
Ограниченным ресурсом являются рабочие места компьютерного клуба.
В клубе имеется фиксированное количество компьютеров:
В качестве общих данных используется экземпляр структуры ClubState , следующего
вида:
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
LONG servedCount; // Количество обслуженных посетителей
С объектом структуры взаимодействуют два типа функций:
Функционал потока посетителя:
Функционал потока наблюдателя:
Реализация
Реализовать следующие потоки:
Поток Роль Приоритет
T1–T8 Обычные посетители THREAD_PRIORITY_NORMAL
T9–T16 Низкоприоритетные посетители THREAD_PRIORITY_BELOW_NORMAL
T17–T20 Высокоприоритетные посетители THREAD_PRIORITY_HIGHEST
T21 Наблюдатель THREAD_PRIORITY_LOWEST
LONG timeoutCount; // Количество ушедших по таймауту
};
Поток посетитель
Поток наблюдатель
Фиксирует момент прихода
Пытается получить доступ к рабочему месту через семафор
Если свободное место есть — занимает его
Если мест нет — ожидает не более заданного времени
После получения доступа:
фиксирует начало обслуживания
работает за компьютером случайное время (2000–5000 мс.)
освобождает место
фиксирует завершение обслуживания
Если время ожидания истекло, поток фиксирует отказ от обслуживания
После завершения поток корректно завершает работу
Каждые 500 мс. считывает текущее состояние клуба
Выводит в консоль:
количество занятых мест
количество обслуженных посетителей
количество посетителей, ушедших по таймауту
Завершается после завершения всех потоков посетителей
Для ограничения количества одновременно работающих потоков использовать семафор
с максимальным количеством ресурсов, равным числу компьютеров клуба.
Все потоки должны корректно завершаться.
Требования к реализации
При разработке программы необходимо использовать следующие функции WinAPI:
Условия работы программы
Анализ работы программы
Создать два варианта программы:
Сравнить работу двух вариантов программ и ответить на вопросы:
CreateSemaphore
WaitForSingleObject
ReleaseSemaphore
CreateThread
SetThreadPriority
WaitForMultipleObjects
CloseHandle
1. Количество компьютеров клуба: CLUB_CAPACITY = 4
2. Количество посетителей: MAX_CLIENTS = 20
3. Каждый посетитель ожидает свободное место не более 3000 мс.
4. Время работы за компьютером задается псевдослучайно в диапазоне 2000–5000 мс.
5. Наблюдатель завершает работу после завершения всех потоков посетителей.
С семафором
Без семафора
1. Почему в варианте без семафора число одновременно работающих потоков может
превышать допустимое количество?
2. Как семафор ограничивает доступ к ресурсу?
3. Как влияют приоритеты потоков на получение доступа к рабочим местам?
4. Что произойдет, если начальное значение семафора равно 1?
5. Что произойдет, если время ожидания сделать меньше времени обслуживания?
6. Чем семафор отличается от критической секции?
Дополнительное задание
1. Вычислить среднее время ожидания посетителей
2. Вычислить среднее время обслуживания
3. Определить максимальное число одновременно занятых мест
4. Вывести список потоков, которые не дождались свободного места
