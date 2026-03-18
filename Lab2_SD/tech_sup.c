#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <windows.h>
#define MAX_QUEUE_SIZE 5
#define MIN_QUEUE_SIZE 2

// Структуры, списки, очереди и стэки

typedef enum {
    pending,
    ready,
    processed,
    canceled
} RequestStatus;

typedef struct Dependency {
    int depends_on_id;
    struct Dependency* next;
} Dependency;

typedef struct Request {
    int id;
    char username[64];
    int priority;
    time_t timestamp;
    RequestStatus status;
    Dependency* dep_list;
} Request;

typedef struct Node {
    Request* req;
    struct Node* next;
} Node;

typedef struct Queue {
    Node* head;
    Node* tail;
    int count;
} Queue;

typedef struct Department {
    Queue** sub_queues;
    int queue_count;
} Department;

typedef struct Stack {
    Node* top;
} Stack;


// создание и добавление узла в очередь

Node* create_node(Request* r) {
    Node* new_node = (Node*)malloc(sizeof(Node));
    if (!new_node) return NULL;

    new_node->req = r;
    new_node->next = NULL;

    return new_node;
}

void enqueue_smart(Queue* q, Request* r) {
    Node* new_node = create_node(r);

    if (q->head == NULL || r->priority > q->head->req->priority || (r->priority == q->head->req->priority && r->timestamp < q->head->req->timestamp)) {
        new_node->next = q->head;
        q->head = new_node;
        if (q->tail == NULL) {
            q->tail = new_node;
        }
    }
    else {
        Node* current = q->head;
        while (current->next != NULL) {
            Request* next_r = current->next->req;
            if (r->priority > next_r->priority) break;
            if (r->priority == next_r->priority && r->timestamp < next_r->timestamp) break;
            current = current->next;
        }
        new_node->next = current->next;
        current->next = new_node;
        if (new_node->next == NULL) q->tail = new_node;
    }

    q->count++;
}

// здесь мы находим запрос во всех департаментах и стэках, везде короче ищет
Request* find_request_globally(int id, Department* departments, int dep_count, Stack* canceled_stack) {
    for (int i = 0; i < dep_count; i++) {
        for (int j = 0; j < departments[i].queue_count; j++) {
            Node* current = departments[i].sub_queues[j]->head;
            while (current != NULL) {
                if (current->req->id == id) {
                    return current->req;
                }
                current = current->next;
            }
        }
    }

    Node* current_st = canceled_stack->top;
    while (current_st != NULL) {
        if (current_st->req->id == id) {
            return current_st->req;
        }
        current_st = current_st->next;
    }

    return NULL;
}

// смотри, готов ли запрос на завершение. смотрим, от чего запрос еще зависит
int is_ready_for_processing(Request* req, Department* deps, int dep_count, Stack* canceled_stack) {
    Dependency* current_dep = req->dep_list;

    while (current_dep != NULL) {
        Request* parent = find_request_globally(current_dep->depends_on_id, deps, dep_count, canceled_stack);

        if (parent != NULL && parent->status != processed) {
            return 0;
        }
        current_dep = current_dep->next;
    }

    return 1;
}


// здесь происходит балансировка, то есть если большая очередь, то делем ее и наоборот 

void split_queue(Department* dev, int q_idx);
void merge_queues(Department* dev, int q_idx);

void balance_department(Department* dev) {
    for (int i = 0; i < dev->queue_count; i++) {
        if (dev->sub_queues[i]->count > MAX_QUEUE_SIZE) {
            split_queue(dev, i);
        }
        else if (dev->sub_queues[i]->count < MIN_QUEUE_SIZE && dev->queue_count > 1) {
            merge_queues(dev, i);
        }
    }
}

