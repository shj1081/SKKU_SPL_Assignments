#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// ========== Constants ==========
// action codes
typedef enum {
    Termination = 0,
    Login,
    Book,
    ConfirmBooking,
    CancelBooking,
    Logout
} Action;

#pragma pack(push, 1)  // exact fit - no padding

// request structure
typedef struct {
    uint32_t user;
    uint32_t size;  // size of data
    uint8_t action;
    uint8_t* data;
} Request;

// response structure
typedef struct {
    uint32_t code;
    uint32_t size;  // size of data
    uint8_t* data;
} Response;

#pragma pack(pop)  // back to original setting

// ========== Function Prototypes ==========
void setup_conn_fd(char* host, in_port_t port);          // setup connection file descriptor
void handle_exit();                                      // close connection on exit
void handle_sigint(int sig);                             // signal handler for SIGINT
void setup_signal_handler();                             // setup signal handler for SIGINT
void send_tlv_request(int fd, Request* request);         // send TLV message
int recv_tlv_response(int fd, Response* response);       // receive TLV message
void exec_action(Request* request);                      // execute action based on request
int data_to_int(uint8_t* data, size_t size);             // convert data to integer
char* data_to_string(uint8_t* data, size_t size);        // convert data to string
void print_error(Request* request, Response* response);  // print error message
void process_input(char* input);                         // process input from file or REPL

// ========== Global Variables ==========
int conn_fd = -1;      // connection file descriptor
int login_status = 0;  // 0: not logged in, else: user ID

// ========== Main Function ==========
int main(int argc, char* argv[]) {
    atexit(handle_exit);     // close connection on exit
    setup_signal_handler();  // setup signal handler for SIGINT

    // check for correct number of arguments
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Received %d arguments. Please enter host address and port number! (Optional: file name)\n", argc - 1);
        exit(1);
    }

    // get port number
    in_port_t port = (in_port_t)strtol(argv[2], NULL, 10);
    if (errno == ERANGE) {
        fprintf(stderr, "invalid port number %s\n", argv[2]);
        exit(1);
    }

    // setup connection file descriptor
    setup_conn_fd(argv[1], port);

    if (argc == 4) {
        // Process file input
        FILE* file = fopen(argv[3], "r");
        if (!file) {
            fprintf(stderr, "Failed to open file %s\n", argv[3]);
            exit(1);
        }

        char* line = NULL;
        size_t len = 0;
        ssize_t read;
        while ((read = getline(&line, &len, file)) != -1) {
            // Remove newline character from the end of the line
            if (line[read - 1] == '\n') {
                line[read - 1] = '\0';
            }

            process_input(line);
        }
        free(line);
        fclose(file);

        // Send termination request after processing the file
        Request term_request = {.user = 0, .action = Termination};
        int num = 0;
        term_request.data = (uint8_t*)&num;
        term_request.size = sizeof(num);
        exec_action(&term_request);
    } else {
        // Interactive REPL mode
        while (1) {
            char* input = readline("Enter <user action data> : ");  // get user input

            // if input is empty, continue to next iteration
            if (input == NULL || strlen(input) == 0) {
                continue;
            }
            add_history(input);  // add input to history

            process_input(input);
            free(input);  // free the input buffer
        }
    }

    return 0;
}

// ========== Function Definitions ==========

// process input from file or REPL
void process_input(char* input) {
    char* ptr;
    long long user = strtoll(input, &ptr, 10);
    long long action = strtoll(ptr, &ptr, 10); // if action is not a number, it will be 0

    // for positive user ID
    if (user < 0 && action != Termination) {
        user = 0;
    }

    Request request = {.user = user,
                       .action = action,
                       .size = 0,
                       .data = NULL};

    // get data and set size based on action
    char* data = strtok(ptr, " ");
    switch (request.action) {
        case Login:  // check the password (string)
            if (data != NULL) {
                request.data = (uint8_t*)data;
                request.size = strlen(data) + 1;  // include null terminator
                // Do not free data here since it's used in the request and managed elsewhere
            }
            break;
        default:  // Check for any remaining input(numbers or N/A)
            if (data == NULL) {
                // No additional data present
                request.data = NULL;
                request.size = 0;
            } else {
                // Convert string to number and store as data if there's additional input
                long long num = strtoll(data, NULL, 10);
                request.data = (uint8_t*)&num;
                request.size = sizeof(num);
            }
            break;
    }

    // execute action based on request
    exec_action(&request);
}

