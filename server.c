#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include "helper.h"
#include "robust_io.h"
#include "alloc.h"
#include "database.h"
#include "queue.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/un.h>

#define PACKLEN 1024


    
/**
 * I learned reader-writer paradigm which gives priority to the writers from 
 * lecture slides. Therefore I'm copying the same logic and the variable names
 **/
int threads_executed = 0;
int current_sleeping_threads = 0;
int AR = 0; //Active readers.
int AW = 0; //Active writers.
int WR = 0; //Waiting readers. 
int WW = 0; //Waiting writers.
pthread_cond_t okToRead = PTHREAD_COND_INITIALIZER;
pthread_cond_t okToWrite = PTHREAD_COND_INITIALIZER;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;


char path_to_log_file[MAXFILELEN];
FILE *log_file;
FILE *prevent_fp;
volatile sig_atomic_t continue_execution = 1;
char* line_buffer = NULL; //needs to be freed
FILE *fptr = NULL; //needs to be freed
struct _database *database; //needs to be freed
int sockfd;
int pool_size;
int global_longest_line;
struct sockaddr_in host_addr;
pthread_t *threads;
struct Client_Queue *clients;
pthread_mutex_t mutex_terminate = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; //This mutex will protect the client queue also the condition variable.
pthread_mutexattr_t attr; //For error check attribute.
pthread_cond_t request_condition_variable = PTHREAD_COND_INITIALIZER;
pthread_cond_t rendezvous = PTHREAD_COND_INITIALIZER;
pthread_cond_t terminate_cond = PTHREAD_COND_INITIALIZER;
void pid_lock();
void pid_cleanup();
void becomeDaemon(int abstract_fd);
void allocate_database(unsigned int row, unsigned int col);
void free_database();
int get_line_number(char *file_name, size_t *max);
int find_column_count(char *line);
void resolve_columns(char *line);
void init_database(char *file_name);
void resolve_entries(char *entry, int row_no);
void print_columns();
void print_database();
void debug_ascii(char *s);
int tcp_connect(int port_no);
void allocate_threads(int tno);
void free_threads();
void spawn_threads(int tno);
void* common_area();
void alloc_queue();
void f_queue();
void create_mutex();
int parse_select(char *query, int *column_indexes);

int search_column(char *single_cell);
void combine_columns(int *column_indexes, char *output);
void combine_entries(int *column_indexes, char **output_list);
void print_buffer_list(char **output_list);
int SELECT(int client_fd, char *pack);
void UPDATE(int client_fd, char *pack);
int duplicate_check(char **output_list, char *single_entry, int search_limit);
int parse_update(char *query, int *effected_indexes ,int *condition_index, char **values,char *value_condition);

int make_changes_on_table(int *effected_indexes ,int condition_index, char **values,char *value_condition, int changed_column_count);
int command_classifier(char *query);
void open_log_file(char *file_name);
void close_log_file();


static void
handler()
{
    fprintf(log_file,"Termination signal received, waiting for ongoing threads to complete.\n");
    continue_execution = 0;
}


int main(int argc, char *argv[])
{

    char dataset_path[MAXFILELEN];
    int port_no;
    parse_args(argc,argv, &port_no,path_to_log_file, &pool_size , dataset_path);
    int s;
    struct sockaddr_un sun;

    //https://unix.stackexchange.com/a/219687
    s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s < 0) {
        perror("socket");
        exit(1);
    }
    memset(&sun, 0, sizeof(sun));
    sun.sun_family = AF_UNIX;
    strcpy(sun.sun_path + 1, "mihim");
    if (bind(s, (struct sockaddr *) &sun, sizeof(sun)))
    {
         fprintf(stderr,"Daemon server can only executed once. If the other daemon terminates the socket will be released.\n");
         exit(EXIT_FAILURE);
    }
   
    becomeDaemon(s);
    /*pid_lock();
    if(fclose(prevent_fp) != 0)
    {
        fprintf(stderr, "%s\n", "Couldn't close the log file. Exiting the program.\n");
    }
    becomeDaemon();
    pid_lock();*/

    open_log_file(path_to_log_file);

    setvbuf(log_file, NULL, _IONBF, 0);
    
    signal(SIGPIPE, SIG_IGN);
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = handler;
    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        fprintf(log_file, "Sigaction returned an error. Exiting the program\n");
        exit(EXIT_FAILURE);
    }
        
    
    //create_mutex(mutex);
   
    

    

    
    print_args(port_no, path_to_log_file, pool_size, dataset_path, &log_file);
    //current_sleeping_threads = pool_size;
    alloc_queue();
    
    init_database(dataset_path);
   
    allocate_threads(pool_size);
    //print_database();
    spawn_threads(pool_size);
    tcp_connect(port_no);
    

   
    free_database();
    for (size_t i = 0; i < (unsigned int)pool_size; i++)
    {
        pthread_join(threads[i], NULL);
    }

    free_threads();
    f_queue();
    fprintf(log_file, "All threads have terminated, server shutting down.");

    close_log_file();
    
    return 0;
}

