#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <argon2.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>


// Task Queue Definition
typedef struct TaskNode {
    int conn_fd;
    uint32_t is_logged_in; // Store the user ID if logged in, otherwise 0
    struct TaskNode* next;
} TaskNode;

typedef struct {
    TaskNode* front;
    TaskNode* rear;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} TaskQueue;

void init_task_queue(TaskQueue* queue) {
    queue->front = NULL;
    queue->rear = NULL;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

void enqueue_task(TaskQueue* queue, int conn_fd, uint32_t is_logged_in) {
    TaskNode* new_node = (TaskNode*)malloc(sizeof(TaskNode));
    new_node->conn_fd = conn_fd;
    new_node->is_logged_in = is_logged_in;
    new_node->next = NULL;

    pthread_mutex_lock(&queue->mutex);

    if (queue->rear == NULL) {
        queue->front = new_node;
        queue->rear = new_node;
    } else {
        queue->rear->next = new_node;
        queue->rear = new_node;
    }

    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
}

TaskNode dequeue_task(TaskQueue* queue) {
    pthread_mutex_lock(&queue->mutex);

    while (queue->front == NULL) {
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }

    TaskNode* temp = queue->front;
    TaskNode task = *temp; // Copy the task to return
    queue->front = queue->front->next;

    if (queue->front == NULL) {
        queue->rear = NULL;
    }

    free(temp);
    pthread_mutex_unlock(&queue->mutex);

    return task;
}



// Constants
#define MEMORY_USAGE 512
#define SALT_SIZE 16
#define HASH_SIZE 32
#define HASHED_PASSWORD_SIZE 128
#define TIMEOUT_SECONDS 1 // Define a timeout of 60 seconds

typedef enum {
    Termination = 0,
    Login,
    Book,
    ConfirmBooking,
    CancelBooking,
    Logout
} Action;

#pragma pack(push, 1)

typedef struct {
    uint32_t user;
    uint32_t size;
    uint8_t action;
    uint8_t* data;
} Request;

typedef struct {
    uint32_t code;
    uint32_t size;
    uint8_t* data;
} Response;

#pragma pack(pop)

typedef struct User {
    uint32_t user_id;
    struct User* next;
} User;

// Function Prototypes
void handle_exit();
void handle_sigint(int sig);
void setup_signal_handler();
void generate_salt(uint8_t* salt);
void hash_password(char* password, char* hashed_password);
int validate_password(char* password_to_validate, char* hashed_password);
int login(uint32_t user, char* password);
void send_tlv_response(int fd, Response response);
int recv_tlv_request(int fd, Request* request);
int data_to_int(uint8_t* data, size_t size);
char* data_to_string(uint8_t* data, size_t size);
void* worker_thread(void* arg);
int get_num_cores();

// Global Variables
int listen_fd = -1;
pthread_mutex_t seat_mutexes[256];
uint32_t seats[256] = {0};
pthread_mutex_t password_file_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t user_list_mutex = PTHREAD_MUTEX_INITIALIZER;
User* user_list_head = NULL;
TaskQueue task_queue;
int THREAD_POOL_SIZE;

// Main Function
int main(int argc, char* argv[]) {
    atexit(handle_exit);
    setup_signal_handler();

    THREAD_POOL_SIZE = get_num_cores();

    socklen_t conn_addr_len;
    struct sockaddr_in sock_addr, conn_addr;

    if (argc < 2) {
        fprintf(stderr, "Received %d arguments. Please enter port number!\n", argc - 1);
        exit(1);
    }

    in_port_t port = (in_port_t)strtol(argv[1], NULL, 10);
    if (errno == ERANGE) {
        fprintf(stderr, "invalid port number %s\n", argv[1]);
        exit(1);
    }

    for (int i = 0; i < 256; i++) {
        pthread_mutex_init(&seat_mutexes[i], NULL);
    }

    if ((listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        printf("socket() failed.\n");
        exit(1);
    }
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    sock_addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr*)&sock_addr, sizeof(sock_addr)) < 0) {
        printf("bind() failed.\n");
        exit(2);
    }

    if (listen(listen_fd, 5) < 0) {
        printf("listen() failed.\n");
        exit(3);
    }

    init_task_queue(&task_queue);

    pthread_t threads[THREAD_POOL_SIZE];
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_create(&threads[i], NULL, worker_thread, NULL);
    }

    puts("Waiting for connection");

    while (1) {
        conn_addr_len = sizeof(conn_addr);
        int conn_fd = accept(listen_fd, (struct sockaddr*)&conn_addr, &conn_addr_len);
        if (conn_fd < 0) {
            printf("accept() failed.\n");
            continue;
        }

        enqueue_task(&task_queue, conn_fd, 0);
    }

    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_join(threads[i], NULL);
    }

    close(listen_fd);
    return 0;
}

