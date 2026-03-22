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

// Создание и добавление узла в очередь
Node* create_node(Request* r) {
    Node* new_node = (Node*)malloc(sizeof(Node));
    if (!new_node) return NULL;
    new_node->req = r;
    new_node->next = NULL;
    return new_node;
}

// Умная вставка в очередь с учетом приоритета
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

// Глобальный поиск заявки
Request* find_request_globally(int id, Department* departments, int dep_count, Stack* canceled_stack, int id_counter) {
    if (id >= id_counter || id <= 0) return NULL;

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

// Проверка зависимостей
int is_ready_for_processing(Request* req, Department* deps, int dep_count, Stack* canceled_stack, int id_counter) {
    Dependency* current_dep = req->dep_list;

    while (current_dep != NULL) {
        int dep_id = current_dep->depends_on_id;

        if (dep_id >= id_counter) return 0;

        Request* parent = find_request_globally(dep_id, deps, dep_count, canceled_stack, id_counter);

        if (parent == NULL) {
            current_dep = current_dep->next;
            continue;
        }

        if (parent->status != processed) {
            return 0;
        }

        current_dep = current_dep->next;
    }

    return 1;
}

// Балансировка
void split_queue(Department* dev, int q_idx);
void merge_queues(Department* dev, int q_idx);

void balance_department(Department* dev) {
    for (int i = 0; i < dev->queue_count; i++) {
        if (dev->sub_queues[i]->count > MAX_QUEUE_SIZE) {
            split_queue(dev, i);
            break;
        }
        else if (dev->sub_queues[i]->count < MIN_QUEUE_SIZE && dev->queue_count > 1) {
            merge_queues(dev, i);
            break;
        }
    }
}

// Разделение очередей
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

    Queue** temp = (Queue**)realloc(dev->sub_queues, sizeof(Queue*) * dev->queue_count);
    if (!temp) {
        printf("[Ошибка] Не удалось выделить память при разделении очереди.\n");
        dev->queue_count--;
        free(new_q);

        old_q->tail = new_q->tail;
        old_q->count += new_q->count;
        current->next = new_q->head;
        return;
    }
    dev->sub_queues = temp;
    dev->sub_queues[dev->queue_count - 1] = new_q;
    printf("[Система] Очередь %d переполнена. Создана подочередь.\n", q_idx + 1);
}

// Слияние очередей
void merge_queues(Department* dev, int q_idx) {
    if (dev->queue_count < 2) return;
    int target_idx, source_idx;

    if (q_idx == 0) {
        target_idx = 0;
        source_idx = 1;
    }
    else {
        target_idx = q_idx - 1;
        source_idx = q_idx;
    }

    Queue* target = dev->sub_queues[target_idx];
    Queue* source = dev->sub_queues[source_idx];

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

    for (int i = source_idx; i < dev->queue_count - 1; i++) {
        dev->sub_queues[i] = dev->sub_queues[i + 1];
    }

    dev->queue_count--;

    Queue** temp = (Queue**)realloc(dev->sub_queues, sizeof(Queue*) * dev->queue_count);
    if (temp != NULL || dev->queue_count == 0) {
        dev->sub_queues = temp;
    }
    printf("[Система] Очереди %d и %d объединены. Осталось подочередей: %d\n",
        target_idx + 1, source_idx + 1, dev->queue_count);
}

// Инициализация департамента
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

// Вставка в стек
void push_to_stack(Stack* s, Request* r) {
    Node* new_node = create_node(r);
    if (!new_node) return;
    r->status = canceled;
    new_node->next = s->top;
    s->top = new_node;
    printf("[Система] Заявка %d отменена и помещена в стек.\n", r->id);
}

// Достать из стека
Request* pop_from_stack(Stack* s) {
    if (s->top == NULL) return NULL;
    Node* temp = s->top;
    Request* r = temp->req;
    s->top = s->top->next;
    free(temp);
    r->status = pending;
    return r;
}

// Удаление узла из очереди
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

