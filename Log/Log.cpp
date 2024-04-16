#include "Log.h"
Log *Log::get_instance()
{
    static Log instance;
    return &instance;
}

void *Log::flush_log_thread(void *args)
{
    Log::get_instance()->async_write_log();
}

bool Log::init(const char *file_name, int log_buf_size, int split_lines, int max_queue_size)
{
    if (max_queue_size >= 1)
    {
        this->m_is_async = true;
        this->m_log_queue = new Block_queue<string>(max_queue_size);
        pthread_t tid;
        pthread_create(&tid, NULL, flush_log_thread, NULL);
    }

    this->m_log_buf_size = log_buf_size;
    this->m_buf = new char[log_buf_size];

    memset(this->m_buf, '\0', log_buf_size);
    this->m_max_lines = split_lines;

    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    const char *p = strrchr(file_name, '/');
    char log_full_name[256] = {0};

    if (p == NULL)
    {
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    }
    else
    {
        strcpy(log_name, p + 1);
        strncpy(this->dir_name, file_name, p - file_name + 1);
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", this->dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
    }

    this->m_today = my_tm.tm_mday;

    this->m_fp = fopen(log_full_name, "a");

    if (m_fp == NULL)
    {
        return false;
    }

    return true;
}

void Log::write_log(int level, const char *format, ...)
{
    // get time
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    // get info type
    char s[16] = {0};
    switch (level)
    {
    case 0:
        strcpy(s, "[debug]:");
        break;
    case 1:
        strcpy(s, "[info]:");
        break;
    case 2:
        strcpy(s, "[warn]:");
        break;
    case 3:
        strcpy(s, "[erro]:");
        break;
    default:
        strcpy(s, "[info]:");
        break;
    }

    this->m_mutex.lock();
    this->m_line_count++;

    if (this->m_today != my_tm.tm_mday || this->m_line_count % this->m_max_lines == 0)
    {

        fflush(m_fp);
        fclose(m_fp);

        char new_log[256] = {0};
        char tail[16] = {0};

        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);

        if (m_today != my_tm.tm_mday)
        {
            snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
            this->m_today = my_tm.tm_mday;
            this->m_line_count = 0;
        }
        else
        {
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, this->m_line_count / this->m_max_lines);
        }
        m_fp = fopen(new_log, "a");
    }

    m_mutex.unlock();

    va_list valst;
    va_start(valst, format);

    string log_str;
    m_mutex.lock();

    // 写入的具体时间内容格式
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);

    int m = vsnprintf(m_buf + n, m_log_buf_size - 1, format, valst);
    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';
    log_str = m_buf;

    this->m_mutex.unlock();

    if (this->m_is_async && !this->m_log_queue->full())
    {
        this->m_log_queue->push(log_str);
    }
    else
    {
        this->m_mutex.lock();
        fputs(log_str.c_str(), m_fp);
        this->m_mutex.unlock();
    }

    va_end(valst);
}

void Log::flush(void)
{
    this->m_mutex.lock();
    // 强制刷新写入流缓冲区
    fflush(m_fp);
    this->m_mutex.unlock();
}

Log::Log()
{
    this->m_line_count = 0;
    this->m_is_async = false;
}

Log::~Log()
{
    if (this->m_fp != NULL)
    {
        fclose(this->m_fp);
    }
}

void *Log::async_write_log()
{
    string single_log;
    while (this->m_log_queue->pop(single_log))
    {
        this->m_mutex.lock();
        fputs(single_log.c_str(), this->m_fp);
        this->m_mutex.unlock();
    }
}
