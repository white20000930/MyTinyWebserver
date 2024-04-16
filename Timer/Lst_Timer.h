#ifndef LST_TIMER
#define LST_TIMER

#include <netinet/in.h>
#include <stdio.h>
#include <time.h>

class Util_timer;
struct Client_data
{
    sockaddr_in address; // user address, ip and port
    int sockfd;
    Util_timer *timer;
};

class Util_timer
{
public:
    Util_timer() : prev(NULL), next(NULL) {}

public:
    time_t expire; // expire time
    void (*cb_func)(Client_data *);
    Client_data *user_data;
    Util_timer *prev;
    Util_timer *next;
};

class Sort_timer_lst
{
public:
    Sort_timer_lst() : head(NULL), tail(NULL) {}
    ~Sort_timer_lst();
    void add_timer(Util_timer *timer);
    void adjust_timer(Util_timer *timer);
    void del_timer(Util_timer *timer);
    void tick();

private:
    void add_timer(Util_timer *timer, Util_timer *lst_head);

private:
    Util_timer *head;
    Util_timer *tail;
};
Sort_timer_lst::~Sort_timer_lst()
{
    Util_timer *tmp = head;
    while (tmp)
    {
        this->head = tmp->next;
        delete tmp;
        tmp = this->head;
    }
}

void Sort_timer_lst::add_timer(Util_timer *timer)
{
    if (!timer)
    {
        return;
    }
    if (!this->head)
    {
        this->head = this->tail = timer;
        return;
    }
    if (timer->expire < this->head->expire)
    {
        timer->next = this->head;
        this->head->prev = timer; // expire from small to large
        this->head = timer;
        return;
    }
    this->add_timer(timer, this->head);
}

void Sort_timer_lst::adjust_timer(Util_timer *timer)
{
    if (!timer)
    {
        return;
    }
    Util_timer *tmp = timer->next;
    if (!tmp || (timer->expire < tmp->expire))
    {
        return;
    }
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head);
    }
    else
    {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}

void Sort_timer_lst::del_timer(Util_timer *timer)
{
    if (!timer)
    {
        return;
    }
    if ((timer == head) && (timer == tail))
    {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    if (timer == tail)
    {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

void Sort_timer_lst::tick()
{
    if (!head)
    {
        return;
    }
    // printf( "timer tick\n" );
    // LOG_INFO("%s", "timer tick");
    // Log::get_instance()->flush();
    time_t cur = time(NULL);
    Util_timer *tmp = head;
    while (tmp)
    {
        if (cur < tmp->expire)
        {
            break;
        }
        tmp->cb_func(tmp->user_data);
        head = tmp->next;
        if (head)
        {
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}

void Sort_timer_lst::add_timer(Util_timer *timer, Util_timer *lst_head)
{
    Util_timer *prev = lst_head;
    Util_timer *tmp = prev->next;
    while (tmp)
    {
        if (timer->expire < tmp->expire)
        {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    if (!tmp)
    {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}

#endif
