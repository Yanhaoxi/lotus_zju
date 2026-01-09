#include <pthread.h>
#include <stdio.h>

int shared_counter = 0;
pthread_mutex_t lock;

void *increment_thread(void *arg) {
  for (int i = 0; i < 1000; i++) {
    pthread_mutex_lock(&lock);
    shared_counter++;  // Protected
    pthread_mutex_unlock(&lock);
  }
  return NULL;
}

void *buggy_thread(void *arg) {
  for (int i = 0; i < 1000; i++) {
    shared_counter++;  // Bug: race condition
  }
  return NULL;
}

int main(void) {
  pthread_t t1, t2;
  pthread_mutex_init(&lock, NULL);

  pthread_create(&t1, NULL, increment_thread, NULL);
  pthread_create(&t2, NULL, buggy_thread, NULL);

  pthread_join(t1, NULL);
  pthread_join(t2, NULL);

  printf("Counter: %d\n", shared_counter);

  pthread_mutex_destroy(&lock);
  return 0;
}
