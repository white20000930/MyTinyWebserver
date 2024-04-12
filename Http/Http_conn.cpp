#include "Http_conn.h"
#include <fcntl.h>
#include <unistd.h>

//  #define edge_TRIGGERED //边缘触发 非阻塞
#define level_TRIGGERED // 水平触发 阻塞

int addfd(int epollfd, int fd, bool one_shot)
{
    epoll_event event;
    event.data.fd = fd;
#ifdef edge_TRIGGERED
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
#endif

#ifdef level_TRIGGERED
    event.events = EPOLLIN | EPOLLRDHUP;
#endif

    if (one_shot)
    {
        event.events |= EPOLLONESHOT; // The epoll instance will trigger the event only once
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
    return 0;
}

void modfd(int epollfd, int fd, int ev)
{
    epoll_event event;
    event.data.fd = fd;
#ifdef edge_TRIGGERED
    event.events = ev | EPOLLET | EPOLLRDHUP;
#endif

#ifdef level_TRIGGERED
    event.events = ev | EPOLLRDHUP;
#endif

    event.events |= EPOLLONESHOT; // The epoll instance will trigger the event only once

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

int removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
    return 0;
}

int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

/*
 * about class Http_conn
 */

int Http_conn::m_user_count = 0;
int Http_conn::m_epollfd = -1;

void Http_conn::init(int sockfd, const sockaddr_in &addr)
{
    this->m_sockfd = sockfd;
    this->m_address = addr;

    addfd(m_epollfd, sockfd, true);
    m_user_count++; // m_user_count is a static variable
    init();
}

void Http_conn::close_conn(bool real_close)
{
    if (real_close && (this->m_sockfd != -1))
    {
        removefd(m_epollfd, this->m_sockfd);
        this->m_sockfd = -1;
        m_user_count--;
    }
}

void Http_conn::process()
{
    HTTP_CODE read_ret = process_read(); // Response status
    if (read_ret == NO_REQUEST)
    {
        modfd(m_epollfd, this->m_sockfd, EPOLLIN);
        return;
    }
    bool write_ret = process_write(read_ret);
    if (!write_ret)
    {
        close_conn();
    }
    modfd(m_epollfd, this->m_sockfd, EPOLLOUT);
}

void Http_conn::init()
{
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

HTTP_CODE Http_conn::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;

    while ((this->m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))
    {
    }

    return NO_REQUEST;
}

bool Http_conn::process_write(HTTP_CODE ret)
{
    return false;
}

LINE_STATUS Http_conn::parse_line()
{
    char temp;
    for (; this->m_checked_idx < this->m_read_idx; ++this->m_checked_idx)
    {
        temp = this->m_read_buf[this->m_checked_idx]; // m_read_buf[] is populated by read_once() already
        if (temp == '\r')                             // if temp = "enter"
        {
            if ((this->m_checked_idx + 1) == this->m_read_idx)
            { // if the next one is the last one, usually '\n'
                return LINE_OPEN;
            }
            else if (this->m_read_buf[this->m_checked_idx + 1] == '\n')
            {
            }
        }
    }
    return LINE_STATUS();
}