void split_queue(Department* dev, int q_idx) {
    Queue* old_q = dev->sub_queues[q_idx];
    int mid = old_q->count / 2;

    Queue* new_q = (Queue*)malloc(sizeof(Queue));
    if (!new_q) return;

    Node* current = old_q->head;
    for (int i = 0; i < mid - 1; i++) current = current->next;

    new_q->head = current->next;
    new_q->tail = old_q->tail;
    new_q->count = old_q->count - mid;
    current->next = NULL;

    old_q->tail = current;
    old_q->count = mid;

    dev->queue_count++;
    dev->sub_queues = (Queue**)realloc(dev->sub_queues, sizeof(Queue*) * dev->queue_count);
    dev->sub_queues[dev->queue_count - 1] = new_q;
    printf("[Система] Очередь %d переполнена. Создана подочередь.\n", q_idx + 1);
}

void merge_queues(Department* dev, int q_idx) {
    if (q_idx == 0 || dev->queue_count < 2) return;

    Queue* target = dev->sub_queues[q_idx - 1];
    Queue* source = dev->sub_queues[q_idx];

    if (target->tail != NULL) {
        target->tail->next = source->head;
        if (source->tail != NULL) {
            target->tail = source->tail;
        }
    }
    else {
        target->head = source->head;
        target->tail = source->tail;
    }

    target->count += source->count;

    free(source);

    for (int i = q_idx; i < dev->queue_count - 1; i++) {
        dev->sub_queues[i] = dev->sub_queues[i + 1];
    }

    dev->queue_count--;
    dev->sub_queues = (Queue**)realloc(dev->sub_queues, sizeof(Queue*) * dev->queue_count);

    printf("Очереди объединены для оптимизации нагрузки.\n");
}


// инициализируем департаменты 
Department* init_departments() {
    Department* deps = (Department*)malloc(3 * sizeof(Department));
    if (!deps) return NULL;

    for (int i = 0; i < 3; i++) {
        deps[i].queue_count = 1;
        deps[i].sub_queues = (Queue**)malloc(sizeof(Queue*));
        if (!deps[i].sub_queues) return NULL;

        deps[i].sub_queues[0] = (Queue*)malloc(sizeof(Queue));
        deps[i].sub_queues[0]->head = deps[i].sub_queues[0]->tail = NULL;
        deps[i].sub_queues[0]->count = 0;
    }

    return deps;
}

// вставка и удаление из стека
void push_to_stack(Stack* s, Request* r) {
    Node* new_node = create_node(r);
    if (!new_node) return;

    r->status = canceled;
    new_node->next = s->top;
    s->top = new_node;

    printf("Заявка %d отменена и помещена в стек.\n", r->id);
}

Request* pop_from_stack(Stack* s) {
    if (s->top == NULL) return NULL;

    Node* temp = s->top;
    Request* r = temp->req;

    s->top = s->top->next;
    free(temp);

    r->status = pending;
    return r;
}

// удаление узла из очереди
void remove_node(Queue* q, Node* prev, Node* target) {
    if (prev == NULL) {
        q->head = target->next;
    }
    else {
        prev->next = target->next;
    }

    if (target == q->tail) {
        q->tail = prev;
    }
    q->count--;
    free(target);
}

void process_next_in_department(Department* dev, int dep_id, Department* all_deps, int dep_count, Stack* s) {
    for (int i = 0; i < dev->queue_count; i++) {
        Queue* q = dev->sub_queues[i];
        Node* curr = q->head;
        Node* prev = NULL;

        while (curr != NULL) {
            if (is_ready_for_processing(curr->req, all_deps, dep_count, s)) {
                printf("Заявка %d (%s) обработана и удалена.\n", curr->req->id, curr->req->username);
                curr->req->status = processed;

                remove_node(q, prev, curr);
                balance_department(dev);
                return;
            }
            prev = curr;
            curr = curr->next;
        }
    }

    printf("Нет заявок, готовых к обработке в данном подразделении.\n");
}


