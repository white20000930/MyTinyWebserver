#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <mysql/mysql.h>
class Connection_pool
{
public:
    Connection_pool();
    ~Connection_pool();

    MYSQL *GetConnection();
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