// setup connection file descriptor
void setup_conn_fd(char* host, in_port_t port) {
    struct hostent* host_entry;
    struct sockaddr_in sock_addr;

    if ((conn_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        fprintf(stderr, "socket() failed.\n");
        exit(1);
    }

    if ((host_entry = gethostbyname(host)) == NULL) {
        fprintf(stderr, "invalid hostname %s\n", host);
        exit(1);
    }

    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    memcpy(&sock_addr.sin_addr.s_addr, host_entry->h_addr_list[0], host_entry->h_length);
    sock_addr.sin_port = htons(port);

    if (connect(conn_fd, (struct sockaddr*)&sock_addr, sizeof(sock_addr)) < 0) {
        fprintf(stderr, "connect() failed.\n");
        exit(1);
    }
}

// signal handler for SIGINT
void handle_sigint(int sig) {
    if (sig == SIGINT) {
        exit(0);
    }
}

// close connection on exit
void handle_exit() {
    if (conn_fd != -1) {
        close(conn_fd);
    }
}

// setup signal handler for SIGINT
void setup_signal_handler() {

    struct sigaction act;
    act.sa_handler = handle_sigint;
    sigaction(SIGINT, &act, NULL);
}

// send TLV message : user, action, data size, data
void send_tlv_request(int fd, Request* request) {
    // allocate buffer for request
    size_t total_size = sizeof(request->user) + sizeof(request->action) + sizeof(request->size) + request->size;
    uint8_t* buffer = malloc(total_size);

    // copy user, size, action and data to buffer
    memcpy(buffer, &request->user, sizeof(request->user));
    memcpy(buffer + sizeof(request->user), &request->size, sizeof(request->size));
    memcpy(buffer + sizeof(request->user) + sizeof(request->size), &request->action, sizeof(request->action));
    if (request->size > 0) {
        memcpy(buffer + sizeof(request->user) + sizeof(request->action) + sizeof(request->size), request->data, request->size);
    }

    // send buffer to server
    write(fd, buffer, total_size);
    free(buffer);
}

// receive TLV message : code, data size, data
int recv_tlv_response(int fd, Response* response) {
    uint8_t header[sizeof(response->code) + sizeof(response->size)];
    if (read(fd, header, sizeof(header)) != sizeof(header)) {
        return -1;
    }

    // copy code and size from header
    memcpy(&response->code, header, sizeof(response->code));
    memcpy(&response->size, header + sizeof(response->code), sizeof(response->size));

    // allocate buffer for data if not NULL
    if (response->size > 0) {
        response->data = (uint8_t*)malloc(response->size);
        if (read(fd, response->data, response->size) == 0) {
            return -1;
        }
    }

    // set data to NULL if size is 0
    else {
        response->data = NULL;
    }
    return 0;
}

// execute action based on request
void exec_action(Request* request) {
    Response response;
    switch (request->action) {
        // ACTION 0: Termination
        case Termination:
            // send with wrong arguments
            if (data_to_int(request->data, request->size) != 0 || request->user != 0) {
                // just send and receive response (result will be failure)
                send_tlv_request(conn_fd, request);
                recv_tlv_response(conn_fd, &response);
                print_error(request, &response);
            }
            // check if user is logged in
            else if (login_status != 0) {
                // send logout request to server
                Request logout_request = {.user = login_status,
                                          .action = Logout,
                                          .size = 0,
                                          .data = NULL};
                send_tlv_request(conn_fd, &logout_request);

                // if response is received, set login status to 0 and
                if (recv_tlv_response(conn_fd, &response) == 0) {
                    // assume that logout never fails
                    login_status = 0;
                    send_tlv_request(conn_fd, request);
                    // receive response and terminate (fail condition checked above so always success)
                    if (recv_tlv_response(conn_fd, &response) == 0) {
                        // close connection
                        close(conn_fd);
                        conn_fd = -1;
                        printf("Connection Terminated.\n");
                        exit(0);
                    }
                }
            }

            // if not logged in, just send termination request
            else {
                // send and receive response (result will be failure)
                send_tlv_request(conn_fd, request);
                recv_tlv_response(conn_fd, &response);
                if (response.code == 0) {
                    printf("Connection Terminated.\n");
                    exit(0);
                } else {
                    print_error(request, &response);
                }
            }
            break;

        // ACTION 1: Login
        case Login:
            // send login request to server
            send_tlv_request(conn_fd, request);

            // receive response
            if (recv_tlv_response(conn_fd, &response) == 0) {
                // if login is successful, set login status to user ID
                if (response.code == 0) {
                    login_status = request->user;
                    printf("Logged in successfully.\n");
                } else {
                    print_error(request, &response);
                }
            }
            break;

        // ACTION 2: Book
        case Book:
            // send book request to server
            send_tlv_request(conn_fd, request);

            // receive response
            if (recv_tlv_response(conn_fd, &response) == 0) {
                // if booking is successful, print booking ID
                if (response.code == 0) {
                    int booked_seat = data_to_int(response.data, response.size);
                    printf("Seat %d booked.\n", booked_seat);
                } else {
                    print_error(request, &response);
                }
            }
            break;

        // ACTION 3: ConfirmBooking
        case ConfirmBooking:
            // send confirm booking request to server
            send_tlv_request(conn_fd, request);

            // receive response
            if (recv_tlv_response(conn_fd, &response) == 0) {
                if (response.code == 0) {
                    // Check if data was intended to fetch available seats (e.g., request->size == 0 might mean "fetch all")
                    if (request->size != 0 && data_to_int(request->data, request->size) == 0) {
                        // When there's a specific flag or condition indicating to fetch available seats
                        if (response.data != NULL && response.size > 0) {
                            char* available_seats = data_to_string(response.data, response.size);
                            printf("Available seats: %s\n", available_seats);
                            free(available_seats);
                        } else {
                            printf("No available seats or no data received.\n");
                        }
                    } else {
                        // Handle confirmed bookings or specific data processing
                        if (response.data != NULL && response.size > 0) {
                            char* booked_seats = data_to_string(response.data, response.size);
                            printf("Booked the seats %s.\n", booked_seats);
                            free(booked_seats);
                        } else {
                            printf("Did not book any seats.\n");
                        }
                    }
                } else {
                    print_error(request, &response);
                }
            } else {
                printf("Failed to receive a valid response from server.\n");
            }
            break;

        // ACTION 4: CancelBooking
        case CancelBooking:
            // send cancel booking request to server
            send_tlv_request(conn_fd, request);

            // receive response
            if (recv_tlv_response(conn_fd, &response) == 0) {
                if (response.code == 0) {
                    printf("Canceled seat number %d.\n", data_to_int(response.data, response.size));
                } else {
                    print_error(request, &response);
                }
            }
            break;

        // ACTION 5: Logout
        case Logout:
            // send logout request to server
            send_tlv_request(conn_fd, request);

            // receive response
            if (recv_tlv_response(conn_fd, &response) == 0) {
                // if logout is successful, set login status to 0
                if (response.code == 0) {
                    login_status = 0;
                    printf("Logged out successfully.\n");
                } else {
                    print_error(request, &response);
                }
            }
            break;
        default:
            send_tlv_request(conn_fd, request);
            recv_tlv_response(conn_fd, &response);
            print_error(request, &response);
            break;
    }

    // free data if not NULL
    if (response.data != NULL) {
        free(response.data);
    }
}

// change little endian hex data to integer
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
    // Allocate memory for the string plus one extra byte for the null terminator
    char* str = malloc(size + 1);

    if (str == NULL) {
        // Handle memory allocation failure if necessary
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }

    // Copy the data into the new string buffer
    memcpy(str, data, size);

    // Explicitly set the null terminator at the end of the string
    str[size] = '\0';

    return str;
}

