#include "helper.h"
#include "robust_io.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


void 
parse_args(int argc, char**argv, int *port_no, char *path_to_log_file, int *pool_size , char *dataset_path)
{
    char c;
    if(argc != 9)
    {
        robust_exit("Bad parameters. Usage: ./server -p PORT -o pathToLogFile –l poolSize –d datasetPath\n", EXIT_FAILURE);
    }
    while ((c = getopt (argc, argv, "p:o:l:d:")) != -1)
    {
        switch (c)
        {
            case 'p':
                *port_no = atoi(optarg );
                if(*port_no < 0)
                {
                    robust_exit("Port number can not be smaller than zero. Quitting the program.\n", EXIT_FAILURE);
                }
                else if(*port_no >= 1 && *port_no <=1023)
                {
                    robust_exit("Port numbers in range 1-1023 are the privileged ones. Quitting the program.\n", EXIT_FAILURE);
                }
                else if(*port_no >  65535)
                {
                    robust_exit("Port number can not be greater than 65535. Quitting the program.\n", EXIT_FAILURE);
                }
                break;
            case 'o':
                strcpy(path_to_log_file, optarg);
                break;
            case 'l':
                *pool_size = atoi(optarg );
                if(*pool_size < 2)
                {
                    robust_exit("Pool size can't be smaller than 2. Quitting the program.\n", EXIT_FAILURE);
                }
                break;
            case 'd':
                strcpy(dataset_path, optarg);
                if( access( dataset_path, F_OK ) != 0 )
                {
                   robust_exit("The specified file doesn't exist.\n", EXIT_FAILURE); 
                }
                break;
            default:
                robust_exit("Bad parameters. Usage: ./server -p PORT -o pathToLogFile –l poolSize –d datasetPath\n", EXIT_FAILURE);
        }
    }
}
///client –i id -a 127.0.0.1 -p PORT -o pathToQueryFile
void 
parse_args_c(int argc, char**argv, int *client_id, char *ip_addr, int *port_no, char *query_file)
{
    char c;
    if(argc != 9)
    {
        robust_exit("Bad parameters. Usage: ./client –i id -a 127.0.0.1 -p PORT -o pathToQueryFile\n", EXIT_FAILURE);
    }
    while ((c = getopt (argc, argv, "i:a:p:o:")) != -1)
    {
        switch (c)
        {
            case 'i':
                *client_id = atoi(optarg );
                if(*client_id < 1)
                {
                    robust_exit("Bad parameters. Usage: ./client –i id -a 127.0.0.1 -p PORT -o pathToQueryFile\n", EXIT_FAILURE);                }
                break;
            case 'a':
                strcpy(ip_addr, optarg);
                break;
            case 'p':
                *port_no = atoi(optarg );
                if(*port_no < 0)
                {
                    robust_exit("Port number can not be smaller than zero. Quitting the program.\n", EXIT_FAILURE);
                }
                else if(*port_no >= 1 && *port_no <=1023)
                {
                    robust_exit("Port numbers in range 1-1023 are the privileged ones. Quitting the program.\n", EXIT_FAILURE);
                }
                else if(*port_no >  65535)
                {
                    robust_exit("Port number can not be greater than 65535. Quitting the program.\n", EXIT_FAILURE);
                }
                break;
            case 'o':
                strcpy(query_file, optarg);
                if( access( query_file, F_OK ) != 0 )
                {
                   robust_exit("The specified file doesn't exist.\n", EXIT_FAILURE); 
                }
                break;
            default:
                
                robust_exit("Bad parameters. Usage: ./client –i id -a 127.0.0.1 -p PORT -o pathToQueryFile\n", EXIT_FAILURE);
        }
    }   
}

void 
print_args(int port_no, char *path_to_log_file, int pool_size , char *dataset_path, FILE **fp)
{
    /**
     * 
    -p 34567
    -o /home/erhan/sysprog/logfile
    -l 8
    -d /home/erhan/sysprog/dataset.csv
    **/
   fprintf(*fp,"Executing with parameters:\n");
   fprintf(*fp,"-p %d\n-o %s\n-l %d\n-d %s\n", port_no, path_to_log_file, pool_size, dataset_path);
}
void robust_pthread_create(pthread_t *t, void *(*start_routine)(void *), void *arg)
{
    if (pthread_create(t, NULL, start_routine, arg) != 0)
    {
        robust_exit("Can't create the thread. Quitting the program", EXIT_FAILURE);
    }
}


void robust_pthread_join(pthread_t t)
{
    if (pthread_join(t, NULL) != 0)
    {
        robust_exit("Can't join the thread. Quitting the program", EXIT_FAILURE);
    }
}

void robust_pthread_mutexattr_init(pthread_mutexattr_t *attr)
{
    int result;
    if ((result = pthread_mutexattr_init(attr)) != 0) {
        perror("pthread_mutexattr_init");
        robust_exit("pthread_mutexattr_init encountered an error. Quitting the program.\n", EXIT_FAILURE);
    }
}


void robust_pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type)
{
    int result;
    if ((result = pthread_mutexattr_settype(attr, type)) != 0) {
        perror("pthread_mutexattr_settype");
        robust_exit("pthread_mutexattr_settype encountered an error. Quitting the program.\n", EXIT_FAILURE);
    }
}



void robust_pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
    int result;
    if ((result = pthread_mutex_init(mutex, attr)) != 0) {
        perror("pthread_mutexattr_settype");
        robust_exit("pthread_mutexattr_settype encountered an error. Quitting the program.\n", EXIT_FAILURE);
    }
    
}

void robust_pthread_mutex_lock(pthread_mutex_t *mutex)
{
    int result;
    if ((result = pthread_mutex_lock(mutex)) != 0) {
        perror("Mutex lock:");
        robust_exit("pthread_mutex_lock encountered an error. Quitting the program.\n", EXIT_FAILURE);
    }
}

void robust_pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    int result;
    if ((result = pthread_mutex_unlock(mutex)) != 0) {
        perror("Mutex unlock:");
        robust_exit("pthread_mutex_unlock encountered an error. Quitting the program.\n", EXIT_FAILURE);
    }
}



  