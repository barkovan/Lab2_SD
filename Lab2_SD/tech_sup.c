#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>

typedef struct {
	unsigned int id;
	char username[51]; // 50 символов
	int priority; // 1-5
	long long timestamp;
} Request;

typedef struct Node {
	Request data;
	struct Node* next;
} Node;

typedef struct {
	Node* head;
	Node* tail;
	int size;
} Queue;

typedef struct {
	Node* top;
	int size;
} Stack;

void initQueue(Queue* q) {
	q->head = NULL;
	q->tail = NULL;
	q->size = 0;
}

void initStack(Stack* s) {
	s->top = NULL;
	s->size = 0;
}

Request createRequest(unsigned int id, char* username, int priority) {
	Request req;
	req.id = id;
	strncpy(req.username, username, 50);
	req.username[50] = '\0';
	req.priority = priority;
	req.timestamp = time(NULL);

	return req;
}

void printRequest(const Request* req) {
	printf("[ID: %u, User: %s, Priority: %d, Time: %lld]\n",
		req->id, req->username, req->priority, req->timestamp);
}

void addQueue(Queue* q, Request req) {
	Node* newNode = (Node*)malloc(sizeof(Node));
	if (!newNode) {
		printf("Memory allocation failed!\n");
		return;
	}

	newNode->data = req;
	newNode->next = NULL;

	if (q->tail == NULL) {
		q->head = newNode;
		q->tail = newNode;
	}
	else {
		q->tail->next = newNode;
		q->tail = newNode;
	}
	q->size++;
}

Request* peekQueue(Queue* q) {
	if (q->head == NULL) {
		return NULL;
	}
	return &(q->head->data);
}

Request popQueue(Queue* q) {
	Request emptyReq = { 0, "", 0, 0 };

	if (q->head == NULL) {
		return emptyReq;
	}

	Node* temp = q->head;
	Request data = temp->data;
	q->head = q->head->next;

	if (q->head == NULL) {
		q->tail = NULL;
	}

	free(temp);
	q->size--;
	return data;
}

void displayMenu() {
	printf("\n=== СЛУЖБА ПОДДЕРЖКИ ===\n");
	printf("1. Добавить заявку\n");
	printf("2. Просмотреть первую заявку в очереди\n");
	printf("3. Обработать заявку (удалить из очереди)\n");
	printf("4. Переместить заявку в другое подразделение\n");
	printf("5. Найти заявку\n");
	printf("6. Вывести все заявки, отсортированные по приоритету\n");
	printf("7. Отменить заявку\n");
	printf("8. Восстановить последнюю отмененную заявку\n");
	printf("9. Показать текущее состояние\n");
	printf("10. Сохранить в файл\n");
	printf("11. Загрузить из файла\n");
	printf("0. Выход\n\n");
}

void scanf_c(char* format, void* buffer)
{
	scanf(format, buffer);
	while (getchar() != '\n');
}

int main() {
	SetConsoleCP(1251);
	SetConsoleOutputCP(1251);

	Queue departments[3];
	Stack cancStack;

	// Инициализация очередей и стека
	for (int i = 0; i < 3; i++) {
		initQueue(&departments[i]);
	}
	initStack(&cancStack);

	unsigned int uniqId = 1;
	int to_exit = 0;
	int choice;

	while (!to_exit) {
		displayMenu();
		printf("Выберите действие: ");
		scanf_c("%d", &choice);

		switch (choice) {
			char username[50];
			int priority, dept;

			case 1: { // Добавить заявку
				printf("Введите имя пользователя (макс 50 символов): ");
				scanf_c("%50s", username);
				printf("Введите приоритет (1-5): ");
				scanf_c("%d", &priority);
				printf("Введите подразделение (1-3): ");
				scanf_c("%d", &dept);

				if (dept < 1 || dept > 3) {
					printf("Error: неверный номер подразделения!\n");
					break;
				}
				if (priority < 1 || priority > 5)
				{
					printf("Error: неверный номер приоритета!\n");
					break;
				}

				Request newReq = createRequest(uniqId++, username, priority);
				addQueue(&departments[dept - 1], newReq);
				printf("Заявка №%d успешно добавлена в очередь %d\n", newReq.id, dept);
				break;
			}

			case 2: { // Просмотреть первую заявку
				printf("Введите подразделение (1-3): ");
				scanf_c("%d", &dept);

				if (dept < 1 || dept > 3) {
					printf("Error: неверный номер подразделения!\n");
					break;
				}

				Request* rq = peekQueue(&departments[dept - 1]);
				if (rq) {
					printf("Первая заявка в очереди %d:\n", dept);
					printRequest(rq);
				}
				else {
					printf("Очередь %d пуста\n", dept);
				}
				break;
			}

			case 3: {
				printf("Введите подразделение (1-3): ");
				scanf_c("%d", &dept);

				if (dept < 1 || dept > 3) {
					printf("Error: неверный номер подразделения!\n");
					break;
				}

				if (departments[dept - 1].head != NULL) {
					Request processed = popQueue(&departments[dept - 1]);
					printf("Заявка №%d обработана и удалена из системы\n", processed.id);
				}
				else {
					printf("Очередь %d пуста, нечего обрабатывать\n", dept);
				}
				break;
			}

			case 4: {
				printf("ToDo\n");
				break;
			}

			case 5: {
				printf("ToDo\n");
				break;
			}

			case 6: {
				printf("ToDo\n");
				break;
			}

			case 7: {
				printf("ToDo\n");
				break;
			}

			case 8: {
				printf("ToDo\n");
				break;
			}

			case 9: {
				printf("ToDo\n");
				break;
			}

			case 10: {
				printf("ToDo\n");
				break;
			}

			case 11: {
				printf("ToDo\n");
				break;
			}

			case 0: {
				printf("Выход...\n");
				to_exit = 1;
				break;
			}

			default: {
				printf("Введена неверная команда\n");
				break;
			}
		}
	}

	return 0;
}