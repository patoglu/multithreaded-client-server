#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "helper.h"
#include "robust_io.h"
#include "alloc.h"
#include <signal.h>
#include <unistd.h>
#define PACKLEN 1024
char newline_detector(char *received_packet);
void read_queries(char *filename);
void handler();
int main(int argc, char *argv[])
{
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = handler;
    if (sigaction(SIGINT, &sa, NULL) == -1)
        robust_exit("Sig action returned an error!\n", EXIT_FAILURE);

    int total_queries_executed = 0;
    int read_bytes;
    int i;
    char *pack = robust_calloc_q(255, sizeof(char));
    int file_client_no = -1;
    char junk = 'j';
    char received_pack[PACKLEN];
    int client_id;
    char ip_addr[MAXFILELEN];
    char query_file[MAXFILELEN];
    int port_no;
    size_t line_len = 255;

    parse_args_c(argc, argv, &client_id, ip_addr, &port_no, query_file);
    FILE *fptr = fopen(query_file,"r");
    if(fptr == NULL)
    {
        fprintf(stderr, "Can't open the file. Exiting the program.");
        fclose(fptr);
        exit(EXIT_FAILURE);
    }

    /**
    Steps are verified from this source: https://www.gta.ufrj.br/ensino/eel878/sockets/index.html
    **/
    struct sockaddr_in host_address;
    int socket_descriptor;
    

    if((socket_descriptor = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        robust_exit("Socket function returned an error.Quitting the program.", EXIT_FAILURE);
    }
    bzero(&host_address, sizeof(struct sockaddr_in));

    host_address.sin_family = AF_INET;
    host_address.sin_addr.s_addr = inet_addr(ip_addr);
    host_address.sin_port = htons(port_no);
    if(connect(socket_descriptor, (struct sockaddr *)&host_address, sizeof(host_address)) != 0)
        robust_exit("Connect function returned an error.Quitting the program.", EXIT_FAILURE);
    
    printf("Client-%d connecting to %s:%d\n", client_id, ip_addr, port_no);
    

    while(getline(&pack,&line_len,fptr) != -1)
    {
        
        pack[strcspn(pack, "\n")] = 0;
        sscanf(pack, "%d %c", &file_client_no, &junk);
        if(file_client_no == client_id)
        {
            total_queries_executed++;
            printf("Client-%d connected and sending query '%s'\n", client_id, pack);
            robust_write(socket_descriptor, pack, strlen(pack) + 1);
            while ((read_bytes = robust_read(socket_descriptor, received_pack, PACKLEN)) > 0)
            {
                received_pack[read_bytes] = '\0';
                if(strcmp(received_pack, "HALT") == 0)
                {
                    memset(received_pack,0,strlen(received_pack));
                    
                    break;
                }
                else if(strstr(received_pack, "HALT") != NULL)
                {
                    received_pack[strlen(received_pack) - 5] = '\0';
                    printf("%s\n", received_pack);
                    memset(received_pack,0,strlen(received_pack));
                    break;
                }
                else if(newline_detector(received_pack) )
                {
                    received_pack[strlen(received_pack)] = '\0';
                    printf("%s", received_pack);
                }
                else
                {
                    received_pack[strlen(received_pack)] = '\0';
                    printf("%s\n", received_pack);
                }
                memset(received_pack,0,strlen(received_pack));
            }
        }
        
        
    }
    printf("A total of %d queries were executed, client %d is terminating.\n",total_queries_executed, client_id);
    free(pack);
    fclose(fptr);
    return 0;
}

char newline_detector(char *received_packet)
{
    int detected = 0;
    int i;
    for(i = 0 ; i < strlen(received_packet) ; ++i)
    {
        if(received_packet[i] == '\n')
        {
            detected = 1;
            break;
        }
    }
    return detected;
}


void read_queries(char *filename)
{
    FILE *fptr = fopen(filename,"r");

    if(fptr == NULL)
    {
        fprintf(stderr, "Can't open the file. Exiting the program.");
        fclose(fptr);
        exit(EXIT_FAILURE);
    }
}

void debug_ascii(char *s)
{
    printf("Start.\n");
    int i;
    for(i = 0 ; i < strlen(s) ; ++i)
    {
        printf("-%d-", s[i]);
    }
     printf("End.\n");
}
void handler()
{
    robust_write(STDOUT_FILENO,"CLIENT SHOULDN'T BE INTERRUPTED. OTHERWISE THE ON-GOING CONNECTION WILL BROKE AND SERVER WILL RECEIVE SIGPIPE. EVENTUALLY CRASH.",
        sizeof("CLIENT SHOULDN'T BE INTERRUPTED. OTHERWISE THE ON-GOING CONNECTION WILL BROKE AND SERVER WILL RECEIVE SIGPIPE. EVENTUALLY CRASH."));
}