void allocate_database(unsigned int row, unsigned int col)
{
    int i, j, k;
    database = robust_calloc(1, sizeof(struct _database), &log_file);
    database->row = row - 1; //Don't need column row.
    database->col = col;
    database->column_names = robust_calloc(col, sizeof(char *), &log_file);
    
    for(i = 0 ; i < (int)col ; ++i)
    {
        database->column_names[i] = robust_calloc(256, sizeof(char), &log_file);
    }


    database->table = robust_calloc(database->row, sizeof(char **), &log_file);
    for(k = 0; k < (int)database->row; k++) 
    {
        database->table[k] = robust_calloc(database->col, sizeof(char *), &log_file);
        for(j = 0; j < (int)database->col; j++)
           database->table[k][j] = robust_calloc(256, sizeof(char), &log_file);
    }

    //printf("TABLE[%d][%d][%d] is created.\n", database->row, database->col, 256);

}
void free_database()
{
    int i, j, k;
    for(i = 0 ; i < (int)database->col ; ++i)
    {
        free(database->column_names[i]);
    }
    free(database->column_names);
    for(k = 0; k < (int)database->row; k++) 
    {
        
        for(j = 0; j < (int)database->col; j++)
           free(database->table[k][j]);
        free(database->table[k]);
    }
    free(database->table);
    free(database);


}
int
get_line_number(char *file_name, size_t *max)
{
    
    fptr = fopen(file_name,"r");
    int count = 0;
    char c;
    int longest_line = 0;
    *max = 0;
    if(fptr == NULL)
    {
        fprintf(log_file, "Can't open the file. Exiting the program.");
        fclose(fptr);
        exit(EXIT_FAILURE);
    }

    for (c = getc(fptr); c != EOF; c = getc(fptr))
    {
        if (c == '\n')
        {
            if(longest_line > (int)*max)
            {
                *max = longest_line;
            }
            count = count + 1;
            //printf("This line contains %d characters.\n", longest_line);
            longest_line = 0;
        }
        longest_line++;
    }
    global_longest_line = longest_line;
    fclose(fptr);
    return count;
}
int find_column_count(char *line)
{
    int i;
    int col_no = 0;
    int line_size = strlen(line);
    for(i = 0 ; i < line_size ; ++i)
    {
        if(line[i] == ',')
        {
            col_no++;
        }
    }
    return col_no + 1;
}
void debug_ascii(char *s)
{
    printf("Start.\n");
    int i;
    for(i = 0 ; i < (int)strlen(s) ; ++i)
    {
        printf("-%d-", s[i]);
    }
     printf("End.\n");
}

void comma_slice(char *input, char *output, int start, int end)
{
    int i;
    int counter = 0;
    if(end - start > 250)
    {
        fprintf(log_file, "Maximum cell value can't be greater than 250.\n");
        exit(EXIT_FAILURE);
    	//robust_exit("Maximum cell value can't be greater than 120.", EXIT_FAILURE);
    }
    for(i = start ; i < end ; ++i)
    {   
        output[counter++] = input[i];
    }
    output[i] = '\0';
}
void resolve_columns(char *line)
{
    int counter = 0;
    int start = 0;
    int end;
    int i;
    int line_size = strlen(line);
    char buffer[line_size];
    memset(buffer,0,line_size);
    for(i = 0 ; i < line_size ; ++i)
    {
        if(line[i] == ',' || line[i] == '\n' || line[i] == '\r')
        {
            end = i;
            comma_slice(line, buffer, start, end);
            strcpy(database->column_names[counter++], buffer);
            //printf("Line component: %s\n", buffer);
            memset(buffer,0,line_size);
            start = end + 1;
            if(line[i] == '\n' || line[i] == '\r')
                break;
        }
    }
}
void resolve_entries(char *entry, int row_no)
{
    row_no = row_no - 1;
    int insert_col = 0;
    int i;
    int start = 0;
    int end;
    int quote_encountered = 0;
    int entry_size = strlen(entry);
    char buffer[entry_size];
    for(i = 0 ; i < entry_size ; ++i)
    {
        if(entry[i] == '\"' && quote_encountered == 0)
        {
            quote_encountered = 1;
            start = i + 1;
        }
        else if((entry[i] == '\"'|| entry[i] == '\n' || entry[i] == '\r') && quote_encountered == 1)
        {
            end = i;
            comma_slice(entry, buffer, start, end);
            strncpy(database->table[row_no][insert_col++], buffer, strlen(buffer));
            //printf("\"Line component: %s\n", buffer);
            memset(buffer,0,entry_size);
            start = end + 1;
            quote_encountered = 0;
        }
        if((entry[i] == ',' || entry[i] == '\n' || entry[i] == '\r') && quote_encountered == 0)
        {
                end = i;
                if(start != end)
                {
                    comma_slice(entry, buffer, start, end);
                    strncpy(database->table[row_no][insert_col++], buffer, strlen(buffer));
                    //printf(",Line component: %s\n", buffer);
                    //printf("Start and end %d %d\n", start, end );
                    memset(buffer,0,entry_size);
                    
                }
                //if(start == end && entry[i] == ',' && entry[i - 1] != '\"')
                if(start == end && entry[i] == ',' && entry[i - 1] != '\"')
                {
                    
                    //printf("Start: %d, end: %d row: %d\n", start, end, row_no);
                    comma_slice(entry, buffer, start, end);
                    strncpy(database->table[row_no][insert_col++], "NULL", strlen("NULL"));
                    //printf(",Line component: %s\n", buffer);
                    //printf("Start and end %d %d\n", start, end );
                    memset(buffer,0,entry_size);
                }
                start = end + 1;
        }
    }
}
void print_columns()
{
    printf("\n");
    int i;
    for(i = 0 ; i < (int)database->col ; ++i)
    {
        printf("|||%s|||\t", database->column_names[i]);
    }
    printf("\n");
}
void print_database()
{
    int i,j;
    print_columns();
    for(i = 0 ; i < (int)database -> row; ++i)
    {
        for(j = 0 ; j < (int)database -> col ; ++j)
        {
            printf("||%s||    ", database->table[i][j]);
        }
        printf("\n");
    }
}

