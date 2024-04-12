server: main.cpp ./Lock/Lockers.h ./Threadpool/Threadpool.h ./CGIMysql/Sql_connection_pool.cpp ./CGIMysql/Sql_connection_pool.h
	g++ -o server main.cpp ./Lock/Lockers.h ./Threadpool/Threadpool.h ./CGIMysql/Sql_connection_pool.cpp ./CGIMysql/Sql_connection_pool.h

clean:
	rm -r server