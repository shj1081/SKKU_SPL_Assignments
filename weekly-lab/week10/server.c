#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAXLINE 80

typedef enum { SendFileName, Ack, SendFileData, EndOfFile } Operation;

// These pragmas are used to not pad the struct
#pragma pack(push, 1)
typedef struct {
  uint32_t operation;
  char data[MAXLINE];
  uint32_t size;
} Data;
#pragma pack(pop)

// Preparing data to be sent over the network
Data serialize_data(Data* data);
// Preparing data to be used in the program
Data deserialize_data(Data* data);

void handle_exit();
void handle_sigint(int sig);
void setup_signal_handler();

int listen_fd = -1, conn_fd = -1;

int main(int argc, char* argv[]) {
  atexit(handle_exit);
  setup_signal_handler();

  socklen_t conn_addr_len;
  struct sockaddr_in sock_addr, conn_addr;

  if (argc < 2) {
    fprintf(stderr, "Received %d arguments. Please enter port number!\n",
            argc - 1);
    exit(1);
  }
  in_port_t port = (in_port_t)strtol(argv[1], NULL, 10);
  if (errno == ERANGE) {
    fprintf(stderr, "invalid port number %s\n", argv[1]);
    exit(1);
  }

  /* Create listen socket */
  if ((listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
    printf("socket() failed.\n");
    exit(1);
  }
  memset(&sock_addr, 0, sizeof(sock_addr));
  sock_addr.sin_family = AF_INET;

  sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  sock_addr.sin_port = htons(port);

  /* Bind sockaddr (IP, etc.) to listen socket */

  if (bind(listen_fd, (struct sockaddr*)&sock_addr, sizeof(sock_addr)) < 0) {
    printf("bind() failed.\n");
    exit(2);
  }

  /* Listen to listen socket */
  if (listen(listen_fd, 5) < 0) {
    printf("listen() failed.\n");
    exit(3);
  }

  while (1) {
    puts("Waiting for connection");

    /* Accept connection request from clients */
    conn_addr_len = sizeof(conn_addr);
    if ((conn_fd = accept(listen_fd, (struct sockaddr*)&conn_addr,
                          &conn_addr_len)) < 0) {
      printf("accept() failed.\n");
      continue;
    }

    puts("Connected!");

    // Handle client
    while (1) {
      /* Get filename from client */
      Data serialized_filename_data;
      puts("Waiting for data!");

      read(conn_fd, &serialized_filename_data, sizeof(Data));

      Data filename_data = deserialize_data(&serialized_filename_data);

      /* Print filename */
      printf("Filename: %s\n", filename_data.data);

      /* Create a new file called <filename>_copy  */
      char filename_copy[MAXLINE];
      strcpy(filename_copy, filename_data.data);
      strcat(filename_copy, "_copy");

      // Open file with write only, create if not exist, truncate if exist
      int fd = open(filename_copy, O_WRONLY | O_CREAT | O_TRUNC, 0666);

      /* Send ack */
      Data ack_data;
      ack_data.operation = Ack;
      Data serialized_ack_data = serialize_data(&ack_data);

      write(conn_fd, &serialized_ack_data, sizeof(Data));

      /* Receive data and save data to <filename> _copy */
      size_t total_chars_received = 0;
      int quit = 0;  // quit if n_read_size <= 0
      // If n_read_size == 0, print "Client disconnected."
      ssize_t n_read_size;
      
      while (1) {
        Data serialized_data;
        n_read_size = recv(conn_fd, &serialized_data, sizeof(Data), 0);

        if (n_read_size == 0) {
          puts("Client disconnected.");
          quit = 1;
        }
        Data data = deserialize_data(&serialized_data);

        // Check for operation.
        // If Eof, break
        // If SendFileData, write to file and add length of bytes of file to sum

         if (data.operation == EndOfFile) {
          break;
         } else if (data.operation == SendFileData) {
          write(fd, data.data, data.size);
          total_chars_received += data.size;
         }
      }

      /* Print numbers of bytes received. */
      printf("Received %ld bytes.\n", total_chars_received);
      close(fd);

      /* Break from loop once client quits */
      if (quit)
        break;
    }
  }

  close(conn_fd);
  close(listen_fd);
  conn_fd = listen_fd = -1;

  return 0;
}

/* Helper functions */

void handle_sigint(int sig) {
  if (sig == SIGINT) {
    exit(0);
  }
}

void handle_exit() {
  if (listen_fd != -1) {
    close(listen_fd);
  }

  if (conn_fd != -1) {
    close(conn_fd);
  }
}

void setup_signal_handler() {
  struct sigaction act;
  act.sa_handler = handle_sigint;
  sigaction(SIGINT, &act, NULL);
}

// Preparing data to be sent over the network
Data serialize_data(Data* data) {
  Data serialized_data;
  serialized_data.operation = htonl((uint32_t)data->operation);
  serialized_data.size = htonl((uint32_t)data->size);
  memcpy(serialized_data.data, data->data, MAXLINE);
  return serialized_data;
}

// Preparing data to be used in the program
Data deserialize_data(Data* data) {
  Data deserialized_data;
  deserialized_data.operation = ntohl(data->operation);
  deserialized_data.size = ntohl(data->size);
  memcpy(deserialized_data.data, data->data, MAXLINE);
  return deserialized_data;
}