void
init_database(char *file_name)
{
    clock_t start_time, end_time;
    double elapsed_time;

    int counter = 0;
    size_t largest_line;
    int written;
    int col_count;
    int row_count = get_line_number(file_name, &largest_line);
    fptr = fopen(file_name,"r");
    if(fptr == NULL)
    {
        fprintf(log_file, "Can't open the file. Exiting the program.");
        exit(EXIT_FAILURE);
    }
    largest_line++;
    //printf("The file contains %d lines.\nLargest line contains %d characters.", row_count, largest_line);
    line_buffer = robust_calloc(largest_line,sizeof(char), &log_file);
    //printf("Largest line is %d\n", largest_line);
    start_time = clock();
    
    while((written = getline(&line_buffer,&largest_line,fptr)) != -1)
    {   
        if(counter == 0)
        {
            col_count = find_column_count(line_buffer);
            allocate_database(row_count, col_count);
            resolve_columns(line_buffer);
            counter++;
        }
        else
        {
            
            resolve_entries(line_buffer, counter);
            counter++;
            
                
        }
       
    }
    end_time = clock();
    elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    fprintf(log_file, "Dataset loaded in %.3lf seconds with %d records.\n", elapsed_time, database->row);
    free(line_buffer);
    fclose(fptr);
    
}

int tcp_connect(int port_no)
{
    /**
     * Standart procedure for tcp connect:
     *      -create socket
     *      -bind
     *      -listen
     *      -accept connections in a loop.
     * 
     *  Steps are verified from this source: https://www.gta.ufrj.br/ensino/eel878/sockets/index.html
     **/
    int i;
    int socket_fd;
    int client_fd;
    struct sockaddr_in host_address;
    struct sockaddr_in client_address;
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) == -1)
    {
        fprintf(log_file, "Socket returned an error. Server exits.\n");
        exit(EXIT_FAILURE);
        //robust_exit("Socket returned error.", EXIT_FAILURE);
    }

    int optval = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) == -1)
    {
        fprintf(log_file, "Set sock opt returned error. Server exits.\n");
        exit(EXIT_FAILURE);
        //robust_exit("Set sock opt returned error.", EXIT_FAILURE);
    }
    host_address.sin_port = htons(port_no);
    host_address.sin_family = AF_INET;
    host_address.sin_addr.s_addr = inet_addr("127.0.0.1");
    memset(&(host_address.sin_zero), '\0', 8);

    if (bind(socket_fd, (struct sockaddr *)&host_address, sizeof(struct sockaddr)) == -1)
    {
        fprintf(log_file, "Bind returned an error. Server exits.\n");
        exit(EXIT_FAILURE);
    	//perror("bind");
        //robust_exit("Bind returned error.", EXIT_FAILURE);
    }

    if (listen(socket_fd, 20) == -1)
    {
        fprintf(log_file, "Listen returned an error. Server exits.\n");
        exit(EXIT_FAILURE);
    	//perror("listen");
        //robust_exit("Listen returned error.", EXIT_FAILURE);
    }

    while(continue_execution)
    {

        socklen_t sock_len = sizeof(struct sockaddr_in);
        
        
        if((client_fd = accept(socket_fd, (struct sockaddr *)&client_address, &sock_len)) == -1)
        {
            if (errno == EINTR && continue_execution == 0)
            {
                //printf("Termination signal received, waiting for ongoing threads to complete.\n");
                break;
            }
               
            else
            {
                fprintf(log_file, "Accept returned an error. Server exits.\n");
                exit(EXIT_FAILURE);
                //perror("Accept");
                //robust_exit("Accept returned an error.Exiting the server", EXIT_FAILURE);
            }
        
        }
        robust_pthread_mutex_lock(&mutex);
        enqueue_client(&clients, client_fd);
        pthread_cond_signal(&request_condition_variable);
        robust_pthread_mutex_unlock(&mutex);
    }
    for(i = 0 ; i < pool_size ; ++i)
    {
        robust_pthread_mutex_lock(&mutex);
        enqueue_client(&clients, -1); 
        pthread_cond_signal(&request_condition_variable);
        robust_pthread_mutex_unlock(&mutex);
    }
    robust_pthread_mutex_lock(&mutex_terminate);
    while(threads_executed < pool_size)
    {
        pthread_cond_wait(&terminate_cond, &mutex_terminate);
    }
    robust_pthread_mutex_unlock(&mutex_terminate);

    return socket_fd;

}
void allocate_threads(int tno)
{
   
    threads = robust_calloc(tno, sizeof(pthread_t), &log_file);
}
void free_threads()
{
    free(threads);
}

