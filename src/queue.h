
typedef struct Client_Queue
{
    int back; //define them in in case of any negative values.
    int front;
    int capacity;
    int size;
    int* queue;
} Client_Queue;

Client_Queue* allocate_queue();
int full(Client_Queue * _queue);
int empty(Client_Queue * _queue);
void enqueue_client(Client_Queue ** _queue,  int client_id);
int dequeue_client(Client_Queue *_queue);
int front(Client_Queue * _queue);
int back(Client_Queue * _queue);
void free_queue(Client_Queue * _queue);
void reallocate(Client_Queue ** _queue);
