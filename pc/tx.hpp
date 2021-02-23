#pragma once

#include <pc/bincode.hpp>
#include <pc/key_pair.hpp>

namespace pc
{

  // transaction message construction
  namespace tx
  {

    // create new account with initial funding, space and program ownership
    void create_account( bincode&,
                         const key_pair& funding_acc,
                         const key_pair& new_acc,
                         uint64_t lamports,
                         uint64_t space,
                         const pub_key& prog_id );

    // assign account to a program
    void assign( bincode&,
                 const key_pair& acc,
                 const pub_key& prog_id );

    // construct transfer instruction message
    void transfer( bincode&,
                   const hash& recent_hash,
                   const key_pair& sender,
                   const pub_key& receiver,
                   uint64_t lamports );

  }

}
