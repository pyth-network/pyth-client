#pragma once

#include <ctype.h>
#include <stdlib.h>

namespace pc
{

  namespace json
  {

    // expected interface for use with parse()
    class handler
    {
    public:
      void parse_start_object() {}
      void parse_start_array() {}
      void parse_end_object() {}
      void parse_end_array() {}
      void parse_key( const char *, const char *) {}
      void parse_string( const char *, const char *) {}
      void parse_number( const char *, const char *) {}
      void parse_keyword( const char *, const char *) {}
    };

    // json tokenizer for (expected to be) well-formed json messages
    template<class T>
    void parse( const char *cptr, size_t sz, T *hptr )
    {
      typedef enum { e_start, e_string, e_number, e_keyword } state_t;
      state_t st = e_start;
      const char *txt, *end = &cptr[sz];
      for(;;) {
        switch(st) {
          case e_start: {
            for(;;++cptr) {
              if( cptr==end ) return;
              if (*cptr == '{' ) {
                hptr->parse_start_object();
              } else if ( *cptr == '[' ) {
                hptr->parse_start_array();
              } else if (*cptr == '}' ) {
                hptr->parse_end_object();
              } else if ( *cptr == ']' ) {
                hptr->parse_end_array();
              } else if ( *cptr == '"' ) {
                st = e_string; txt=cptr+1; break;
              } else if ( *cptr == '-' || *cptr == '.' || isdigit(*cptr)) {
                st = e_number; txt=cptr; break;
              } else if ( *cptr == 't' || *cptr == 'f' || *cptr == 'n' ) {
                st = e_keyword; txt=cptr; break;
              }
            }
            ++cptr;
            break;
          }

          case e_string: {
            for(;;++cptr) {
              if( cptr==end ) return;
              if (*cptr == '"' ) {
                st = e_start;
                const char *etxt = cptr;
                // peek if this is a key
                for(++cptr;;++cptr) {
                  if( cptr==end ) return;
                  if ( *cptr == ':' ) {
                    hptr->parse_key( txt, etxt );
                    break;
                  } else if ( !isspace(*cptr) ) {
                    hptr->parse_string( txt, etxt );
                    break;
                  }
                }
                break;
              }
            }
            break;
          }

          case e_number: {
            for(;;++cptr) {
              if( cptr==end ) return;
              if ( !isdigit(*cptr) && *cptr!='.' &&
                   *cptr!='-' && *cptr!='e' && *cptr!='+' ) {
                st = e_start; hptr->parse_number( txt, cptr );break;
              }
            }
            break;
          }

          case e_keyword:{
            for(;;++cptr) {
              if ( cptr==end ) return;
              if (!isalpha(*cptr) ) {
                st = e_start; hptr->parse_keyword( txt, cptr ); break;
              }
            }
            break;
          }
        }
      }
    }

  }

}
