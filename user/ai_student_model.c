#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "ai_student_impl.h"

struct model_cache {
  uchar valid[AI_TOTAL_SHARDS];
  char blocks[AI_TOTAL_SHARDS][AI_SHARD_BYTES];
};

int
ai_student_model_begin(struct ai_session *session)
{
  if(!session || session->memory)
    return -1;
  struct model_cache *cache = malloc(sizeof(*cache));
  if(!cache)
    return -1;
  memset(cache->valid, 0, sizeof(cache->valid));
  session->memory = cache;
  return 0;
}

int
ai_student_model_load(struct ai_session *session, uint family, uint shard,
                      char *block, struct ai_io *io)
{
  if(!session || !session->memory || !block || !io)
    return -1;
  uint index;
  if(family == AI_MODEL_FAMILY && shard < AI_MODEL_SHARDS)
    index = shard;
  else if(family == AI_EMBED_FAMILY && shard < AI_EMBED_SHARDS)
    index = AI_MODEL_SHARDS + shard;
  else
    return -1;
  struct model_cache *cache = session->memory;
  if(!cache->valid[index]) {
    char name[AI_SHARD_NAME_LEN];
    ai_shard_name(name, family, shard);
    int fd = open(name, O_RDONLY);
    if(fd < 0)
      return -1;
    int done = 0;
    while(done < AI_SHARD_BYTES) {
      int n = read(fd, cache->blocks[index] + done, AI_SHARD_BYTES - done);
      if(n <= 0) {
        close(fd);
        return -1;
      }
      done += n;
    }
    close(fd);
    cache->valid[index] = 1;
    if(family == AI_MODEL_FAMILY)
      io->model_read_bytes += AI_SHARD_BYTES;
    else
      io->embed_read_bytes += AI_SHARD_BYTES;
  }
  memmove(block, cache->blocks[index], AI_SHARD_BYTES);
  return 0;
}

void
ai_student_model_end(struct ai_session *session)
{
  if(session && session->memory) {
    free(session->memory);
    session->memory = 0;
  }
}
