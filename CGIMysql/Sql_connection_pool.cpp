#include "Sql_connection_pool.h"

Connection_pool::Connection_pool()
{
}

Connection_pool::~Connection_pool()
{
}

MYSQL *Connection_pool::GetConnection()
{
    return nullptr;
}

ConnectionRAII::ConnectionRAII(MYSQL **SQL, Connection_pool *connPool)
{
    *SQL = connPool->GetConnection();

    this->conRAII = *SQL;
    this->poolRAII = connPool;
}

ConnectionRAII::~ConnectionRAII()
{
}