void spawn_threads(int tno)
{
    int i;
    
    for(i = 0 ; i < tno ; ++i)
    {
        robust_pthread_create(&threads[i], common_area, NULL);
    }
}
int ll = 0;
int arrived  = 0;
void* common_area()
{
    int read;
    int client_fd;
    char pack[PACKLEN];
    robust_pthread_mutex_lock(&mutex);
    arrived++;
    if(arrived < pool_size)
    {
        pthread_cond_wait(&rendezvous, &mutex);
    }
    else
    {
        fprintf(log_file, "A pool of %d threads has been created.\n", pool_size);
        pthread_cond_broadcast(&rendezvous);
    }
    robust_pthread_mutex_unlock(&mutex);
    
   
    
    while(1)
    {   
        /*  DEQUEUE THE JOB*/
        robust_pthread_mutex_lock(&mutex);
        while(empty(clients))
        {
            current_sleeping_threads++;
            //printf("1 Current sleeping threads is %d\n", current_sleeping_threads);
            fprintf(log_file,"Thread #%lu: waiting for connection.\n", pthread_self());
            pthread_cond_wait(&request_condition_variable, &mutex);
            current_sleeping_threads--;
            if(current_sleeping_threads == 0)
            {
                fprintf(log_file,"No thread is available! (Main thread is still be able to queue jobs.)\n");
            }
            //printf("2 Current sleeping threads is %d\n", current_sleeping_threads);
        }
        client_fd = dequeue_client(clients);
        if(client_fd == -1)
        {
            robust_pthread_mutex_lock(&mutex_terminate);
            threads_executed++;
            pthread_cond_signal(&terminate_cond);
            robust_pthread_mutex_unlock(&mutex_terminate);
            robust_pthread_mutex_unlock(&mutex);
            return NULL;
        }
        else
        {
            fprintf(log_file,"A connection has been delegated to thread id #%lu\n",pthread_self());
        }
        ll++;       
        robust_pthread_mutex_unlock(&mutex);
        while((read = robust_read(client_fd, pack, PACKLEN)) > 0)
        {

            //TODO: FIX THIS. NUMBER IS NOT NEEDED.
            fprintf(log_file,"Thread #%lu: received query ‘%s‘\n", pthread_self(), pack);
           
            if(command_classifier(pack) == 1)
            {   
                robust_pthread_mutex_lock(&m);
                while((AW + WW) > 0)
                {
                    WR++;
                    pthread_cond_wait(&okToRead, &m);
                    WR--;
                }
                AR++;
                robust_pthread_mutex_unlock(&m);
                //ACCESS DB HERE.
               // sleep(1);
                int record_count = SELECT(client_fd, pack);
                fprintf(log_file,"Thread #%lu: query completed, %d records have been returned.\n", pthread_self(), record_count);

                
                robust_pthread_mutex_lock(&m);
                AR--;
                if(AR == 0 && WW > 0)
                {
                    pthread_cond_signal(&okToWrite);
                }
                robust_pthread_mutex_unlock(&m);
            }
            else
            {
                robust_pthread_mutex_lock(&m);
                while((AW + AR) > 0)
                {
                    WW++;
                    pthread_cond_wait(&okToWrite, &m);
                    WW--;
                }
                AW++;
                robust_pthread_mutex_unlock(&m);
                //ACCESS DB HERE.
                //sleep(1);
                UPDATE(client_fd, pack);
                robust_pthread_mutex_lock(&m);
                AW--;
                if(WW > 0)
                {
                    pthread_cond_signal(&okToWrite);
                }
                else if(WR > 0)
                {
                    pthread_cond_broadcast(&okToRead);
                }
                robust_pthread_mutex_unlock(&m);
            }
        }
        
    }
}

