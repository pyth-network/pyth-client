#include "tx.hpp"

static pc::hash gen_sys_id()
{
  pc::hash id;
  id.zero();
  return id;
}

// system program id
static pc::hash sys_id = gen_sys_id();

namespace pc
{
  namespace tx
  {

    enum system_instruction : uint32_t
    {
      e_create_account,
      e_assign,
      e_transfer
    };

    // transfer amount from one account to another
    void transfer( bincode& msg,
                   const hash& rhash,
                   const key_pair& sender,
                   const pub_key& receiver,
                   uint64_t lamports )
    {
      // asignatures section
      msg.add_len<1>();      // one signature
      size_t sign_idx = msg.reserve_sign();

      // message header
      size_t msg_idx = msg.get_pos();
      msg.add( (uint8_t)1 ); // signing accounts
      msg.add( (uint8_t)0 ); // read-only signed accounts
      msg.add( (uint8_t)1 ); // read-only unsigned accounts

      // accounts
      msg.add_len<3>();      // 3 accounts: sender, receiver, program
      msg.add( sender );     // sender account
      msg.add( receiver );   // receiver account
      msg.add( sys_id );     // system programid

      // recent block hash
      msg.add( rhash );      // recent block hash

      // instructions section
      msg.add_len<1>();      // one instruction
      msg.add( (uint8_t)2);  // program_id index
      msg.add_len<2>();      // 2 accounts: sender, receiver
      msg.add( (uint8_t)0 ); // index of sender account
      msg.add( (uint8_t)1 ); // index of receiver account

      // instruction parameter section
      msg.add_len<12>();     // size of data array
      msg.add( (uint32_t)system_instruction::e_transfer );
      msg.add( (uint64_t)lamports );

      // sign message
      msg.sign( sign_idx, msg_idx, sender );
    }

  }
}
