#include <stdlib.h>
#include "alloc.h"
#include "queue.h"
#include "robust_io.h"
/**
 * Implemented Circular Queue :(Page 85)
 *  http://www.nitjsr.ac.in/course_assignment/CS01CS1302A%20Book%20Fundamentals%20of%20Data%20Structure%20(1982)%20by%20Ellis%20Horowitz%20and%20Sartaj%20Sahni.pdf
 **/

Client_Queue* allocate_queue()
{
    Client_Queue *temp = robust_calloc_q(sizeof(Client_Queue), 1);
    temp->queue = robust_calloc_q(8192, sizeof(int));
    temp->capacity = 8192;
    temp->front = temp->size = 0;
    temp->back = temp->capacity - 1;
    return temp;
}
void enqueue_client(Client_Queue ** _queue,  int client_fd)
{
    int offset;
    int back;
    if (full(*_queue) == 1)
    {
        (*_queue)->queue = realloc((*_queue)->queue, sizeof(int) * ((*_queue)->capacity * 2));
        if((*_queue)->queue == NULL)
        {
            perror("realloc");
            robust_exit("REALLOC ENCOUNTERED ERROR. EXITING SERVER.", EXIT_FAILURE);
        }
        (*_queue)->capacity *= 2;
        printf("Hope realloc");
    }
    offset = ((*_queue)->back + 1) % (*_queue)->capacity;
    (*_queue)->back = offset;
    back = (*_queue)->back;
    (*_queue)->queue[back] = client_fd;
    (*_queue)->size++;
}

int front(Client_Queue * _queue)
{   
    return _queue->queue[_queue->front];
}

int back(Client_Queue * _queue)
{
    return _queue->queue[_queue->back];
}

int dequeue_client(Client_Queue *_queue)
{
    
    if (empty(_queue) == 1)
    {
        robust_exit("Can't dequeue an empty queue.\n", EXIT_FAILURE);
    }
        
    int item = _queue->queue[_queue->front];
    _queue->front = (_queue->front + 1) % _queue->capacity;
    _queue->size--;
    return item;


}

int full(Client_Queue * _queue)
{
    if(_queue->size == _queue->capacity)
    {
        return 1;
    }
    else
    {
        return 0;
    }

}
int empty(Client_Queue * _queue)
{

    if(_queue->size == 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }

}