void alloc_queue()
{
    clients = allocate_queue();
}
void f_queue()
{
    free(clients->queue);
    free(clients);
}
void create_mutex(pthread_mutex_t t)
{
    robust_pthread_mutexattr_init(&attr);
    robust_pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    
    
    robust_pthread_mutex_init(&t, &attr);
}


/**
 * parses the SELECT QUERIES. Stores the relevant indexes in column_indexes array. 
 * Column indexes array should be initialized first.
 * It won't be done by this function.
 * */


void print_buffer_list(char **output_list)
{
    int i;
    for(i = 0 ; i < (int)database->row ; ++i)
    {
        printf("%s\n", output_list[i]);
    }
}

int SELECT(int client_fd, char *pack)
{
	char junk = 'j';
	int client_no = 0;
	clock_t start_time, end_time;
    double elapsed_time;
    int returned_records = 0;
    int select_type = -2;
    char done_message[] = "HALT";
    char log[256];
    memset(log,0,256);
    start_time = clock();

        /*Read the file with specified descriptor.*/
    /**
    * Allocate memory for all rows in the field. The columns will be initialized later.
    **/
    char **output_list = robust_calloc(database->row, sizeof(char *), &log_file); // Create outputlist.
    for(int i = 0 ; i < (int)database->row ; ++i)
    {
        output_list[i] = robust_calloc(1024, sizeof(char), &log_file);
    }
    char *columns = robust_calloc(1024, sizeof(char), &log_file);
    /**
    * Now initialize columns. Don't need to allocate memory for that. It's small.
    **/
    sscanf(pack, "%d %c\n", &client_no,  &junk);
    int column_indexes[database->col];
    select_type = parse_select(pack, column_indexes); //parse the query and find the specified column indexes.
    combine_columns(column_indexes, columns);
    combine_entries(column_indexes,output_list); //call combine entries. Now you have all entries. Don't forget to free them.
    //fprintf(strlenderr, "THREAD %u speaking with %d. client_fd. The message is :%s\n", pthread_self(), client_fd, pack);
    //printf("Columns:%s\n", columns);
    if(select_type == 1 || select_type == 2)
    {
    	int col_len = strlen(columns);
    	columns[col_len - 1] = '\n';
    	columns[col_len] = '\0';
    	robust_write(client_fd, columns, col_len);
        for(int i = 0 ; i < (int)database->row ; ++i)
        {
            ++returned_records;
            int str_len = strlen(output_list[i]);
            output_list[i][str_len - 1] = '\n';
            output_list[i][str_len] = '\0';
            robust_write(client_fd, output_list[i], str_len);
            //printf("Written %d bytes.\n", write_byte);
            //fprintf(stderr,"outputlist[%d](%d) = %s\n", i,database->row,output_list[i] );
        }
    }
    else if(select_type == 3)
    {
        int col_len = strlen(columns);
        columns[col_len - 1] = '\n';
        columns[col_len] = '\0';
        robust_write(client_fd, columns, col_len);
        for(int i = 0 ; i < (int)database->row ; ++i)
        {
            
            int str_len = strlen(output_list[i]);
            output_list[i][str_len - 1] = '\n';
            output_list[i][str_len] = '\0';
            
            if(duplicate_check(output_list, output_list[i], i) == 0)
            {
                ++returned_records;
                output_list[i][str_len] = '\0';
                robust_write(client_fd, output_list[i], str_len);
                //fprintf(stderr,"outputlist[%d](%d) = %s\n", i,database->row,output_list[i] );
            }
        }
    }
    usleep(500000);
    end_time = clock();
    elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    elapsed_time+=0.5;
    sprintf(log, "Server’s response to Client-%d is %d records, and arrived in %.1lf seconds.\n", client_no, returned_records, elapsed_time);
    robust_write(client_fd, log, strlen(log));
    robust_write(client_fd, done_message, strlen(done_message));
    for(int i = 0 ; i < (int)database->row ; ++i)
    {
        free(output_list[i]);
    }
    free(output_list);
    free(columns);
    return returned_records;   
}

void UPDATE(int client_fd, char *pack)
{
	clock_t start_time, end_time;
    double elapsed_time;
	int client_no = -1;
	char junk = 'j';
    char done_message[] = "HALT";
    char log[128];
    char **values = robust_calloc(256, sizeof(char *), &log_file);// Create outputlist.
    memset(log, 0, 128);
    for(int i = 0 ; i < 256 ; ++i)
    {
        values[i] = robust_calloc(1024, sizeof(char), &log_file);
    }
    sscanf(pack, "%d %c\n", &client_no,  &junk);
    char * value_cond = robust_calloc(1024, sizeof(char), &log_file);
    int effected_indexes[database->col];
    int condition_index;
    start_time = clock();
    int tuples = parse_update(pack, effected_indexes, 
    &condition_index, values, value_cond);
    int update_records = make_changes_on_table(effected_indexes, condition_index, values, value_cond, tuples);
    fprintf(log_file,"Thread #%lu: query completed, %d records have been updated.\n", pthread_self(), update_records);
    usleep(500000);
    
    end_time = clock();

    elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    elapsed_time+=0.5;
    sprintf(log, "Server’s response to Client-%d is %d records updated, and arrived in %.1lf seconds.\n", client_no, update_records, elapsed_time);
    robust_write(client_fd, log, strlen(log));
    robust_write(client_fd, done_message, strlen(done_message));
    /*for(int i = 0 ; i < tuples ; ++i)
    {
        printf("***%s -> %s***", database->column_names[effected_indexes[i]], values[i]);
    }
    printf("\n");
    printf("The condition values: %s -> %s\n", database->column_names[condition_index], value_cond);*/
    
    
    for(int i = 0 ; i < 256 ; ++i)
    {
        free(values[i]);
    }
    free(values);
    free(value_cond);
    
}