// перемещаем заявку из одной очереди в другую
void transfer_request(int id, Department* all_deps, int from_dep, int to_dep, Stack* s) {
    Request* target_req = NULL;
    Department* src = &all_deps[from_dep];

    for (int i = 0; i < src->queue_count; i++) {
        Queue* q = src->sub_queues[i];
        Node* curr = q->head;
        Node* prev = NULL;
        while (curr != NULL) {
            if (curr->req->id == id) {
                target_req = curr->req;
                remove_node(q, prev, curr);
                balance_department(src);
                break;
            }
            prev = curr;
            curr = curr->next;
        }
        if (target_req) break;
    }

    if (target_req) {
        enqueue_smart(all_deps[to_dep].sub_queues[0], target_req);
        balance_department(&all_deps[to_dep]);
        printf("Заявка %d переведена в отдел %d.\n", id, to_dep + 1);
    }
    else {
        printf("Заявка с таким ID не найдена в исходном отделе.\n");
    }
}

// просмотр первой заявки
void peek_first_request(Department* deps) {
    int d_num;
    printf("Введите номер отдела для просмотра (1-3): ");
    scanf("%d", &d_num);
    if (d_num >= 1 && d_num <= 3) {
        Department* dev = &deps[d_num - 1];
        // Смотрим только первую подочередь, так как приоритетная заявка всегда в её начале
        if (dev->queue_count > 0 && dev->sub_queues[0]->head != NULL) {
            Request* r = dev->sub_queues[0]->head->req;
            printf(">>> Первая заявка: ID %d | Имя: %s | Приоритет: %d | Статус: %d\n",
                r->id, r->username, r->priority, r->status);
        }
        else {
            printf("Очереди отдела %d пусты.\n", d_num);
        }
    }
    else {
        printf("Неверный номер отдела.\n");
    }
}

// вывод текущего состояния всех структур
void print_structures_state(Department* deps, int dep_count, Stack* s) {
    printf("\n=== ТЕКУЩЕЕ СОСТОЯНИЕ СТРУКТУР ПАМЯТИ ===\n");

    for (int i = 0; i < dep_count; i++) {
        printf("Отдел %d (Всего подочередей: %d):\n", i + 1, deps[i].queue_count);
        for (int j = 0; j < deps[i].queue_count; j++) {
            printf("  [Очередь %d]: ", j + 1);
            Node* curr = deps[i].sub_queues[j]->head;
            if (!curr) printf("Пусто");
            while (curr) {
                printf("{ID:%d, Prio:%d} -> ", curr->req->id, curr->req->priority);
                curr = curr->next;
            }
            printf("NULL\n");
        }
    }

    printf("\nСтек отмененных заявок:\n  [Вершина] -> ");
    Node* st = s->top;
    if (!st) printf("Пусто");
    while (st) {
        printf("{ID:%d} -> ", st->req->id);
        st = st->next;
    }
    printf("NULL\n=========================================\n");
}

// поиск по имени пользователя
void find_by_username(Department* deps, int dep_count, Stack* s, const char* name) {
    int found = 0;
    for (int i = 0; i < dep_count; i++) {
        for (int j = 0; j < deps[i].queue_count; j++) {
            Node* curr = deps[i].sub_queues[j]->head;
            while (curr) {
                if (strcmp(curr->req->username, name) == 0) {
                    printf("[Отдел %d] ID: %d, Приоритет: %d, Статус: %d\n", i + 1, curr->req->id, curr->req->priority, curr->req->status);
                    found = 1;
                }
                curr = curr->next;
            }
        }
    }

    Node* st_curr = s->top;
    while (st_curr) {
        if (strcmp(st_curr->req->username, name) == 0) {
            printf("[В СТЕКЕ] ID: %d, Приоритет: %d, Статус: %d\n", st_curr->req->id, st_curr->req->priority, st_curr->req->status);
            found = 1;
        }
        st_curr = st_curr->next;
    }
    if (!found) printf("Заявки пользователя %s не найдены.\n", name);
}

// Компаратор для сортировки (qsort)
int compare_requests(const void* a, const void* b) {
    Request* r1 = *(Request**)a;
    Request* r2 = *(Request**)b;
    if (r1->priority != r2->priority)
        return r2->priority - r1->priority;
    return (int)(r1->timestamp - r2->timestamp);
}