// Обработка заявки
void process_next_in_department(Department* dev, int dep_id, Department* all_deps, int dep_count, Stack* s, int id_counter) {
    for (int i = 0; i < dev->queue_count; i++) {
        Queue* q = dev->sub_queues[i];
        Node* curr = q->head;
        Node* prev = NULL;

        while (curr != NULL) {
            if (is_ready_for_processing(curr->req, all_deps, dep_count, s, id_counter)) {
                printf("[Система] Заявка %d (%s) обработана и удалена.\n", curr->req->id, curr->req->username);

                Request* to_delete = curr->req;
                remove_node(q, prev, curr);

                Dependency* d = to_delete->dep_list;
                while (d) {
                    Dependency* tmp = d->next;
                    free(d);
                    d = tmp;
                }
                free(to_delete);

                balance_department(dev);
                return;
            }
            prev = curr;
            curr = curr->next;
        }
    }
    printf("[Система] Нет заявок, готовых к обработке (либо очереди пусты, либо зависимости не выполнены).\n");
}

// Перемещение заявки из одного департамента в другой
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
        printf("[Система] Заявка %d переведена в отдел %d.\n", id, to_dep + 1);
    }
    else {
        printf("[Ошибка] Заявка с таким ID не найдена в исходном отделе.\n");
    }
}

// Просмотр первой заявки в департаменте
void peek_first_request(Department* deps) {
    int d_num;
    printf("Введите номер отдела для просмотра (1-3): ");
    scanf("%d", &d_num);
    if (d_num >= 1 && d_num <= 3) {
        Department* dev = &deps[d_num - 1];
        if (dev->queue_count > 0 && dev->sub_queues[0]->head != NULL) {
            Request* r = dev->sub_queues[0]->head->req;
            printf(">>> Первая заявка: ID %d | Имя: %s | Приоритет: %d | Статус: %d\n",
                r->id, r->username, r->priority, r->status);
        }
        else {
            printf("[Система] Очереди отдела %d пусты.\n", d_num);
        }
    }
    else {
        printf("[Ошибка] Неверный номер отдела.\n");
    }
}

// Вывод состояния структур
void print_structures_state(Department* deps, int dep_count, Stack* s) {
    printf("\n=== ТЕКУЩЕЕ СОСТОЯНИЕ СТРУКТУР ПАМЯТИ ===\n");
    for (int i = 0; i < dep_count; i++) {
        printf("Отдел %d (Всего подочередей: %d):\n", i + 1, deps[i].queue_count);
        for (int j = 0; j < deps[i].queue_count; j++) {
            printf("  [Очередь %d]: ", j + 1);
            Node* curr = deps[i].sub_queues[j]->head;
            while (curr) {
                printf("{ID:%d, Prio:%d} -> ", curr->req->id, curr->req->priority);
                curr = curr->next;
            }
            printf("NULL\n");
        }
    }
    printf("\nСтек отмененных заявок:\n  [Вершина] -> ");
    Node* st = s->top;
    while (st) {
        printf("{ID:%d} -> ", st->req->id);
        st = st->next;
    }
    printf("NULL\n=========================================\n");
}

// Поиск по имени пользователя
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
            printf("[Стек] ID: %d, Приоритет: %d, Статус: %d\n", st_curr->req->id, st_curr->req->priority, st_curr->req->status);
            found = 1;
        }
        st_curr = st_curr->next;
    }
    if (!found) printf("[Ошибка] Заявки пользователя %s не найдены.\n", name);
}

// Сравнение заявок по приоритету
int compare_requests(const void* a, const void* b) {
    Request* r1 = *(Request**)a;
    Request* r2 = *(Request**)b;
    if (r1->priority != r2->priority)
        return r2->priority - r1->priority;
    return (int)(r1->timestamp - r2->timestamp);
}

// Вывод всех отсортированных заявок
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
        printf("[Система] Очереди пусты.\n");
        return;
    }

    qsort(all, total, sizeof(Request*), compare_requests);

    printf("\n--- Список всех заявок (по приоритету) ---\n");
    for (int i = 0; i < total; i++) {
        printf("ID: %d | %-15s | Приоритет: %d | Статус: %d\n",
            all[i]->id, all[i]->username, all[i]->priority, all[i]->status);
    }
}