int duplicate_check(char **output_list, char *single_entry, int search_limit)
{
    int i;
    int duplicate = 0;
    for(i = 0 ; i < search_limit ; ++i)
    {
        if(strcmp(output_list[i], single_entry) == 0)
        {
            //printf("Comparing %s and %s\n", output_list[i], single_entry);
            duplicate = 1;
            break;
        }
    }
    return duplicate;
}

int parse_update(char *query, int *effected_indexes,int *condition_index, char **values, char *value_condition)
{
    int where_index = 0;
    int tuple_count = 0;
    char *column_name = robust_calloc(256, sizeof(char), &log_file);
    char *key_column = robust_calloc(256, sizeof(char), &log_file);
    char *key_value = robust_calloc(1024, sizeof(char), &log_file);
    char *value = robust_calloc(1024, sizeof(char), &log_file);
    int column_name_index = 0;
    int searching_column = 1;
    int value_index = 0;
    int value_list_index = 0;
    int effected_index_number = 0;
    char *buffer = robust_calloc(1024, sizeof(char), &log_file);
    int i,j;

    for(i = 0 ; i < (int)strlen(query) - 3 ; ++i)
    {
        if(query[i] == 'S' && query[i + 1] == 'E' && query[i + 2] == 'T')
        {
            break;
        }
    }
    i += 4;
    
    for(j = 0 ; j < (int)strlen(query) ; ++j)
    {
        if(query[i] == 'W' && query[i + 1] == 'H' && query[i + 2] == 'E' && query[i + 3] == 'R' && query[i + 4] == 'E')
        {
            where_index = i;
            buffer[j - 1] = '\0';
            break;
        }
        
        buffer[j] = query[i];
        i++;
    }
    //Name='Yusuf', Surname='Patoglu', Age='24, 25'
    i = 0;
    for(i = 0 ; i < (int)strlen(buffer) ; ++i)
    {
        if(buffer[i] == '=' && searching_column)
        {
            tuple_count++;
            column_name[column_name_index] = '\0';
            int found = search_column(column_name);
            if(found == -1)
            {
                fprintf(log_file, "SERVER REPORTS: NO SUCH COLUMN(%s) FOUND.SERVER EXITING.\n", column_name);
                exit(EXIT_FAILURE);
                //robust_exit("SERVER REPORTS: NO SUCH COLUMN FOUND.\n", EXIT_FAILURE);
            }
            else
            {
                effected_indexes[effected_index_number++] = found;
            }
            //printf("Column:%s=\n", column_name);
            column_name_index = 0;
            memset(column_name,0,strlen(column_name));
            searching_column = 0;
            i+=2;
        }
        if(buffer[i] == '\'' && searching_column == 0)
        {
            value[value_index] = '\0';
            strncpy(values[value_list_index++], value, strlen(value));
            //printf("Value:%s=\n", value);
            value_index = 0;
            memset(value,0,strlen(value));
            searching_column = 1;
            i+=3; 
        }
        if(searching_column)
            column_name[column_name_index++] = buffer[i];
        else
        {
            value[value_index++] = buffer[i];
        }
    }
    column_name_index = 0;
    value_index = 0;
    int last_value_index;
    for(i = where_index + 6 ; i < (int)strlen(query) ; ++i)
    {
        if(query[i] == '=')
        {
            key_column[column_name_index] = '\0';
            last_value_index = i;
            break;
        }
        key_column[column_name_index++] = query[i];
    }

    for(i = last_value_index + 2 ; i < (int)strlen(query) ; ++i)
    {
        key_value[value_index++] = query[i];
    }
    key_value[value_index - 1] = '\0';
    //printf("\n");

    //printf("Total %d tuples are detected:\n", tuple_count);
    /*for(i = 0 ; i < tuple_count ; ++i)
    {
        printf("%s -> %s\n", database->column_names[i], values[i]);
    }*/
    //printf("Changed column condition and value: %s,,,,,%s-\n", key_column, key_value);
    strncpy(value_condition, key_value, strlen(key_value));
    *condition_index = search_column(key_column);
    if(*condition_index == -1)
    {
        fprintf(log_file,"SERVER REPORTS: NO SUCH COLUMN(%s) FOUND.\n", key_column);
        exit(EXIT_FAILURE);
    }
    free(buffer);
    free(column_name);
    free(value);
    free(key_column);
    free(key_value);
    return tuple_count;
}
int parse_select(char *query, int *column_indexes)
{
    
    char *buffer = robust_calloc(1024, sizeof(char), &log_file);
    int select_type = -1; // 1 for SELECT ALL, 2 for SELECT WHERE, 3 for SELECT DISTINCT 
    int i,j;
    for(i = 0 ; i < (int)database->col ; ++i)
    {
            column_indexes[i] = 0;
    }
    if(strstr(query, "SELECT * FROM") != NULL) // Simple select 
    {
        for(i = 0 ; i < (int)database->col ; ++i)
        {
            column_indexes[i] = 1;
        }
        //printf("This is a simple select command.\n");
        select_type = 1;
    }
    else if(strstr(query, "SELECT DISTINCT") != NULL)
    {
        for(i = 0 ; i < (int)strlen(query) - 2 ; ++i)
        {
            if((query[i] == 'N') && (query[i + 1] == 'C') && (query[i + 2] == 'T'))
            {
                break;
            }
        }
        i += 3;

        for(j = 0 ; j < (int)strlen(query) ; ++j)
        {
            if((query[i] == 'F') && (query[i+1] == 'R') && (query[i+2] == 'O'))
            {
                buffer[j - 1] = '\0';
                break;
                
            }
            buffer[j] = query[i];
            i++;
        }
        char *rest = NULL;
        char *token;

        for (token = strtok_r(buffer, ", ", &rest);
            token != NULL;
            token = strtok_r(NULL, ", ", &rest)) {   
            //printf("token:%s-\n", token);
            int i = search_column(token);
            if(i == -1)
            {
                fprintf(log_file, "No such column as %s\n", token);
                fprintf(log_file, "SERVER REPORTS: NO SUCH COLUMN DETECTED. SHUTTING DOWN\n\n");
                exit(EXIT_FAILURE);
                //robust_exit("SERVER REPORTS: NO SUCH COLUMN DETECTED. SHUTTING DOWN\n", EXIT_FAILURE);
            }
            column_indexes[i] = 1; 
        }
        select_type = 3;
    }
    else if(strstr(query, "SELECT") != NULL) //Simple with multiple columns
    {
        for(i = 0 ; i < (int)strlen(query) - 1 ; ++i)
        {
            if((query[i] == 'C') && (query[i + 1] == 'T'))
            {
                break;
            }
        }
        i += 3;
        for(j = 0 ; j < (int)strlen(query) ; ++j)
        {
            if((query[i] == 'F') && (query[i+1] == 'R') && (query[i+2] == 'O'))
            {
                buffer[j - 1] = '\0';
                break;
                
            }
            buffer[j] = query[i];
            i++;
        }
        char *rest = NULL;
        char *token;

        for (token = strtok_r(buffer, ", ", &rest);
            token != NULL;
            token = strtok_r(NULL, ", ", &rest)) {   
            //printf("token:%s-\n", token);
            int i = search_column(token);
            if(i == -1)
            {
                fprintf(log_file, "No such column as %s\n", token);
                fprintf(log_file, "SERVER REPORTS: NO SUCH COLUMN DETECTED. SHUTTING DOWN\n\n");
                exit(EXIT_FAILURE);
            }
            column_indexes[i] = 1; 
        }
    
        //printf("Buffer is %s-\n", buffer);
        select_type = 2;
    }
    
    free(buffer);
    return select_type;
}