void* worker_thread(void* arg) {
    while (1) {
        TaskNode task = dequeue_task(&task_queue); // Get the task including login status
        int conn_fd = task.conn_fd;
        uint32_t logged_in_user = task.is_logged_in;

        while (1) {
            Request request;
            memset(&request, 0, sizeof(Request));

            fd_set read_fds;
            struct timeval timeout;
            int activity;

            // Initialize the file descriptor set
            FD_ZERO(&read_fds);
            FD_SET(conn_fd, &read_fds);

            // Set timeout
            timeout.tv_sec = TIMEOUT_SECONDS;
            timeout.tv_usec = 0;

            // Wait for activity on the connection with a timeout
            activity = select(conn_fd + 1, &read_fds, NULL, NULL, &timeout);

            // if timeout occurs
            if (activity == 0) {
                goto end;
            } 

            if (recv_tlv_request(conn_fd, &request) == -1) {
                puts("Client disconnected.");
                close(conn_fd);
                break;
            }

            Response response;
            User* current;
            User* prev;
            int keep_connection_active = 1;

            switch (request.action) {
                case Termination:
                    if (data_to_int(request.data, request.size) == 0 && request.user == 0) {
                        puts("Terminating connection.");
                        response.code = 0;
                        response.size = 0;
                        response.data = NULL;
                        send_tlv_response(conn_fd, response);
                        close(conn_fd);
                        keep_connection_active = 0;
                    } else {
                        puts("Invalid termination request.");
                        response.code = 1;
                        response.size = 0;
                        response.data = NULL;
                        send_tlv_response(conn_fd, response);
                    }
                    break;

                case Login:
                    printf("User %u: Attempting to login.\n", request.user);

                    // user shoulbe positive integer
                    if (request.user == 0) {
                        response.code = 4;
                        response.size = 0;
                        send_tlv_response(conn_fd, response);
                        break;
                    }
                    

                    // Check if the user is already logged in
                    if (logged_in_user != 0) {
                        response.code = 2;
                        response.size = 0;
                        send_tlv_response(conn_fd, response);
                        break;
                    }

                    pthread_mutex_lock(&user_list_mutex);
                    current = user_list_head;
                    while (current) {
                        if (current->user_id == request.user) {
                            break;
                        }
                        current = current->next;
                    }

                    if (current) {
                        response.code = 1;
                        response.size = 0;
                        send_tlv_response(conn_fd, response);
                        pthread_mutex_unlock(&user_list_mutex);
                        break;
                    }

                    if (login(request.user, (char*)request.data) > 0) {
                        response.code = 0;
                        response.size = 0;
                        logged_in_user = request.user;

                        User* new_user = (User*)malloc(sizeof(User));
                        if (new_user) {
                            new_user->user_id = request.user;
                            new_user->next = user_list_head;
                            user_list_head = new_user;
                        }
                    } else {
                        response.code = 3;
                        response.size = 0;
                    }
                    pthread_mutex_unlock(&user_list_mutex);

                    send_tlv_response(conn_fd, response);
                    break;

                case Book:
                    printf("User %u: Attempting to book seat %d.\n", request.user, data_to_int(request.data, request.size));
                    if (logged_in_user != request.user) {
                        response.code = 1;
                        send_tlv_response(conn_fd, response);
                        break;
                    }

                    int seat = data_to_int(request.data, request.size);
                    if (seat < 1 || seat > 256) {
                        response.code = 3;
                        send_tlv_response(conn_fd, response);
                        break;
                    }

                    int seat_index = seat - 1;
                    pthread_mutex_lock(&seat_mutexes[seat_index]);
                    if (seats[seat_index] == 0) {
                        seats[seat_index] = request.user;
                        response.code = 0;
                        printf("Booking confirmed for seat %d.\n", seat);
                    } else {
                        response.code = 2;
                        printf("Booking failed for seat %d: already booked.\n", seat);
                    }
                    pthread_mutex_unlock(&seat_mutexes[seat_index]);

                    response.data = (uint8_t*)&seat;
                    response.size = sizeof(seat);
                    send_tlv_response(conn_fd, response);
                    break;

                case ConfirmBooking:
                    printf("User %u: Confirming booking.\n", request.user);

                    if (logged_in_user != request.user) {
                        response.code = 1;
                        response.size = 0;
                        send_tlv_response(conn_fd, response);
                        break;
                    }

                    if (data_to_int(request.data, request.size) == 0) {
                        char* available_seats = malloc(4096 * sizeof(char));
                        available_seats[0] = '\0';

                        for (int i = 0; i < 256; i++) {
                            pthread_mutex_lock(&seat_mutexes[i]);
                            if (seats[i] == 0) {
                                char buffer[10];
                                sprintf(buffer, "%d,", i + 1);
                                strcat(available_seats, buffer);
                            }
                            pthread_mutex_unlock(&seat_mutexes[i]);
                        }

                        size_t len = strlen(available_seats);
                        if (len > 0) {
                            available_seats[len - 1] = '\0';
                        }
                        response.data = (uint8_t*)available_seats;
                        response.size = strlen(available_seats);
                        response.code = 0;
                    } else if (request.size == 0){
                        char booked_seat[2048] = {0};
                        for (int i = 0; i < 256; i++) {
                            pthread_mutex_lock(&seat_mutexes[i]);
                            if (seats[i] == request.user) {
                                sprintf(booked_seat + strlen(booked_seat), "%d,", i + 1);
                            }
                            pthread_mutex_unlock(&seat_mutexes[i]);
                        }

                        if (strlen(booked_seat) > 0) {
                            booked_seat[strlen(booked_seat) - 1] = '\0';
                        }

                        response.data = (uint8_t*)booked_seat;
                        response.size = strlen(booked_seat);
                        response.code = 0;
                    } else {
                        response.code = 2;
                        response.size = 0;
                        response.data = NULL;
                    }

                    send_tlv_response(conn_fd, response);
                    break;

                case CancelBooking:
                    printf("User %u: Attempting to cancel booking for seat %d.\n", request.user, data_to_int(request.data, request.size));

                    if (logged_in_user != request.user) {
                        response.code = 1;
                        response.size = 0;
                        send_tlv_response(conn_fd, response);
                        break;
                    }

                    int seat_to_cancel = data_to_int(request.data, request.size);
                    if (seat_to_cancel < 1 || seat_to_cancel > 256) {
                        response.code = 3;
                        send_tlv_response(conn_fd, response);
                        break;
                    }

                    int seat_index_to_cancel = seat_to_cancel - 1;
                    pthread_mutex_lock(&seat_mutexes[seat_index_to_cancel]);
                    if (seats[seat_index_to_cancel] == request.user) {
                        seats[seat_index_to_cancel] = 0;
                        response.code = 0;
                        printf("Booking cancelled for seat %d.\n", seat_to_cancel);
                    } else {
                        response.code = 2;
                        printf("Booking cancellation failed for seat %d: not booked by user.\n", seat_to_cancel);
                    }
                    pthread_mutex_unlock(&seat_mutexes[seat_index_to_cancel]);

                    response.data = (uint8_t*)&seat_to_cancel;
                    response.size = sizeof(seat_to_cancel);
                    send_tlv_response(conn_fd, response);
                    break;

                case Logout:
                    printf("User %u: Logging out.\n", request.user);

                    if (logged_in_user != request.user) {
                        printf("Logout failed for user %u.\n", request.user);
                        response.code = 1;
                        response.size = 0;
                        send_tlv_response(conn_fd, response);
                        break;
                    }

                    pthread_mutex_lock(&user_list_mutex);
                    response.code = 1;
                    response.size = 0;

                    current = user_list_head;
                    prev = NULL;
                    while (current) {
                        if (current->user_id == request.user) {
                            if (prev) {
                                prev->next = current->next;
                            } else {
                                user_list_head = current->next;
                            }
                            free(current);
                            response.code = 0;
                            response.size = 0;
                            logged_in_user = 0;
                            break;
                        }
                        prev = current;
                        current = current->next;
                    }
                    pthread_mutex_unlock(&user_list_mutex);

                    send_tlv_response(conn_fd, response);
                    break;

                default:
                    printf("Unknown action %d from user %u.\n", request.action, request.user);
                    response.code = 1;
                    response.size = 0;
                    send_tlv_response(conn_fd, response);
                    break;
            }

            free(request.data);
end:
            // Re-enqueue the connection if it is still active
            if (keep_connection_active) {
                enqueue_task(&task_queue, conn_fd, logged_in_user);
                break; // Move to next connection in the task queue
            }
        }
    }
    return NULL;
}