// Вывод всех заявок
void print_all_sorted(Department* deps, int dep_count) {
    Request* all[1000];
    int total = 0;

    for (int i = 0; i < dep_count; i++) {
        for (int j = 0; j < deps[i].queue_count; j++) {
            Node* curr = deps[i].sub_queues[j]->head;
            while (curr && total < 1000) {
                all[total++] = curr->req;
                curr = curr->next;
            }
        }
    }

    if (total == 0) {
        printf("Очереди пусты.\n");
        return;
    }

    qsort(all, total, sizeof(Request*), compare_requests);

    printf("\n--- Список всех заявок (по приоритету) ---\n");
    for (int i = 0; i < total; i++) {
        printf("ID: %d | %-15s | Приоритет: %d | Статус: %d\n",
            all[i]->id, all[i]->username, all[i]->priority, all[i]->status);
    }
}

void save_to_file(const char* filename, Department* deps, int dep_count) {
    FILE* file = fopen(filename, "w");
    if (!file) return;

    for (int i = 0; i < dep_count; i++) {
        for (int j = 0; j < deps[i].queue_count; j++) {
            Node* curr = deps[i].sub_queues[j]->head;
            while (curr) {
                fprintf(file, "%d %s %d %ld %d %d\n",
                    i,
                    curr->req->username,
                    curr->req->priority,
                    (long)curr->req->timestamp,
                    curr->req->id,
                    curr->req->status);
                curr = curr->next;
            }
        }
    }
    fclose(file);
    printf("Данные сохранены в %s\n", filename);
}

void load_from_file(const char* filename, Department* deps, int* max_id) {
    FILE* file = fopen(filename, "r");
    if (!file) return; // Файла нет при первом запуске, это нормально

    int d_idx, prio, id, status;
    char uname[64];
    long ts;

    while (fscanf(file, "%d %s %d %ld %d %d", &d_idx, uname, &prio, &ts, &id, &status) == 6) {
        Request* r = (Request*)malloc(sizeof(Request));
        r->id = id;
        strcpy(r->username, uname);
        r->priority = prio;
        r->timestamp = (time_t)ts;
        r->status = (RequestStatus)status;
        r->dep_list = NULL;

        if (status == pending || status == ready) {
            enqueue_smart(deps[d_idx].sub_queues[0], r);
            balance_department(&deps[d_idx]);
        }
        else {
            free(r); // В рамках лабы отмененные/обработанные не грузим, чтобы не усложнять
        }
        if (id >= *max_id) *max_id = id + 1;
    }
    fclose(file);
    printf("Данные успешно загружены.\n");
}

void free_all(Department* deps, int dep_count, Stack* s) {
    for (int i = 0; i < dep_count; i++) {
        for (int j = 0; j < deps[i].queue_count; j++) {
            Node* curr = deps[i].sub_queues[j]->head;
            while (curr) {
                Node* next = curr->next;
                Dependency* d = curr->req->dep_list;
                while (d) { Dependency* tmp = d->next; free(d); d = tmp; }
                free(curr->req);
                free(curr);
                curr = next;
            }
            free(deps[i].sub_queues[j]);
        }
        free(deps[i].sub_queues);
    }
    free(deps);
    Node* st = s->top;
    while (st) { Node* tmp = st->next; free(st->req); free(st); st = tmp; }
}