int search_column(char *single_cell)
{
    int i;
    for(i = 0 ; i < (int)database->col ; ++i)
    {
        if(strcmp(database->column_names[i], single_cell) == 0)
        {
            return i;
        }
    }
    return -1;
}
void combine_columns(int *column_indexes, char *output)
{
    int i;
    char *target = output;
    for(i = 0 ; i < (int)database -> col ; ++i)
    {

        if(column_indexes[i] == 1)
        {
        	target += sprintf(target, "%25s   ", database->column_names[i]);
            //strncat(output, database->column_names[i], strlen(database->column_names[i]));
            //strncat(output, seperator, strlen(seperator));
        }
    }
    //printf("Output is: %s\n", output);
}
void combine_entries(int *column_indexes, char **output_list)
{
    int i,j;
    for(i = 0 ; i < (int)database -> row ; ++i)
    {
    	char *target = output_list[i];
        for(j = 0 ; j < (int)database -> col ; ++j)
        {
            if(column_indexes[j] == 1)
            {
            	
            	target += sprintf(target, "%25s   ", database->table[i][j]);
                //strncat(output_list[i], database->table[i][j], strlen(database->table[i][j]));
                //strncat(output_list[i], seperator, strlen(seperator));
            }
            
            //printf("%s\n", output_list[i]);
        }
        
    }

    //print_buffer_list(output_list);
    //printf("Done\n");
    
}


    

