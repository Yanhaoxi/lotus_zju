#include "Utils/General/MultiThreading.h"

// Initializes a pthread mutex.
PPMutex::PPMutex() {
  int res = pthread_mutex_init(&mutex, nullptr);
  is_valid = (res == 0);
}

// Destroys the pthread mutex.
PPMutex::~PPMutex() {
  pthread_mutex_destroy(&mutex);
  is_valid = false;
}

// Returns true if the mutex was successfully initialized.
bool PPMutex::isValid() { return is_valid; }

// Locks the mutex. Returns true on success.
bool PPMutex::lock() {
  if (!isValid()) {
    return false;
  }

  int res = pthread_mutex_lock(&mutex);
  return res == 0;
}

// Unlocks the mutex. Returns true on success.
bool PPMutex::unlock() {
  if (!isValid()) {
    return false;
  }

  int res = pthread_mutex_unlock(&mutex);
  return res == 0;
}

// Initializes a pthread read-write lock.
PPReadWriteLock::PPReadWriteLock() {
  int res = pthread_rwlock_init(&mutex, nullptr);
  is_valid = (res == 0);
}

// Destroys the pthread read-write lock.
PPReadWriteLock::~PPReadWriteLock() {
  pthread_rwlock_destroy(&mutex);
  is_valid = false;
}

// Returns true if the read-write lock was successfully initialized.
bool PPReadWriteLock::isValid() { return is_valid; }

// Acquires a read lock. Returns true on success.
bool PPReadWriteLock::rdLock() {
  if (!isValid()) {
    return false;
  }

  int res = pthread_rwlock_rdlock(&mutex);
  return res == 0;
}

// Acquires a write lock. Returns true on success.
bool PPReadWriteLock::wrLock() {
  if (!isValid()) {
    return false;
  }

  int res = pthread_rwlock_wrlock(&mutex);
  return res == 0;
}

// Releases a read lock. Returns true on success.
bool PPReadWriteLock::rdUnlock() {
  if (!isValid()) {
    return false;
  }

  int res = pthread_rwlock_unlock(&mutex);
  return res == 0;
}

// Releases a write lock. Returns true on success.
bool PPReadWriteLock::wrUnlock() {
  if (!isValid()) {
    return false;
  }

  int res = pthread_rwlock_unlock(&mutex);
  return res == 0;
}

// Initializes thread-specific data with a destructor.
PPThreadData::PPThreadData(void (*destructor)(void *)) {
  int res = pthread_key_create(&key, destructor);
  is_valid = (res == 0);
}

// Destroys the thread-specific data key.
PPThreadData::~PPThreadData() {
  pthread_key_delete(key);
  is_valid = false;
}

// Returns true if the thread data was successfully initialized.
bool PPThreadData::isValid() { return is_valid; }

// Sets thread-specific data. Returns true on success.
bool PPThreadData::setValue(void *data) {
  if (!isValid()) {
    return false;
  }

  int res = pthread_setspecific(key, data);
  return res == 0;
}

// Gets thread-specific data. Returns nullptr if invalid.
void *PPThreadData::getValue() {
  if (!isValid()) {
    return nullptr;
  }

  return pthread_getspecific(key);
}

// Returns the current thread ID.
PPThreadIDType getCurrentThreadID() { return pthread_self(); }