// Signal handler for SIGINT
void handle_sigint(int sig) {
    if (sig == SIGINT) {
        exit(0);
    }
}

// Exit handler to close the listening socket
void handle_exit() {
    if (listen_fd != -1) {
        close(listen_fd);
    }
}

// Setup signal handler for SIGINT
void setup_signal_handler() {
    struct sigaction act;
    act.sa_handler = handle_sigint;
    sigaction(SIGINT, &act, NULL);
}

void generate_salt(uint8_t* salt) {
    int fd = open("/dev/urandom", O_RDONLY);
    read(fd, salt, SALT_SIZE);
    close(fd);
}

void hash_password(char* password, char* hashed_password) {
    char hash[HASHED_PASSWORD_SIZE];
    uint8_t salt[SALT_SIZE];
    generate_salt(salt);
    argon2id_hash_encoded(2, MEMORY_USAGE, 1, password,
                          strlen(password), salt, SALT_SIZE, HASH_SIZE,
                          hash, HASHED_PASSWORD_SIZE);
    strcpy(hashed_password, hash);
}

int validate_password(char* password_to_validate,
                      char* hashed_password) {
    if (argon2id_verify(hashed_password, password_to_validate,
                        strlen(password_to_validate)) == ARGON2_OK) {
        return 1;
    } else {
        return 0;
    }
}

