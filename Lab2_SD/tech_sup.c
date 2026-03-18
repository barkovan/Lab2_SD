#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>
#define MAX_QUEUE_SIZE 10
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

        if (parent == NULL || parent->status != processed) {
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

    new_q->head = NULL;
    new_q->tail = NULL;
    new_q->count = 0;

    Node* current = old_q->head;
    for (int i = 0; i < mid - 1; i++) {
        current = current->next;
    }

    new_q->head = current->next;
    new_q->tail = old_q->tail;
    new_q->count = old_q->count - mid;

    current->next = NULL;
    old_q->tail = current;
    old_q->count = mid;

    dev->queue_count++;
    dev->sub_queues = (Queue**)realloc(dev->sub_queues, sizeof(Queue*) * dev->queue_count);
    dev->sub_queues[dev->queue_count - 1] = new_q;

    printf("Очередь %d переполнена. Создана новая подочередь.\n", q_idx);
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

void pop_from_stack(Stack* s) {
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
                printf("Заявка 5d (%s) обработана и удалена.\n", curr->req->id, curr->req->username);
                curr->req->status = processed;

                remove_node(q, prev, curr);
                balance_department(dev);
                return;
            }
            prev = curr;
            curr = curr->next;
        }
    }

    printf("Нет заявок, готовых к обработке в данном подразделени.\n");
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

int main(void) {
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);


    return 0;
}