int main(void) {
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);

    Department* departments = init_departments();
    Stack canceled_stack = { .top = NULL };
    int choice, id_counter = 1;

    load_from_file("data.txt", departments, &id_counter);

    while (1) {
        printf("\n1. Новая заявка\n2. Обработать\n3. Список (сорт)\n4. Отмена\n5. Восстановить\n6. Перевод\n7. Поиск по ID\n8. Поиск по Имени\n9. Просмотр первой заявки\n10. Состояние структур\n0. Выход\n> ");        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            continue;
        }
        if (choice == 0) break;

        switch (choice) {
        case 1: {
            Request* r = (Request*)malloc(sizeof(Request));
            r->id = id_counter++;
            printf("Имя: "); scanf("%s", r->username);
            printf("Приоритет (1-5): "); scanf("%d", &r->priority);
            r->timestamp = time(NULL);
            r->status = pending;
            r->dep_list = NULL;

            printf("Зависит от ID (0 если нет): ");
            int dep_id; scanf("%d", &dep_id);
            if (dep_id != 0) {
                Dependency* d = (Dependency*)malloc(sizeof(Dependency));
                d->depends_on_id = dep_id;
                d->next = NULL;
                r->dep_list = d;
            }

            printf("Отдел (1-3): ");
            int d_num; scanf("%d", &d_num);
            if (d_num >= 1 && d_num <= 3) {
                enqueue_smart(departments[d_num - 1].sub_queues[0], r);
                balance_department(&departments[d_num - 1]);
            }
            else {
                printf("Неверный отдел!\n");
                free(r);
            }
            break;
        }
        case 2: {
            printf("Отдел (1-3): ");
            int d_num; scanf("%d", &d_num);
            if (d_num >= 1 && d_num <= 3) {
                process_next_in_department(&departments[d_num - 1], d_num - 1, departments, 3, &canceled_stack);
            }
            break;
        }
        case 3:
            print_all_sorted(departments, 3);
            break;
        case 4: {
            int id;
            printf("Введите ID для отмены: "); scanf("%d", &id);
            Request* target = NULL;
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < departments[i].queue_count; j++) {
                    Queue* q = departments[i].sub_queues[j];
                    Node* curr = q->head;
                    Node* prev = NULL;
                    while (curr != NULL) {
                        if (curr->req->id == id) {
                            target = curr->req;
                            remove_node(q, prev, curr);
                            balance_department(&departments[i]);
                            break;
                        }
                        prev = curr;
                        curr = curr->next;
                    }
                    if (target) break;
                }
                if (target) break;
            }
            if (target) {
                push_to_stack(&canceled_stack, target);
            }
            else {
                printf("Активная заявка с ID %d не найдена.\n", id);
            }
            break;
        }
        case 5: {
            Request* restored = pop_from_stack(&canceled_stack);
            if (restored) {
                enqueue_smart(departments[0].sub_queues[0], restored);
                printf("Заявка %d восстановлена в 1-й отдел.\n", restored->id);
            }
            else {
                printf("Стек отмены пуст.\n");
            }
            break;
        }
        case 6: {
            int id, f, t;
            printf("ID заявки: "); scanf("%d", &id);
            printf("Из отдела (1-3): "); scanf("%d", &f);
            printf("В отдел (1-3): "); scanf("%d", &t);
            if (f >= 1 && f <= 3 && t >= 1 && t <= 3) {
                transfer_request(id, departments, f - 1, t - 1, &canceled_stack);
            }
            else {
                printf("Отделы должны быть от 1 до 3.\n");
            }
            break;
        }
        case 7: {
            int id;
            printf("Введите ID: "); scanf("%d", &id);
            Request* r = find_request_globally(id, departments, 3, &canceled_stack);
            if (r) {
                printf("Найдено -> ID: %d | Имя: %s | Приоритет: %d | Статус: %d\n", r->id, r->username, r->priority, r->status);
            }
            else {
                printf("Заявка с таким ID не найдена.\n");
            }
            break;
        }
        case 8: {
            char name[64];
            printf("Введите имя: "); scanf("%s", name);
            find_by_username(departments, 3, &canceled_stack, name);
            break;
        }

        case 9: {
            peek_first_request(departments);
            break;
        }
        case 10: {
            print_structures_state(departments, 3, &canceled_stack);
            break;
        }
        default:
            printf("Неверный пункт меню.\n");
            break;
        }
    }

    save_to_file("data.txt", departments, 3);

    free_all(departments, 3, &canceled_stack);

    return 0;
}