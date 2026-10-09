#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "ai_student_impl.h"

// state[1]: fd+1; [2]: phase; [3]/[4]: next write/read request.
// state[0] belongs to the framework; [6]/[7] belong to prefetch.
int
ai_student_kv_begin(struct ai_session *session)
{
  if(!session || session->worker < 0 || session->worker >= AI_MAX_WORKERS ||
     session->requests <= 0 || session->requests > AI_MAX_REQUESTS)
    return -1;
  for(int i = 1; i <= 4; i++)
    session->state[i] = 0;
  return 0;
}

int
ai_student_kv_begin_write(struct ai_session *session)
{
  if(!session || session->state[2] != 0)
    return -1;
  char name[AI_STREAM_NAME_LEN];
  ai_student_kv_name(name, session->worker);
  unlink(name);
  int fd = open(name, O_CREATE | O_WRONLY);
  if(fd < 0) {
    // open may create the inode before file/fd allocation fails.
    unlink(name);
    return -1;
  }
  session->state[1] = fd + 1;
  session->state[2] = 1;
  return 0;
}

int
ai_student_kv_store(struct ai_session *session, int request,
                    struct kv_entry *kv, struct ai_io *io)
{
  if(!session || !kv || !io || session->state[2] != 1 ||
     request < 0 || request >= session->requests || request != session->state[3])
    return -1;
  int done = 0;
  while(done < AI_KV_FILE_BYTES) {
    int n = write(session->state[1] - 1, (char *)kv + done,
                  AI_KV_FILE_BYTES - done);
    if(n <= 0) {
      session->state[2] = -1;
      return -1;
    }
    done += n;
  }
  session->state[3]++;
  io->kv_write_bytes += AI_KV_FILE_BYTES;
  return 0;
}

int
ai_student_kv_end_write(struct ai_session *session)
{
  if(!session || session->state[2] != 1 ||
     session->state[3] != session->requests)
    return -1;
  close(session->state[1] - 1);
  session->state[1] = 0;
  session->state[2] = 2;
  return 0;
}

int
ai_student_kv_begin_read(struct ai_session *session)
{
  if(!session || session->state[2] != 2)
    return -1;
  char name[AI_STREAM_NAME_LEN];
  ai_student_kv_name(name, session->worker);
  int fd = open(name, O_RDONLY);
  if(fd < 0)
    return -1;
  session->state[1] = fd + 1;
  session->state[2] = 3;
  return 0;
}

int
ai_student_kv_restore(struct ai_session *session, int request,
                      struct kv_entry *kv, struct ai_io *io)
{
  if(!session || !kv || !io || session->state[2] != 3 ||
     request < 0 || request >= session->requests || request != session->state[4])
    return -1;
  int done = 0;
  while(done < AI_KV_FILE_BYTES) {
    int n = read(session->state[1] - 1, (char *)kv + done,
                 AI_KV_FILE_BYTES - done);
    if(n <= 0) {
      session->state[2] = -1;
      return -1;
    }
    done += n;
  }
  session->state[4]++;
  io->kv_read_bytes += AI_KV_FILE_BYTES;
  return 0;
}

void
ai_student_kv_end(struct ai_session *session)
{
  if(!session)
    return;
  if(session->state[1])
    close(session->state[1] - 1);
  if(session->state[2]) {
    char name[AI_STREAM_NAME_LEN];
    ai_student_kv_name(name, session->worker);
    unlink(name);
  }
  for(int i = 1; i <= 4; i++)
    session->state[i] = 0;
}
