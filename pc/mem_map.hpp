#pragma once

#include <string>


namespace pc
{

  // memory-mapped file
  class mem_map
  {
  public:
    mem_map();
    ~mem_map();

    // initialize from file
    void set_file( const std::string& );
    std::string get_file() const;
    bool remap();
    bool init();

    // access to file data
    const char *data() const;
    size_t size() const;

  private:
    void close();
    int         fd_;
    size_t      len_;
    const char *buf_;
    std::string file_;
  };

  inline const char *mem_map::data() const
  {
    return buf_;
  }

  inline size_t mem_map::size() const
  {
    return len_;
  }

}
