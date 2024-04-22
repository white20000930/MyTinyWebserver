#include "Sql_connection_pool.h"

Connection_pool::Connection_pool()
{
    this->CurConn = 0;
    this->FreeConn = 0;
}

Connection_pool::~Connection_pool()
{
    this->DestroyPool();
}

// Get a connection from the connection pool
MYSQL *Connection_pool::GetConnection()
{
    MYSQL *con = NULL;
    if (0 == connList.size())
    {
        return NULL;
    }

    this->reserve.wait();
    this->lock.lock();

    con = connList.front();
    this->connList.pop_front();

    --this->FreeConn;
    ++this->CurConn;

    this->lock.unlock();
    return con;
}

bool Connection_pool::ReleaseConnection(MYSQL *conn)
{
    if (NULL == conn)
        return false;

    lock.lock();

    connList.push_back(conn);
    ++FreeConn;
    --CurConn;

    lock.unlock();

    reserve.post();
    return true;
}

int Connection_pool::GetFreeConn()
{
    return this->FreeConn;
}

void Connection_pool::DestroyPool()
{
    lock.lock();
    if (connList.size() > 0)
    {
        list<MYSQL *>::iterator it;
        for (it = connList.begin(); it != connList.end(); ++it)
        {
            MYSQL *con = *it;
            mysql_close(con);
        }
        CurConn = 0;
        FreeConn = 0;
        connList.clear();

        lock.unlock();
    }

    lock.unlock();
}

Connection_pool *Connection_pool::GetInstance()
{
    static Connection_pool connPool;
    return &connPool;
}

void Connection_pool::init(string url, string User, string PassWord, string DataBaseName, int Port, unsigned int MaxConn)
{
    this->url = url;
    this->Port = Port;
    this->User = User;
    this->PassWord = PassWord;
    this->DatabaseName = DataBaseName;

    this->lock.lock();
    for (int i = 0; i < MaxConn; i++)
    {
        MYSQL *con = NULL;

        con = mysql_init(con);

        if (con == NULL)
        {
            cout << "mysql_init error" << endl;
            exit(1);
        }

        con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DataBaseName.c_str(), Port, NULL, 0);

        if (con == NULL)
        {
            cout << "Error: " << mysql_error(con);
            exit(1);
        }

        this->connList.push_back(con);

        ++this->FreeConn;
    }

    this->reserve = Sem(this->FreeConn);

    this->MaxConn = this->FreeConn;

    this->lock.unlock();
}

ConnectionRAII::ConnectionRAII(MYSQL **SQL, Connection_pool *connPool)
{
    *SQL = connPool->GetConnection();
    this->conRAII = *SQL;
    this->poolRAII = connPool;
}

ConnectionRAII::~ConnectionRAII()
{
    this->poolRAII->ReleaseConnection(this->conRAII);
}
