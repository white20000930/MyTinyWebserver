#include "Http_conn.h"

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

bool Http_conn::read_once()
{
    // Job: read data from socket and store it in m_read_buf[]
    if (this->m_read_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }

    int bytes_read = 0;

#ifdef level_TRIGGERED
    // receive
    bytes_read = recv(m_epollfd, this->m_read_buf + this->m_read_idx, READ_BUFFER_SIZE - this->m_read_idx, 0);

    if (bytes_read <= 0)
    {
        return false;
    }

    this->m_read_idx += bytes_read; // m_read_idx --> the first empty position in m_read_buf[]

    return true;

#endif

#ifdef edge_TRIGGERED
    while (true)
    {
        bytes_read = recv(m_epollfd, this->m_read_buf + this->m_read_idx, READ_BUFFER_SIZE - this->m_read_idx, 0);
        if (bytes_read == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;
            }
            return false;
        }
        else if (bytes_read == 0)
        {
            return false;
        }
        m_read_idx += bytes_read;
    }
    return true;
#endif
}

bool Http_conn::write()
{
    int temp = 0;

    if (this->bytes_to_send == 0)
    {
        modfd(m_epollfd, this->m_sockfd, EPOLLIN);
        init();
        return true;
    }

    while (1)
    {
        temp = writev(this->m_sockfd, this->m_iv, this->m_iv_count); // if error ,temp < 0 , error code is stored in errno

        if (temp < 0)
        {
            if (errno == EAGAIN) // writev
            {
                modfd(m_epollfd, this->m_sockfd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }

        this->bytes_have_send += temp;
        this->bytes_to_send -= temp;

        if (this->bytes_have_send >= this->m_iv[0].iov_len)
        {
            this->m_iv[0].iov_len = 0;
            this->m_iv[1].iov_base = this->m_file_address + (this->bytes_have_send - this->m_write_idx);
            this->m_iv[1].iov_len = this->bytes_to_send;
        }
        else
        {
            this->m_iv[0].iov_base = this->m_write_buf + this->bytes_have_send;
            this->m_iv[0].iov_len = this->m_write_idx - this->bytes_have_send;
        }

        if (this->bytes_to_send <= 0)
        {
            unmap();
            modfd(m_epollfd, this->m_sockfd, EPOLLIN);

            if (this->m_linger)
            {
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}

sockaddr_in *Http_conn::get_address()
{
    return &(this->m_address);
}

void Http_conn::initmysql_result(Connection_pool *connPool)
{
    
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

    while ((this->m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = this->parse_line()) == LINE_OK))
    {
        text = this->get_line(); // get the first line that has not been checked
        this->m_start_line = this->m_checked_idx;

        // LOG_INFO("%s", text);
        // Log::get_instance()->flush();

        switch (this->m_check_state)
        {
        case CHECK_STATE_REQUESTLINE:
        {
            ret = this->parse_request_line(text);
            if (ret == BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            break;
        }
        case CHECK_STATE_HEADER:
        {
            ret = this->parse_headers(text);
            if (ret == BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            else if (ret == GET_REQUEST)
            {
                return this->do_request();
            }
            break;
        }
        case CHECK_STATE_CONTENT:
        {
            ret = this->parse_content(text);
            if (ret == GET_REQUEST)
            {
                return this->do_request();
            }
            line_status = LINE_OPEN;
            break;
        }
        default:
        {
            return INTERNAL_ERROR;
        }
        }
    }

    return NO_REQUEST;
}

bool Http_conn::process_write(HTTP_CODE ret)
{
    switch (ret)
    {
    case INTERNAL_ERROR:
    {
        this->add_status_line(500, error_500_title);
        this->add_headers(strlen(error_500_form));
        if (!this->add_content(error_500_form))
        {
            return false;
        }
        break;
    }
    case BAD_REQUEST:
    {
        this->add_status_line(400, error_400_title);
        this->add_headers(strlen(error_400_form));
        if (!this->add_content(error_400_form))
        {
            return false;
        }
        break;
    }
    case FORBIDDEN_REQUEST:
    {
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form))
            return false;
        break;
    }
    case FILE_REQUEST:
    {
        add_status_line(200, ok_200_title);
        if (this->m_file_stat.st_size != 0)
        {
            add_headers(this->m_file_stat.st_size);
            this->m_iv[0].iov_base = this->m_write_buf;
            this->m_iv[0].iov_len = this->m_write_idx;
            this->m_iv[1].iov_base = this->m_file_address;
            this->m_iv[1].iov_len = this->m_file_stat.st_size;
            this->m_iv_count = 2;
            this->bytes_to_send = this->m_write_idx + this->m_file_stat.st_size;
            return true;
        }
        else
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
        }
    }
    default:
        return false;
    }
    this->m_iv[0].iov_base = this->m_write_buf;
    this->m_iv[0].iov_len = this->m_write_idx;
    this->m_iv_count = 1;
    this->bytes_to_send = this->m_write_idx;
    return true;
}

HTTP_CODE Http_conn::parse_request_line(char *text)
{
    // Job: obtain the request method, URL and HTTP version

    // 1. the request method
    this->m_url = strpbrk(text, " \t"); //\t is tab
    if (!this->m_url)
    {
        return BAD_REQUEST;
    }

    *this->m_url++ = '\0'; // this->m_url is at the position after "the request method"
    char *method = text;
    if (strcasecmp(method, "GET") == 0)
    {
        this->m_method = GET;
    }
    else if (strcasecmp(method, "POST") == 0)
    {
        this->m_method = POST;
        cgi = 1;
    }
    else
    {
        return BAD_REQUEST;
    }

    // 2. URL and version
    this->m_url += strspn(this->m_url, " \t"); // to ensure this->m_url is at the first non-space character
    this->m_version = strpbrk(this->m_url, " \t");
    if (!this->m_version)
    {
        return BAD_REQUEST;
    }
    *this->m_version++ = '\0';                         // to split URL and version
    this->m_version += strspn(this->m_version, " \t"); // to ensure this->m_version is at the first non-space character

    if (strcasecmp(this->m_version, "HTTP/1.1") != 0)
    {
        return BAD_REQUEST;
    }

    if (strncasecmp(this->m_url, "http://", 7) == 0)
    {
        this->m_url += 7;
        this->m_url = strchr(this->m_url, '/');
    }
    if (strncasecmp(this->m_url, "https://", 8) == 0)
    {
        this->m_url += 8;
        this->m_url = strchr(this->m_url, '/');
    }
    if (!this->m_url || this->m_url[0] != '/')
    {
        return BAD_REQUEST;
    }

    if (strlen(this->m_url) == 1)
    {
        strcat(this->m_url, "judge.html");
    }

    this->m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

HTTP_CODE Http_conn::parse_headers(char *text)
{

    if (text[0] == '\0')
    { // an empty line
        if (this->m_content_length != 0)
        { // POST
            this->m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
        {
            this->m_linger = true;
        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        this->m_content_length = atol(text); // atol: char* to long int
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        this->m_host = text;
    }
    else
    {
        std::cout << "oop!unknow header:" << text << std::endl;
    }
    return NO_REQUEST;
}

HTTP_CODE Http_conn::parse_content(char *text)
{
    if (this->m_read_idx >= (this->m_content_length + this->m_checked_idx))
    {
        text[this->m_content_length] = '\0';
        this->m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

HTTP_CODE Http_conn::do_request()
{
    strcpy(this->m_real_file, doc_root); // directory of websites
    int len = strlen(doc_root);

    const char *p = strrchr(this->m_url, '/'); // search '/' from end to start

    if (this->cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
    { // "2...." for login; "3...." for registration

        char flag = this->m_url[1]; // "2" or "3"
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        strcat(m_url_real, this->m_url + 2);                                  // e.g. "/register.htmlCGISQL.cgi"
        strncpy(this->m_real_file + len, m_url_real, FILENAME_LEN - len - 1); // copy at most x elements
        free(m_url_real);

        // Extract user name and password
        // e.g. "name=xxx&password=xxx"
        //       012345  i1234567890
        char name[100], password[100];
        int i;
        for (i = 5; this->m_string[i] != '&'; ++i)
        {
            name[i - 5] = this->m_string[i];
        }
        name[i - 5] = '\0'; // when end, i -> '&'

        int j = 0;
        for (i = i + 10; this->m_string[i] != '\0'; ++i, ++j)
        {
            password[j] = this->m_string[i];
        }
        password[j] = '\0';

        // Check user name and password
        if (*(p + 1) == '3') // Register
        {
            char *sql_insert = (char *)malloc(sizeof(char) * 200);

            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");

            if (users_map.find(name) == users_map.end())
            { // user name do not exist before registration
                m_lock.lock();
                int res = mysql_query(this->mysql, sql_insert);
                users_map.insert(pair<string, string>(name, password));
                m_lock.unlock();

                if (!res)
                {
                    strcpy(this->m_url, "/log.html");
                }
                else
                {
                    strcpy(this->m_url, "/registerError.html");
                }
            }
            else
            {
                strcpy(this->m_url, "/registerError.html");
            }
        }
        else if (*(p + 1) == '2') // Login
        {
            if (users_map.find(name) != users_map.end() && users_map[name] == password)
            {
                strcpy(this->m_url, "/welcome.html");
            }
            else
            {
                strcpy(this->m_url, "/logError.html");
            }
        }
    }

    if (*(p + 1) == '0')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        strncpy(this->m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '1')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        strncpy(this->m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '5')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(this->m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '6')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(this->m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '7')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");
        strncpy(this->m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else
    {
        strncpy(this->m_real_file + len, this->m_url, FILENAME_LEN - len - 1);
    }

    // to get the file status of this->m_real_file and store it in this->m_file_stat
    if (stat(this->m_real_file, &this->m_file_stat) < 0)
        return NO_RESOURCE;
    // if it can not be read
    if (!(this->m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;
    // if it is a directory
    if (S_ISDIR(this->m_file_stat.st_mode))
        return BAD_REQUEST;

    // open the file and map it to this->m_file_address
    int fd = open(this->m_real_file, O_RDONLY);
    this->m_file_address = (char *)mmap(0, this->m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

char *Http_conn::get_line()
{
    return this->m_read_buf + this->m_start_line;
}

LINE_STATUS Http_conn::parse_line()
{
    // Job: update m_checked_idx; return LINE_XX
    // Attention: m_checked_idx -- the first one that has not been checked
    char temp;
    for (; this->m_checked_idx < this->m_read_idx; ++this->m_checked_idx)
    {
        temp = this->m_read_buf[this->m_checked_idx]; // m_read_buf[] is populated by read_once() already
        if (temp == '\r')                             // if temp = "enter"
        {
            if ((this->m_checked_idx + 1) == this->m_read_idx)
            { // if m_checked_idx is the last one
                return LINE_OPEN;
            }
            else if (this->m_read_buf[this->m_checked_idx + 1] == '\n')
            {
                this->m_read_buf[this->m_checked_idx++] = '\0';
                this->m_read_buf[this->m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if (temp == '\n') // usually occur when LINE_OPEN the last time
        {
            if (this->m_checked_idx > 1 && this->m_read_buf[this->m_checked_idx - 1] == '\r')
            {
                this->m_read_buf[this->m_checked_idx - 1] = '\0';
                this->m_read_buf[this->m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

void Http_conn::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

bool Http_conn::add_response(const char *format, ...)
{
    // Job: add response to m_write_buf
    // update m_write_idx

    if (this->m_write_idx >= WRITE_BUFFER_SIZE)
    {
        return false;
    }

    va_list arg_list;
    va_start(arg_list, format);
    int length = vsnprintf(this->m_write_buf + this->m_write_idx, WRITE_BUFFER_SIZE - 1 - this->m_write_idx, format, arg_list);

    if (length >= (WRITE_BUFFER_SIZE - 1 - this->m_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    this->m_write_idx += length;
    va_end(arg_list);
    /*LOG_INFO("request:%s", m_write_buf);
    Log::get_instance()->flush();*/

    return true;
}

bool Http_conn::add_content(const char *content)
{
    return this->add_response("%s", content);
}

bool Http_conn::add_status_line(int status, const char *title)
{

    return this->add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool Http_conn::add_headers(int content_length)
{
    this->add_content_length(content_length); // to record content_length
    this->add_linger();                       // to keep the connection alive
    this->add_blank_line();
    return true;
}

bool Http_conn::add_content_type()
{
    return this->add_response("Content-Type:%s\r\n", "text/html");
}

bool Http_conn::add_content_length(int content_length)
{
    return this->add_response("Content-Length: %d\r\n", content_length);
}

bool Http_conn::add_linger()
{
    return this->add_response("Connection: %s\r\n", (this->m_linger == true) ? "keep-alive" : "close");
}

bool Http_conn::add_blank_line()
{
    return this->add_response("%s", "\r\n");
}
