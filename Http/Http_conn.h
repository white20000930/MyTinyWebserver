#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <map>
#include <sys/mman.h>
#include <sys/uio.h>
using namespace std;
#include "../CGIMysql/Sql_connection_pool.h"
#include "../Lock/Lockers.h"
//  #define edge_TRIGGERED //边缘触发 非阻塞
#define level_TRIGGERED // 水平触发 阻塞

const char *doc_root = "/home/xgwhite/Templates/MyTinyWebServer/Root";

// some HTTP status information
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

static const int FILENAME_LEN = 200;
static const int READ_BUFFER_SIZE = 2048;
static const int WRITE_BUFFER_SIZE = 1024;

// a map to store the username and password
map<string, string> users_map;
Locker m_lock;

/**
 * @brief HTTP请求方法枚举
 */
enum METHOD
{
    GET = 0, // GET请求
    POST,    // POST请求
    HEAD,    // HEAD请求
    PUT,     // PUT请求
    DELETE,  // DELETE请求
    TRACE,   // TRACE请求
    OPTIONS, // OPTIONS请求
    CONNECT, // CONNECT请求
    PATH     // PATH请求
};

/**
 * @brief HTTP解析状态枚举
 */
enum CHECK_STATE
{
    CHECK_STATE_REQUESTLINE = 0, // 解析请求行状态
    CHECK_STATE_HEADER,          // 解析请求头状态
    CHECK_STATE_CONTENT          // 解析请求内容状态
};

/**
 * @brief HTTP响应状态枚举
 */
enum HTTP_CODE
{
    NO_REQUEST,        // 无请求
    GET_REQUEST,       // GET请求
    BAD_REQUEST,       // 错误请求
    NO_RESOURCE,       // 资源不存在
    FORBIDDEN_REQUEST, // 禁止访问
    FILE_REQUEST,      // 文件请求
    INTERNAL_ERROR,    // 内部错误
    CLOSED_CONNECTION  // 连接关闭
};

/**
 * @brief HTTP请求行状态枚举
 */
enum LINE_STATUS
{
    LINE_OK = 0, // 行解析成功
    LINE_BAD,    // 行解析失败
    LINE_OPEN    // 行解析未完成
};

class Http_conn
{
public:
    Http_conn(){};
    ~Http_conn(){};

public:
    void init(int sockfd, const sockaddr_in &addr);
    void close_conn(bool real_close = true);

    void process();

    bool read_once();
    bool write();
    sockaddr_in *get_address();
    void initmysql_result(Connection_pool *connPool);

private:
    void init();

    HTTP_CODE process_read();
    bool process_write(HTTP_CODE ret);

    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);

    HTTP_CODE do_request();

    char *get_line();

    LINE_STATUS parse_line();

    void unmap();

    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    static int m_epollfd;
    static int m_user_count;
    MYSQL *mysql;

private:
    int m_sockfd;
    sockaddr_in m_address;

    char m_read_buf[READ_BUFFER_SIZE];
    int m_read_idx; // m_read_idx --> the first empty position in m_read_buf[]

    int m_checked_idx;
    int m_start_line;

    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx;

    CHECK_STATE m_check_state;
    METHOD m_method;
    char m_real_file[FILENAME_LEN];
    char *m_url;
    char *m_version;
    char *m_host;
    int m_content_length;
    bool m_linger;
    char *m_file_address;
    struct stat m_file_stat; // stat contains various information about a file or directory
    struct iovec m_iv[2];
    int m_iv_count;
    int cgi;        // 是否启用的POST
    char *m_string; // 存储请求头数据
    int bytes_to_send;
    int bytes_have_send;
};

#endif