// print error message
void print_error(Request* request, Response* response) {
    if (request->action == Termination) {
        if (response->code == 1) {
            printf("Failed to disconnect as arguments are invalid\n");
        }
    }

    else if (request->action == Login) {
        if (response->code == 1) {
            printf("Failed to login as user is active\n");
        }

        else if (response->code == 2) {
            printf("Failed to login as client is active\n");
        }

        else if (response->code == 3) {
            printf("Failed to login as password is incorrect\n");
        }
        else if (response->code == 4) {
            printf("Failed to login as user should be positive integer\n");
        }
    }

    else if (request->action == Book) {
        if (response->code == 1) {
            printf("Failed to book as user is not logged in\n");
        }

        else if (response->code == 2) {
            printf("Failed to book as seat is unavailable\n");
        }

        else if (response->code == 3) {
            printf("Failed to book as seat number is out of range\n");
        }
    }

    else if (request->action == ConfirmBooking) {
        if (response->code == 1) {
            printf("Failed to confirm booking as user is not logged in\n");
        }
        else if (response->code == 2) {
            printf("Failed to confirm booking as data for action is not defined\n");
        }
    }

    else if (request->action == CancelBooking) {
        if (response->code == 1) {
            printf("Failed to cancel booking as user is not logged in\n");
        } 
        
        else if (response->code == 2) {
            printf("Failed to cancel booking as user did not book the specified seat\n");
        }

        else if (response->code == 3) {
            printf("Failed to cancel booking as seat number is out of range\n");
        }
    }

    else if (request->action == Logout) {
        if (response->code == 1) {
            printf("Failed to logout as user is not logged in\n");
        }
    }

    else {
        printf("Action %d is unknown\n", (int)request->action);
    }
}
