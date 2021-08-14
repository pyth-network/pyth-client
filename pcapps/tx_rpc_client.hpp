#pragma once

#include <pc/rpc_client.hpp>

namespace pc {

  namespace rpc {

    // get validator node health
    class get_health : public rpc_request
    {
    public:
      void request( json_wtr& ) override;
      void response( const jtree& ) override;
    };

    // get mapping of node id to ip address and port
    class get_cluster_nodes : public rpc_request
    {
    public:
      bool get_ip_addr( const pub_key&, ip_addr& );
      void request( json_wtr& ) override;
      void response( const jtree&p) override;
    private:
      struct trait_node {
        static const size_t hsize_ = 859UL;
        typedef uint32_t        idx_t;
        typedef pub_key         key_t;
        typedef const pub_key&  keyref_t;
        typedef ip_addr         val_t;
        struct hash_t {
          idx_t operator() ( keyref_t a ) {
            uint64_t *i = (uint64_t*)a.data();
            return i[0];
          }
        };
      };
      typedef hash_map<trait_node> node_map_t;
      node_map_t nmap_;
    };

    // get id of leader node by slot
    class get_slot_leaders : public rpc_request
    {
    public:
      get_slot_leaders();
      void set_slot(uint64_t slot);
      void set_limit( uint64_t limit );
      pub_key *get_leader( uint64_t );
      uint64_t get_last_slot() const;
      void request( json_wtr& ) override;
      void response( const jtree& ) override;
    private:
      typedef std::vector<pub_key> ldr_vec_t;
      uint64_t  rslot_;
      uint64_t  limit_;
      uint64_t  lslot_;
      ldr_vec_t lvec_;
    };

    // find out when slots update
    class slot_subscribe : public rpc_subscription
    {
    public:
      uint64_t get_slot() const;
      void request( json_wtr& ) override;
      void response( const jtree& ) override;
      bool notify( const jtree& ) override;
    private:
      uint64_t slot_;
    };

  }

}
