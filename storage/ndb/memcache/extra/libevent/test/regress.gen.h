/*
 * Automatically generated from ./regress.rpc
 */

#ifndef ___REGRESS_RPC_
#define ___REGRESS_RPC_

#include <event-config.h>
#ifdef _EVENT_HAVE_STDINT_H
#include <stdint.h>
#endif
#define EVTAG_HAS(msg, member) ((msg)->member##_set == 1)
#define EVTAG_ASSIGN(msg, member, args...) (*(msg)->base->member##_assign)(msg, ## args)
#define EVTAG_GET(msg, member, args...) (*(msg)->base->member##_get)(msg, ## args)
#define EVTAG_ADD(msg, member) (*(msg)->base->member##_add)(msg)
#define EVTAG_LEN(msg, member) ((msg)->member##_length)

struct msg;
struct kill;
struct run;

/* Tag definition for msg */
enum msg_ {
  MSG_FROM_NAME=1,
  MSG_TO_NAME=2,
  MSG_ATTACK=3,
  MSG_RUN=4,
  MSG_MAX_TAGS
};

/* Structure declaration for msg */
struct msg_access_ {
  int (*from_name_assign)(struct msg *, const char *);
  int (*from_name_get)(struct msg *, char * *);
  int (*to_name_assign)(struct msg *, const char *);
  int (*to_name_get)(struct msg *, char * *);
  int (*attack_assign)(struct msg *, const struct kill*);
  int (*attack_get)(struct msg *, struct kill* *);
  int (*run_assign)(struct msg *, int, const struct run *);
  int (*run_get)(struct msg *, int, struct run * *);
  struct run * (*run_add)(struct msg *);
};

struct msg {
  struct msg_access_ *base;

  char *from_name_data;
  char *to_name_data;
  struct kill* attack_data;
  struct run **run_data;
  int run_length;
  int run_num_allocated;

  uint8_t from_name_set;
  uint8_t to_name_set;
  uint8_t attack_set;
  uint8_t run_set;
};

struct msg *msg_new(void);
void msg_free(struct msg *);
void msg_clear(struct msg *);
void msg_marshal(struct evbuffer *, const struct msg *);
int msg_unmarshal(struct msg *, struct evbuffer *);
int msg_complete(struct msg *);
void evtag_marshal_msg(struct evbuffer *, uint32_t, 
    const struct msg *);
int evtag_unmarshal_msg(struct evbuffer *, uint32_t,
    struct msg *);
int msg_from_name_assign(struct msg *, const char *);
int msg_from_name_get(struct msg *, char * *);
int msg_to_name_assign(struct msg *, const char *);
int msg_to_name_get(struct msg *, char * *);
int msg_attack_assign(struct msg *, const struct kill*);
int msg_attack_get(struct msg *, struct kill* *);
int msg_run_assign(struct msg *, int, const struct run *);
int msg_run_get(struct msg *, int, struct run * *);
struct run * msg_run_add(struct msg *);
/* --- msg done --- */

/* Tag definition for kill */
enum kill_ {
  KILL_WEAPON=65825,
  KILL_ACTION=2,
  KILL_HOW_OFTEN=3,
  KILL_MAX_TAGS
};

/* Structure declaration for kill */
struct kill_access_ {
  int (*weapon_assign)(struct kill *, const char *);
  int (*weapon_get)(struct kill *, char * *);
  int (*action_assign)(struct kill *, const char *);
  int (*action_get)(struct kill *, char * *);
  int (*how_often_assign)(struct kill *, const uint32_t);
  int (*how_often_get)(struct kill *, uint32_t *);
};

struct kill {
  struct kill_access_ *base;

  char *weapon_data;
  char *action_data;
  uint32_t how_often_data;

  uint8_t weapon_set;
  uint8_t action_set;
  uint8_t how_often_set;
};

struct kill *kill_new(void);
void kill_free(struct kill *);
void kill_clear(struct kill *);
void kill_marshal(struct evbuffer *, const struct kill *);
int kill_unmarshal(struct kill *, struct evbuffer *);
int kill_complete(struct kill *);
void evtag_marshal_kill(struct evbuffer *, uint32_t, 
    const struct kill *);
int evtag_unmarshal_kill(struct evbuffer *, uint32_t,
    struct kill *);
int kill_weapon_assign(struct kill *, const char *);
int kill_weapon_get(struct kill *, char * *);
int kill_action_assign(struct kill *, const char *);
int kill_action_get(struct kill *, char * *);
int kill_how_often_assign(struct kill *, const uint32_t);
int kill_how_often_get(struct kill *, uint32_t *);
/* --- kill done --- */

/* Tag definition for run */
enum run_ {
  RUN_HOW=1,
  RUN_MAX_TAGS
};

/* Structure declaration for run */
struct run_access_ {
  int (*how_assign)(struct run *, const char *);
  int (*how_get)(struct run *, char * *);
};

struct run {
  struct run_access_ *base;

  char *how_data;

  uint8_t how_set;
};

struct run *run_new(void);
void run_free(struct run *);
void run_clear(struct run *);
void run_marshal(struct evbuffer *, const struct run *);
int run_unmarshal(struct run *, struct evbuffer *);
int run_complete(struct run *);
void evtag_marshal_run(struct evbuffer *, uint32_t, 
    const struct run *);
int evtag_unmarshal_run(struct evbuffer *, uint32_t,
    struct run *);
int run_how_assign(struct run *, const char *);
int run_how_get(struct run *, char * *);
/* --- run done --- */

#endif  /* ___REGRESS_RPC_ */