// Сохранение данных в файл
void save_to_file(const char* filename, Department* deps, int dep_count, Stack* s) {
    FILE* file = fopen(filename, "w");
    if (!file) return;

    // Очереди
    for (int i = 0; i < dep_count; i++) {
        for (int j = 0; j < deps[i].queue_count; j++) {
            Node* curr = deps[i].sub_queues[j]->head;
            while (curr) {
                int d_cnt = 0;
                Dependency* d_tmp = curr->req->dep_list;
                while (d_tmp) { d_cnt++; d_tmp = d_tmp->next; }

                fprintf(file, "%d %s %d %ld %d %d %d",
                    i, curr->req->username, curr->req->priority,
                    (long)curr->req->timestamp, curr->req->id, curr->req->status, d_cnt);

                d_tmp = curr->req->dep_list;
                while (d_tmp) {
                    fprintf(file, " %d", d_tmp->depends_on_id);
                    d_tmp = d_tmp->next;
                }
                fprintf(file, "\n");
                curr = curr->next;
            }
        }
    }

    // Стек
    Node* st = s->top;
    while (st) {
        int d_cnt = 0;
        Dependency* d_tmp = st->req->dep_list;
        while (d_tmp) { d_cnt++; d_tmp = d_tmp->next; }

        fprintf(file, "-1 %s %d %ld %d %d %d",
            st->req->username, st->req->priority,
            (long)st->req->timestamp, st->req->id, st->req->status, d_cnt);

        d_tmp = st->req->dep_list;
        while (d_tmp) {
            fprintf(file, " %d", d_tmp->depends_on_id);
            d_tmp = d_tmp->next;
        }
        fprintf(file, "\n");
        st = st->next;
    }

    fclose(file);
    printf("[Файл] Данные успешно сохранены в %s\n", filename);
}

// Загрузка данных из файла
void load_from_file(const char* filename, Department* deps, int* max_id, Stack* s) {
    FILE* file = fopen(filename, "r");
    if (!file) return;

    int d_idx, prio, id, status, d_count;
    char uname[64];
    long ts;

    int canceled_capacity = 10;
    Request** canceled_arr = (Request**)malloc(canceled_capacity * sizeof(Request*));
    if (!canceled_arr) { 
        fclose(file); 
        return; 
    }
    int canceled_count = 0;

    while (fscanf(file, "%d %63s %d %ld %d %d %d", &d_idx, uname, &prio, &ts, &id, &status, &d_count) == 7) {
        Request* r = (Request*)malloc(sizeof(Request));
        r->id = id;
        strcpy(r->username, uname);
        r->priority = prio;
        r->timestamp = (time_t)ts;
        r->status = (RequestStatus)status;
        r->dep_list = NULL;

        for (int k = 0; k < d_count; k++) {
            int dep_id;
            fscanf(file, "%d", &dep_id);
            Dependency* new_dep = (Dependency*)malloc(sizeof(Dependency));
            new_dep->depends_on_id = dep_id;
            new_dep->next = r->dep_list;
            r->dep_list = new_dep;
        }

        if (status == pending && d_idx >= 0) {
            enqueue_smart(deps[d_idx].sub_queues[0], r);
            balance_department(&deps[d_idx]);
        }
        else if (status == canceled) {
            if (canceled_count >= canceled_capacity) {
                canceled_capacity *= 2;
                Request** temp_arr = (Request**)realloc(canceled_arr, canceled_capacity * sizeof(Request*));
                if (temp_arr) canceled_arr = temp_arr;
                else {
                    free(r);
                    continue;
                }
            }
            canceled_arr[canceled_count++] = r;
        }
        else free(r);
        if (id >= *max_id) *max_id = id + 1;
    }

    for (int i = canceled_count - 1; i >= 0; i--) {
        Node* new_node = create_node(canceled_arr[i]);
        if (new_node) {
            new_node->next = s->top;
            s->top = new_node;
        }
    }

    free(canceled_arr);
    fclose(file);
    printf("[Файл] Данные загружены. Следующий ID: %d\n", *max_id);
}

