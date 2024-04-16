#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <mysql/mysql.h>
#include <list>
#include <iostream>
#include <string>
using namespace std;
#include "../Lock/Lockers.h"

class Connection_pool
{
public:
    Connection_pool();
    ~Connection_pool();
    void init(string url, string User, string PassWord, string DataBaseName, int Port, unsigned int MaxConn);
    void DestroyPool();

    MYSQL *GetConnection();
    bool ReleaseConnection(MYSQL *conn);
    int GetFreeConn();

    // Singleton Pattern
    static Connection_pool *GetInstance();

private:
    unsigned int MaxConn;
    unsigned int CurConn;
    unsigned int FreeConn;

private:
    Locker lock;
    Sem reserve;

    list<MYSQL *> connList;

private:
    string url;
    string Port;
    string User;
    string PassWord;
    string DatabaseName;
};

class ConnectionRAII
{
public:
    ConnectionRAII(MYSQL **SQL, Connection_pool *connPool);
    ~ConnectionRAII();

private:
    MYSQL *conRAII;
    Connection_pool *poolRAII;
};

#endif