#include "buffer.h"

#include "stdlib.h"

void buffer_init(CharBuffer *buffer) {
  buffer->data = NULL;
  buffer->len = 0;
  buffer->cap = 0;
}

void buffer_free(CharBuffer *buffer) {
  free(buffer->data);
  buffer->data = NULL;
  buffer->len = 0;
  buffer->cap = 0;
}

int buffer_push(CharBuffer *buffer, char ch) {
  // grow capacity when the next byte would overflow.
  if (buffer->len + 1 > buffer->cap) {
    size_t next_cap = buffer->cap == 0 ? 16 : buffer->cap * 2;
    char *next = realloc(buffer->data, next_cap);
    if (next == NULL) {
      return -1;
    }
    buffer->data = next;
    buffer->cap = next_cap;
  }

  buffer->data[buffer->len++] = ch;
  return 0;
}

char *buffer_release(CharBuffer *buffer) {
  char *result = buffer->data;
  buffer->data = NULL;
  buffer->len = 0;
  buffer->cap = 0;
  return result;
}
