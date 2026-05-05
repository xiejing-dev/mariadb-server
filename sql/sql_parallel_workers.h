#ifndef SQL_PARALLEL_WORKERS_H
#define SQL_PARALLEL_WORKERS_H

#include "mariadb.h"
#include "sql_class.h"
#include "mysqld.h"
#include "sql_error.h"

extern MYSQL_THD create_background_thd();
extern void destroy_background_thd(MYSQL_THD thd);
extern void *thd_attach_thd(MYSQL_THD thd);
extern void thd_detach_thd(void *save);

// PWT Parallel Worker Thread


/*
  Message Types
*/
class pwt_error_message
{
public:
  uint worker_errno;
  uint code;
  Sql_condition::enum_warning_level level;
  char *message;
};

class pwt_data_message
{
public:
  TABLE *tmp_table;
};


/*
  Event type. Inherits ilink so it can live in an I_List<pwt_queued_event>.
*/
class pwt_queued_event : public ilink
{
public:
    pwt_error_message   *error;
    pwt_data_message    *data;
};

class pwt_management;


#define CONNECTION_NAME_THREAD         "Thread"
#define CONNECTION_NAME_THREAD_LENGTH  6
#define THREAD_ID_LENGTH               20         // ull can occupy 20 chars
#define WORKER_NAME                    "parallel worker"
#define WORKER_NAME_LENGTH             15

/*
  Parallel Worker Thread specific attributes
*/
class pwt_worker
{
public:
  THD             *thd;
  pwt_management  *manager;
  pthread_t       pthread;
  mysql_mutex_t   LOCK_worker;
  mysql_cond_t    COND_worker;
  char            conn_name[MAX_THREAD_NAME+1];
  /*
    This is displayed in information_schema.processlist.info
    Currently "Thread X: parallel worker Y"
  */
  char            info[CONNECTION_NAME_THREAD_LENGTH+
                       THREAD_ID_LENGTH+3+
                       WORKER_NAME_LENGTH+
                       THREAD_ID_LENGTH+2];
  bool            joined;
  bool            finished;
  killed_state    killed;
  void            *parallel_scan_job;
};


/*
  Class to create, manage and eventually destroy a "team" of worker threads.
*/
class pwt_management : public Sql_alloc
{
public:
  pwt_worker        *workers;
  uint              nworkers;
  I_List<pwt_queued_event> parallel_messages;
  mysql_cond_t      COND_pwt_new_message;
  mysql_mutex_t     LOCK_pwt_manager;
  mysql_mutex_t     LOCK_pwt_thread;
  /*
    Set under LOCK_pwt_thread when a worker fails to allocate a queued event.
    The manager surfaces a single ER_OUTOFMEMORY warning so the user sees
    that worker diagnostics were dropped instead of silently disappearing.
  */
  bool              messages_dropped;
  pwt_management():
    workers(nullptr),
    nworkers(0),
    messages_dropped(false)
    {}
  ~pwt_management()
  {
    if (workers)
      join_parallel_workers(current_thd);
  }
  bool init_parallel_workers(THD *thd);
  void join_parallel_workers(THD *thd);
  void free_queue();
};

#endif