int login(uint32_t user, char* password) {
    FILE* file;
    pthread_mutex_lock(&password_file_mutex);
    file = fopen("/tmp/passwords.tsv", "a+");

    if (!file) {
        perror("Failed to open file");
        pthread_mutex_unlock(&password_file_mutex);
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), file) != NULL) {
        if (line[strlen(line) - 1] == '\n') {
            line[strlen(line) - 1] = '\0';
        }

        char* user_id = strtok(line, "\t");
        char* hashed_password = strtok(NULL, "\t");
        if (user_id && hashed_password && user == (uint32_t)atoi(user_id)) {
            if (validate_password(password, hashed_password)) {
                fclose(file);
                pthread_mutex_unlock(&password_file_mutex);
                return 1;
            } else {
                fclose(file);
                pthread_mutex_unlock(&password_file_mutex);
                return -2;  // Invalid password
            }
        }
    }

    if (ftell(file) == 0 || fseek(file, 0, SEEK_END) == 0) {
        char hashed_password[HASHED_PASSWORD_SIZE];
        hash_password(password, hashed_password);
        fprintf(file, "%u\t%s\n", user, hashed_password);
    }

    fclose(file);
    pthread_mutex_unlock(&password_file_mutex);
    return 2;
}

void send_tlv_response(int fd, Response response) {
    size_t total_size = sizeof(response.code) + sizeof(response.size) + response.size;
    uint8_t* buffer = malloc(total_size);

    memcpy(buffer, &response.code, sizeof(response.code));
    memcpy(buffer + sizeof(response.code), &response.size, sizeof(response.size));
    if (response.size > 0) {
        memcpy(buffer + sizeof(response.code) + sizeof(response.size), response.data, response.size);
    }

    write(fd, buffer, total_size);
    free(buffer);
}

int recv_tlv_request(int fd, Request* request) {
    uint8_t header[sizeof(request->user) + sizeof(request->action) + sizeof(request->size)];
    if (read(fd, header, sizeof(header)) != sizeof(header)) {
        return -1;
    }

    memcpy(&request->user, header, sizeof(request->user));
    memcpy(&request->size, header + sizeof(request->user), sizeof(request->size));
    memcpy(&request->action, header + sizeof(request->user) + sizeof(request->size), sizeof(request->action));

    if (request->size > 0) {
        request->data = (uint8_t*)malloc(request->size);
        if (read(fd, request->data, request->size) == 0) {
            return -1;
        }
    } else {
        request->data = NULL;
    }
    return 0;
}

int data_to_int(uint8_t* data, size_t size) {
    if (size == 0) {
        return -1;
    }
    int num = 0;
    for (size_t i = 0; i < size; i++) {
        num += data[i] << (8 * i);
    }
    return num;
}

char* data_to_string(uint8_t* data, size_t size) {
    char* str = malloc(size);
    memcpy(str, data, size);
    return str;
}

int get_num_cores() {
  cpu_set_t cpu_set;
  sched_getaffinity(0, sizeof(cpu_set), &cpu_set);
  return CPU_COUNT_S(sizeof(cpu_set), &cpu_set);
}