// Освобождение памяти
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

    load_from_file("data.txt", departments, &id_counter, &canceled_stack);

    while (1) {
        printf("\n== СИСТЕМА СЛУЖБЫ ПОДДЕРЖКИ ==\n");
        printf("1. Новая заявка\n2. Обработать\n3. Список (сорт)\n4. Отмена\n5. Восстановить\n6. Перевод\n7. Поиск по ID\n8. Поиск по Имени\n9. Просмотр первой заявки\n10. Состояние структур\n0. Выход\n> ");

        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            continue;
        }

        if (choice == 0) break;

        switch (choice) {
        case 1: {
            Request* r = (Request*)malloc(sizeof(Request));
            r->id = id_counter++;
            r->status = pending;
            r->dep_list = NULL;

            printf("Имя пользователя: ");
            scanf("%63s", r->username);

            printf("Приоритет (1-5): ");
            scanf("%d", &r->priority);
            if (r->priority < 1 || r->priority > 5)
            {
                printf("[Ошибка] Ошибка диапазона!\n");
            }

            r->timestamp = time(NULL);

            printf("Введите ID заявок, от которых зависит эта (0 для завершения):\n");
            while (1) {
                int dep_id;
                printf("Зависит от ID: ");
                if (scanf("%d", &dep_id) != 1) break;
                if (dep_id == 0) break;

                if (dep_id >= r->id || dep_id <= 0) {
                    printf("[Ошибка] ID %d недоступен. Пропускаем.\n", dep_id);
                    continue;
                }

                Dependency* d = (Dependency*)malloc(sizeof(Dependency));
                if (d) {
                    d->depends_on_id = dep_id;
                    d->next = r->dep_list;
                    r->dep_list = d;
                }
            }

            printf("Отдел назначения (1-3): ");
            int d_num;
            scanf("%d", &d_num);
            if (d_num >= 1 && d_num <= 3) {
                enqueue_smart(departments[d_num - 1].sub_queues[0], r);
                balance_department(&departments[d_num - 1]);
                printf("[Система] Заявка %d добавлена.\n", r->id);
            }
            else {
                printf("[Ошибка] Ошибка диапазона!\n");
                Dependency* curr_d = r->dep_list;
                while (curr_d) { Dependency* t = curr_d->next; free(curr_d); curr_d = t; }
                free(r);
                id_counter--;
            }
            break;
        }

        case 2: {
            printf("Номер отдела (1-3): ");
            int d_num; scanf("%d", &d_num);
            if (d_num >= 1 && d_num <= 3) {
                process_next_in_department(&departments[d_num - 1], d_num - 1, departments, 3, &canceled_stack, id_counter);
            }
            break;
        }

        case 3:
            print_all_sorted(departments, 3);
            break;

        case 4: {
            int id;
            printf("ID для отмены: "); scanf("%d", &id);
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
            if (target) push_to_stack(&canceled_stack, target);
            else printf("[Ошибка] Заявка %d не найдена.\n", id);
            break;
        }

        case 5: {
            Request* restored = pop_from_stack(&canceled_stack);
            if (restored) {
                enqueue_smart(departments[0].sub_queues[0], restored);
                balance_department(&departments[0]);
                printf("[Система] Заявка %d восстановлена в 1-й отдел.\n", restored->id);
            }
            else {
                printf("[Ошибка] Стек пуст.\n");
            }
            break;
        }

        case 6: {
            int id, f, t;
            printf("ID: "); scanf("%d", &id);
            printf("Из отдела: "); scanf("%d", &f);
            printf("В отдел: "); scanf("%d", &t);
            if (f >= 1 && f <= 3 && t >= 1 && t <= 3) {
                transfer_request(id, departments, f - 1, t - 1, &canceled_stack);
            }
            break;
        }

        case 7: {
            int id;
            printf("Введите ID: "); scanf("%d", &id);
            Request* r = find_request_globally(id, departments, 3, &canceled_stack, id_counter);
            if (r) {
                printf("ID %d | %s | Prio %d | Status %d\n", r->id, r->username, r->priority, r->status);
            }
            else if (id > 0 && id < id_counter) {
                printf("[Система] Заявка %d обработана.\n", id);
            }
            else {
                printf("[Ошибка] ID не существует.\n");
            }
            break;
        }

        case 8: {
            char name[64];
            printf("Имя: "); scanf("%63s", name);
            find_by_username(departments, 3, &canceled_stack, name);
            break;
        }

        case 9:
            peek_first_request(departments);
            break;

        case 10:
            print_structures_state(departments, 3, &canceled_stack);
            break;

        default:
            break;
        }
    }

    save_to_file("data.txt", departments, 3, &canceled_stack);
    free_all(departments, 3, &canceled_stack);

    return 0;
}