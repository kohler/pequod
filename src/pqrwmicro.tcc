#include "pqrwmicro.hh"
#include <tamer/tamer.hh>
#include "hashclient.hh"
#include "pqtwitter.hh"
#include "pqremoteclient.hh"

namespace pq {

typedef pq::TwitterHashShim<pq::RedisfdHashClient> redis_shim_type;

tamed void run_rwmicro_redisfd(Json& tp_param) {
    tvars {
        pq::RedisfdHashClient* client;
        tamer::fd fd;
        redis_shim_type* shim;
        pq::RwMicro<redis_shim_type> *rw;
    }
    twait { tamer::tcp_connect(in_addr{htonl(INADDR_LOOPBACK)}, 6379, make_event(fd)); }
    client = new pq::RedisfdHashClient(fd);
    shim = new redis_shim_type(*client);
    rw = new pq::RwMicro<redis_shim_type>(tp_param, *shim);
    rw->safe_run();
}

typedef pq::TwitterShim<pq::RemoteClient> pq_shim_type;

tamed void run_rwmicro_pqremote(Json& tp_param, int client_port) {
    tvars {
        tamer::fd fd;
        pq::RemoteClient* rc;
        pq_shim_type* shim;
        pq::RwMicro<pq_shim_type>* rw;
    }
    twait { tamer::tcp_connect(in_addr{htonl(INADDR_LOOPBACK)}, client_port, make_event(fd)); }
    rc = new RemoteClient(fd);
    shim = new pq_shim_type(*rc);
    rw = new pq::RwMicro<pq_shim_type>(tp_param, *shim);
    rw->safe_run();
}

};
