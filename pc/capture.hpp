#pragma once

#include <pc/misc.hpp>
#include <pc/error.hpp>
#include <oracle/oracle.h>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <zlib.h>

namespace pc
{

  // capture aggregate price update
  class capture : public error
  {
  public:

    capture();
    ~capture();

    void set_file( const std::string& );
    std::string get_file() const;

    // start capture thread
    bool init();

    // capture time, account number and account content
    void write( pc_pub_key_t *acc, pc_acc_t * );

    // flush buffer to disk
    void flush();

  public:
    void run();

  private:

    struct PC_PACKED cap_buf {
      uint64_t size_;
      char     buf_[];
    };

    static const uint64_t max_size = 32*1024;

    cap_buf *alloc();

    typedef std::vector<cap_buf*> buf_vec_t;
    typedef std::atomic<bool> atomic_t;

    cap_buf    *curr_;
    std::mutex  mtx_;
    std::thread thrd_;
    atomic_t    is_run_;
    atomic_t    is_wtr_;
    buf_vec_t   pend_;
    buf_vec_t   done_;
    buf_vec_t   reuse_;
    int         fd_;
    gzFile      zfd_;
    std::string file_;
  };

}
