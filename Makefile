server: main.cpp ./Threadpool/Threadpool.h ./Http/Http_conn.cpp ./Http/Http_conn.h ./Lock/Lockers.h ./Log/Log.cpp ./Log/Log.h ./Log/Block_queue.h ./CGIMysql/Sql_connection_pool.cpp ./CGIMysql/Sql_connection_pool.h
	g++ -g -o server main.cpp ./Http/Http_conn.cpp ./Log/Log.cpp ./CGIMysql/Sql_connection_pool.cpp -lpthread -lmysqlclient

clean:
	rm  -r server
