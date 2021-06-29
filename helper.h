
#ifndef _HELPER_H_
#define _HELPER_H_
#include <stdio.h>
#include <pthread.h>
#define MAXFILELEN 255
#define SQL_DELIMETER "NEXT_PACKET>"
void 
parse_args(int argc, char**argv, int *port_no, char *path_to_log_file, int *pool_size , char *dataset_path);
void 
print_args(int port_no, char *path_to_log_file, int pool_size , char *dataset_path, FILE**fp);
void 
parse_args_c(int argc, char**argv, int *client_id, char *ip_addr, int *port_no, char *query_file);

void robust_pthread_create(pthread_t *t, void *(*start_routine)(void *), void *arg);

void robust_pthread_join(pthread_t t);

void robust_pthread_mutexattr_init(pthread_mutexattr_t *attr);

void robust_pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type);

void robust_pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);

void robust_pthread_mutex_lock(pthread_mutex_t *mutex);

void robust_pthread_mutex_unlock(pthread_mutex_t *mutex);


#endif