int make_changes_on_table(int *effected_indexes ,int condition_index, char **values,char *value_condition, int changed_column_count)
{
    int effected_records = 0;
    int i,j;
    int all_same = 1;
    for(i = 0 ; i < (int)database->row ; ++i)
    {
        if(strcmp(database->table[i][condition_index], value_condition) == 0)
        {
            effected_records++;
            for(j = 0 ; j < changed_column_count ; ++j)
            {
                if(strcmp(values[j], database->table[i][effected_indexes[j]]) == 0)
                {
                    all_same = 1;
                }
                else
                {
                    memset(database->table[i][effected_indexes[j]],0,strlen(database->table[i][effected_indexes[j]]));
                    strncpy(database->table[i][effected_indexes[j]], values[j], strlen(values[j]));
                    all_same = 0;
                }
                //printf("Values[%d] = %s, %s\n", j, values[j], database->table[i][effected_indexes[j]]);
            }
            if(all_same == 1)
            {
                effected_records--;
            } 

        }

    }
    return effected_records;
}
//IF ITS READ OPERATION RETURNS 1
//IF ITS WRITE OPERATION RETURNS 2
//ELSE PRINTS AND EXITS ERROR
int command_classifier(char *query)
{
    int command_type;
    if(strstr(query, "UPDATE") != NULL)
    {
        command_type = 2;
    }
    else if(strstr(query, "SELECT") != NULL)
    {
        command_type = 1;
    }
    else
    {
        fprintf(log_file, "SSERVER DETECTED AN UNDEFINED COMMAND. EXITING!\n");
        exit(EXIT_FAILURE);
        //robust_exit("SERVER DETECTED AN UNDEFINED COMMAND. EXITING!\n", EXIT_FAILURE);
    }
    return command_type;
}
void open_log_file(char *file_name)
{
    log_file = fopen(file_name, "w");
    if(log_file == NULL)
    {
        robust_exit("Couldn't create log file. Server exiting.\n", EXIT_FAILURE);
    }

}

void close_log_file()
{
    if(fclose(log_file) != 0)
    {
        fprintf(log_file, "Couldn't close the logfile. Server exiting.\n");
    }
}
//./server -p 1500 -o logfile -l 3 -d backupyusuf.csv
//ps -C server -o "pid ppid pgid sid tty stat command"
//valgrind --log-file="filename" -v --leak-check=full --show-leak-kinds=all --trace-children=yes ./server -p 1500 -o logfile -l 3 -d backupyusuf.csv
//valgrind --log-file="filename" -v --leak-check=full --show-leak-kinds=all --trace-children=yes ./server -p 1500 -o logfile -l 3 -d backupyusuf.csv

//https://bilmuh.gtu.edu.tr/moodle/pluginfile.php/23780/mod_resource/content/1/Week05.pdf
void becomeDaemon(int abstract_fd)
{
    int maxfd, fd;
    switch(fork())
    {
        case -1:
            robust_exit("Fork returned an error from becomeDaemon(1)\n", EXIT_FAILURE);
            break;
        case 0:
            break;
        default:
            _exit(EXIT_SUCCESS);
    }
    
    if(setsid() == -1)
    {
        robust_exit("Setsid returned an error.\n", EXIT_FAILURE);
    }

    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    switch(fork())
    {
        case -1:
            robust_exit("Fork returned an error from becomeDaemon(2)\n", EXIT_FAILURE);
            break;
        case 0:
            break;
        default:
            _exit(EXIT_SUCCESS);
    }
    
    umask(0); 
    
    //chdir("/");

    maxfd = sysconf(_SC_OPEN_MAX);
    if (maxfd == -1)
    {
        //maxfd = BD_MAX_CLOSE;
    }
    
    /* so take a guess */
    for (fd = 0; fd < maxfd; fd++)
    {
        if(fd != abstract_fd)
        {
            close(fd);
        }
        
    }
}
void pid_lock()
{
    prevent_fp = fopen("/var/run/daemonpids_yusuf","w");
    if(prevent_fp == NULL)
    {
        fprintf(stderr, "Can't open the file. Exiting the program.\n");
        exit(EXIT_FAILURE);
    }
    int result = flock(fileno(prevent_fp), LOCK_EX | LOCK_NB);
    if(result == -1)
    {
        fprintf(stderr,"Daemon server can only executed once. If the other daemon terminates the lock will be released.");
        fclose(prevent_fp);
        exit(EXIT_FAILURE);
    }
}
void pid_cleanup()
{
    int ret;
    if(fclose(prevent_fp) != 0)
    {
        fprintf(stderr, "Couldn't close the logfile. Server exiting.\n");
    }
    ret = remove("/var/run/daemonpids_yusuf");

    if(ret != 0)
    {
        fprintf(log_file,"Couldn't delete the temp file.\n");
    } 
    
}