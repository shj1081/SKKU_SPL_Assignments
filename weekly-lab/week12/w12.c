#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct {
  uint64_t* vector;
  uint64_t* matrix_row;
  size_t size;
  uint64_t result;
} thread_data;

void* thread_mvm(void* arg) {
  thread_data* data = (thread_data*)arg;
  uint64_t sum = 0;
  for (size_t i = 0; i < data->size; i++) {
    sum += data->matrix_row[i] * data->vector[i];
  }
  data->result = sum;
  return NULL;
}

#ifdef DEBUG
void single_threaded_matrix_multiplication(const uint64_t* matrix,
                                           const uint64_t* vector,
                                           const uint64_t* result,
                                           uint64_t row_size,
                                           uint64_t column_size) {
  uint64_t single_threaded_result[row_size];

  for (size_t row_i = 0; row_i < row_size; row_i++) {
    uint64_t sum = 0;
    const uint64_t* row = matrix + row_i * column_size;
    for (size_t column_i = 0; column_i < column_size; column_i++) {
      sum += row[column_i] * vector[column_i];
    }
    single_threaded_result[row_i] = sum;
  }

  // Compare result
  for (size_t i = 0; i < row_size; i++) {
    if (result[i] != single_threaded_result[i]) {
      // Print result
      printf("*** Answer ***\n");
      for (size_t i = 0; i < row_size; i++) {
        printf("[ %llu ]\n", (unsigned long long)single_threaded_result[i]);
      }
      printf("\n");

      printf("Error: Wrong answer! (%llu, %llu)\n", 
             (unsigned long long)result[i],
             (unsigned long long)single_threaded_result[i]);

      exit(1);
    }
  }
}
#endif

int main(int argc, char* argv[]) {
  if (argc != 3) {
    printf("Usage: %s <row> <column>\n", argv[0]);
    exit(1);
  }

  srand(time(NULL));

  size_t row_size = strtol(argv[1], NULL, 10);
  size_t col_size = strtol(argv[2], NULL, 10);
  pthread_t tid[row_size];
  thread_data t_data[row_size];
  uint64_t matrix[row_size][col_size];
  uint64_t vector[col_size];
  uint64_t result[row_size];

  // Initialize matrix & vector with random values
  for (size_t i = 0; i < row_size; i++) {
    for (size_t j = 0; j < col_size; j++) {
      matrix[i][j] = rand() % 100;  // random values between 0 and 99
    }
  }

  for (size_t i = 0; i < col_size; i++) {
    vector[i] = rand() % 100;  // random values between 0 and 99
  }

  // Print matrix & vector
  printf("*** Matrix ***\n");
  for (size_t i = 0; i < row_size; i++) {
    for (size_t j = 0; j < col_size; j++) {
      printf("[ %llu ]", (unsigned long long)matrix[i][j]);
      if (j != col_size - 1)
        printf(" ");
    }
    printf("\n\n");
  }

  printf("*** Vector ***\n");
  for (size_t i = 0; i < col_size; i++) {
    printf("[ %llu ] ", (unsigned long long)vector[i]);
    if (i != col_size - 1)
      printf(" ");
  }
  printf("\n\n");

  // Create threads
  for (size_t i = 0; i < row_size; i++) {
    t_data[i].vector = vector;
    t_data[i].matrix_row = matrix[i];
    t_data[i].size = col_size;
    if (pthread_create(&tid[i], NULL, thread_mvm, &t_data[i]) != 0) {
      perror("Failed to create thread");
      return 1;
    }
  }

  // Join threads and store result
  for (size_t i = 0; i < row_size; i++) {
    if (pthread_join(tid[i], NULL) != 0) {
      perror("Failed to join thread");
      return 1;
    }
    result[i] = t_data[i].result;
  }

  // Print result
  printf("*** Result ***\n");
  for (size_t i = 0; i < row_size; i++) {
    printf("[ %llu ]\n", (unsigned long long)result[i]);
  }
  printf("\n");

#ifdef DEBUG
  single_threaded_matrix_multiplication((const uint64_t*)matrix, vector, result,
                                        row_size, col_size);
#endif

  return 0;
}
