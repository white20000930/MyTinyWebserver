#include <iostream>
#include "./Threadpool/Threadpool.h"

// 这三个函数在http_conn.cpp中定义，改变链接属性

// add fd to epollfd, set EPOLLONESHOT if one_shot == true
extern int addfd(int epollfd, int fd, bool one_shot);

// modify fd
extern void modfd(int epollfd, int fd, int ev);

extern int removefd(int epollfd, int fd);

// set nonblocking
extern int setnonblocking(int fd);

int main(int argc, char *argv[])
{
    std::cout << "Test main~" << std::endl;

    return 0;
}
