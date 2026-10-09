#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "ai_student_impl.h"

enum { EMPTY, PENDING, READY };
struct window {
  int helper, request_fd, data_fd, phase, request, next;
  char data[AI_KV_FILE_BYTES];
};
struct reply { int request, status; };

static struct window *
window_get(struct ai_session *session)
{
  uint64 address = (uint)session->state[6] |
                   ((uint64)(uint)session->state[7] << 32);
  return (struct window *)address;
}

static int
read_exact(int fd, void *buffer, int bytes)
{
  int done = 0;
  while(done < bytes) {
    int n = read(fd, (char *)buffer + done, bytes - done);
    if(n <= 0)
      return -1;
    done += n;
  }
  return 0;
}

static int
write_exact(int fd, void *buffer, int bytes)
{
  int done = 0;
  while(done < bytes) {
    int n = write(fd, (char *)buffer + done, bytes - done);
    if(n <= 0)
      return -1;
    done += n;
  }
  return 0;
}

static void
helper_loop(struct ai_session *session, int requests, int replies)
{
  char name[AI_STREAM_NAME_LEN], data[AI_KV_FILE_BYTES];
  ai_student_kv_name(name, session->worker);
  int fd = open(name, O_RDONLY);
  int next = 0, request;
  while(read_exact(requests, &request, sizeof(request)) == 0) {
    struct reply response = { request, -1 };
    if(fd >= 0 && request == next && request < session->requests &&
       read_exact(fd, data, sizeof(data)) == 0)
      response.status = 0;
    if(write_exact(replies, &response, sizeof(response)) < 0 ||
       response.status < 0 || write_exact(replies, data, sizeof(data)) < 0)
      break;
    next++;
  }
  if(fd >= 0)
    close(fd);
  close(requests);
  close(replies);
  exit(0);
}

int
ai_student_prefetch_begin(struct ai_session *session)
{
  if(!session || window_get(session) || session->state[2] != 3)
    return -1;
  struct window *window = malloc(sizeof(*window));
  if(!window)
    return -1;
  memset(window, 0, sizeof(*window));
  int requests[2], replies[2];
  if(pipe(requests) < 0) {
    free(window);
    return -1;
  }
  if(pipe(replies) < 0) {
    close(requests[0]);
    close(requests[1]);
    free(window);
    return -1;
  }
  int pid = fork();
  if(pid < 0) {
    close(requests[0]);
    close(requests[1]);
    close(replies[0]);
    close(replies[1]);
    free(window);
    return -1;
  }
  if(pid == 0) {
    close(requests[1]);
    close(replies[0]);
    // Reopen rather than share the parent's file offset through fork.
    close(session->state[1] - 1);
    helper_loop(session, requests[0], replies[1]);
  }
  close(requests[0]);
  close(replies[1]);
  window->helper = pid;
  window->request_fd = requests[1];
  window->data_fd = replies[0];
  uint64 address = (uint64)window;
  session->state[6] = (uint)address;
  session->state[7] = (uint)(address >> 32);
  return 0;
}

int
ai_student_prefetch_next(struct ai_session *session, int request)
{
  if(!session)
    return -1;
  struct window *window = window_get(session);
  if(!window || window->phase != EMPTY || request != window->next ||
     request < 0 || request >= session->requests)
    return -1;
  // Only a small command is sent here; reading the record is asynchronous.
  if(write_exact(window->request_fd, &request, sizeof(request)) < 0)
    return -1;
  window->request = request;
  window->phase = PENDING;
  return 0;
}

int
ai_student_prefetch_wait(struct ai_session *session, int request)
{
  if(!session)
    return -1;
  struct window *window = window_get(session);
  if(!window || window->phase != PENDING || request != window->request)
    return -1;
  struct reply response;
  if(read_exact(window->data_fd, &response, sizeof(response)) < 0 ||
     response.request != request || response.status != 0 ||
     read_exact(window->data_fd, window->data, sizeof(window->data)) < 0)
    return -1;
  window->phase = READY;
  return 0;
}

int
ai_student_prefetch_restore(struct ai_session *session, int request,
                            struct kv_entry *kv, struct ai_io *io)
{
  if(!session || !kv || !io)
    return -1;
  struct window *window = window_get(session);
  if(!window || window->phase != READY || request != window->request ||
     request != window->next)
    return -1;
  memmove(kv, window->data, sizeof(window->data));
  io->kv_read_bytes += sizeof(window->data);
  window->next++;
  session->state[4] = window->next;
  window->phase = EMPTY;
  return 0;
}

void
ai_student_prefetch_end(struct ai_session *session)
{
  if(!session)
    return;
  struct window *window = window_get(session);
  if(window) {
    // Closing the data pipe also wakes a helper blocked on a partial write.
    close(window->request_fd);
    close(window->data_fd);
    wait(0);
    free(window);
  }
  session->state[6] = session->state[7] = 0;
}
