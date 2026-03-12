#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <string>

using namespace std;

// Вирівнювання по 4 байтах - стандарт для 32-бітних систем
#define ALIGN_UP(size) (((size) + 3) & ~3)

// Структура управління блоком
struct BlockMeta {
    uint32_t check_sum;   // Магічне число (захист від затирання)
    size_t blockSize;     // Скільки байт виділено
    bool isUsed;          // Статус: зайнято/вільно
    BlockMeta* pNext;     // Наступний блок у списку
};

class CustomAllocator {
private:
    uint8_t* _buffer;     // Весь масив пам'яті
    size_t _total;        // Загальний розмір
    BlockMeta* _root;     // Перший елемент списку
    const uint32_t MAGIC_ID = 0xDEADC0DE; 

public:
    // Конструктор: виділяємо великий шматок один раз
    CustomAllocator(size_t n) : _total(n) {
        _buffer = (uint8_t*)malloc(n);
        if (!_buffer) {
            fprintf(stderr, "Fatal: cannot allocate pool\n");
            exit(-1);
        }

        _root = (BlockMeta*)_buffer;
        _root->check_sum = MAGIC_ID;
        _root->blockSize = n - sizeof(BlockMeta);
        _root->isUsed = false;
        _root->pNext = nullptr;
    }

    ~CustomAllocator() {
        if (_buffer) free(_buffer);
    }

    // Власна реалізація malloc (Алгоритм First Fit)
    void* my_malloc(size_t req) {
        size_t needed = ALIGN_UP(req);
        BlockMeta* curr = _root;

        while (curr != nullptr) {
            if (curr->check_sum != MAGIC_ID) {
                printf("!!! ERROR: Metadata corrupted !!!\n");
                return nullptr;
            }

            if (!curr->isUsed && curr->blockSize >= needed) {
                // Якщо блок великий - розбиваємо його (Splitting)
                if (curr->blockSize > (needed + sizeof(BlockMeta) + 8)) {
                    BlockMeta* next_free = (BlockMeta*)((uint8_t*)curr + sizeof(BlockMeta) + needed);
                    
                    next_free->check_sum = MAGIC_ID;
                    next_free->blockSize = curr->blockSize - needed - sizeof(BlockMeta);
                    next_free->isUsed = false;
                    next_free->pNext = curr->pNext;

                    curr->blockSize = needed;
                    curr->pNext = next_free;
                }
                
                curr->isUsed = true;
                return (void*)((uint8_t*)curr + sizeof(BlockMeta));
            }
            curr = curr->pNext;
        }
        return nullptr;
    }

    // Власна реалізація free
    void my_free(void* ptr) {
        if (!ptr) return;

        BlockMeta* m = (BlockMeta*)((uint8_t*)ptr - sizeof(BlockMeta));
        if (m->check_sum != MAGIC_ID) return;

        m->isUsed = false;

        // Дефрагментація: склеюємо сусідні вільні блоки (Coalescing)
        BlockMeta* iter = _root;
        while (iter != nullptr && iter->pNext != nullptr) {
            if (!iter->isUsed && !iter->pNext->isUsed) {
                iter->blockSize += iter->pNext->blockSize + sizeof(BlockMeta);
                iter->pNext = iter->pNext->pNext;
            } else {
                iter = iter->pNext;
            }
        }
    }

    // Вивід поточної карти пам'яті
    void dump() {
        printf("\n--- СТАН ПАМ'ЯТІ ---\n");
        BlockMeta* temp = _root;
        int count = 0;
        while (temp) {
            printf("Блок %d: [%s] Розмір: %-4zu \t Адреса: %p\n", 
                   count++, temp->isUsed ? "USED" : "FREE", temp->blockSize, (void*)temp);
            temp = temp->pNext;
        }
        printf("--------------------\n");
    }
};

int main() {
    CustomAllocator mem(1024);
    printf("Створено менеджер пам'яті на 1024 байти.\n");

    // Тест 1: Виділення
    void* p1 = mem.my_malloc(150);
    void* p2 = mem.my_malloc(300);
    mem.dump();

    // Тест 2: Звільнення та фрагментація
    printf("\nЗвільняємо p1 (150 байт)...");
    mem.my_free(p1);
    mem.dump();

    // Тест 3: Повторне виділення (Splitting)
    printf("\nВиділяємо p3 (50 байт) на місце p1...");
    void* p3 = mem.my_malloc(50);
    mem.dump();

    // Тест 4: Повне очищення та злиття (Coalescing)
    printf("\nОчищуємо все інше (p2 та p3)...");
    mem.my_free(p2);
    mem.my_free(p3);
    mem.dump();

    printf("\nТест завершено. Пам'ять знову ціла.\n");

    return 0;
}