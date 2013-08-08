// -*- mode: c++ -*-
#include "redisfd.hh"
#include "check.hh"
#include "hashclient.hh"
#include <limits.h>
#ifndef IOV_MAX
#define IOV_MAX 1024
#endif

redis_fd::~redis_fd() {
    wrkill_();
    rdkill_();
    wrwake_();
    rdwake_();
}

void redis_fd::write(const Str req) {
    wrelem* w = &wrelem_.back();
    if (w->sa.length() >= wrhiwat) {
        wrelem_.push_back(wrelem());
        w = &wrelem_.back();
        w->sa.reserve(wrcap);
        w->pos = 0;
    }
    int old_len = w->sa.length();
    w->sa << req;
    wrsize_ += w->sa.length() - old_len;
    wrwake_();
    if (!wrblocked_ && wrelem_.front().sa.length() >= wrlowat)
        write_once();
}

void redis_fd::read(tamer::event<String> receiver) {
    while ((rdpos_ != rdlen_ || !rdblocked_)
           && rdwait_.size() && read_once(rdwait_.front().result_pointer())) {
        rdwait_.front().unblock();
        rdwait_.pop_front();
    }
    if ((rdpos_ != rdlen_ || !rdblocked_)
        && read_once(receiver.result_pointer()))
        receiver.unblock();
    else {
        rdwait_.push_back(receiver);
        rdwake_();
    }
}

bool redis_fd::read_once(String* receiver) {
 readmore:
    // if buffer empty, read more data
    if (rdpos_ == rdlen_) {
        // make new buffer or reuse existing buffer
        if (rdcap - rdpos_ < 4096) {
            if (rdbuf_.data_shared())
                rdbuf_ = String::make_uninitialized(rdcap);
            rdpos_ = rdlen_ = 0;
        }

        ssize_t amt = ::read(fd_.value(),
                             const_cast<char*>(rdbuf_.data()) + rdpos_,
                             rdcap - rdpos_);
        rdblocked_ = amt == 0 || amt == (ssize_t) -1;

        if (amt != 0 && amt != (ssize_t) -1)
            rdlen_ += amt;
        else if (amt == 0)
            fd_.close();
        else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
            fd_.close(-errno);

        if (rdpos_ == rdlen_)
            return false;
    }
    // process new data
    rdpos_ += rdparser_.consume(rdbuf_.begin() + rdpos_, rdlen_ - rdpos_);
    if (rdparser_.complete()) {
        mandatory_assert(receiver);
        std::swap(*receiver, rdparser_.value());
        rdparser_.reset();
        return true;
    } else
        goto readmore;
}

tamed void redis_fd::reader_coroutine() {
    tvars {
        tamer::event<> kill;
        tamer::rendezvous<> rendez;
    }

    kill = rdkill_ = tamer::make_event(rendez);

    while (kill && fd_) {
        if (rdwait_.empty())
            twait { rdwake_ = make_event(); }
        else if (rdblocked_) {
            twait { tamer::at_fd_read(fd_.value(), make_event()); }
            rdblocked_ = false;
        } else if (read_once(rdwait_.front().result_pointer())) {
            rdwait_.front().unblock();
            rdwait_.pop_front();
            if (pace_recovered())
                pacer_();
        }
    }

    for (auto& e : rdwait_)
        e.unblock();
    rdwait_.clear();
    kill();
}

void redis_fd::check() const {
    // document invariants
    assert(!wrelem_.empty());
    for (auto& w : wrelem_)
        assert(w.pos <= w.sa.length());
    for (size_t i = 1; i < wrelem_.size(); ++i)
        assert(wrelem_[i].pos == 0);
    for (size_t i = 0; i + 1 < wrelem_.size(); ++i)
        assert(wrelem_[i].pos < wrelem_[i].sa.length());
    if (wrelem_.size() == 1)
        assert(wrelem_[0].pos < wrelem_[0].sa.length()
               || wrelem_[0].sa.empty());
    size_t wrsize = 0;
    for (auto& w : wrelem_)
        wrsize += w.sa.length() - w.pos;
    assert(wrsize == wrsize_);
}

void redis_fd::write_once() {
    // check();
    assert(!wrelem_.front().sa.empty());

    struct iovec iov[3];
    int iov_count = (wrelem_.size() > 3 ? 3 : (int) wrelem_.size());
    size_t total = 0;
    for (int i = 0; i != iov_count; ++i) {
        iov[i].iov_base = wrelem_[i].sa.data() + wrelem_[i].pos;
        iov[i].iov_len = wrelem_[i].sa.length() - wrelem_[i].pos;
        total += iov[i].iov_len;
    }

    ssize_t amt = writev(fd_.value(), iov, iov_count);
    wrblocked_ = amt == 0 || amt == (ssize_t) -1;

    if (amt != 0 && amt != (ssize_t) -1) {
        wrsize_ -= amt;
        while (wrelem_.size() > 1
               && amt >= wrelem_.front().sa.length() - wrelem_.front().pos) {
            amt -= wrelem_.front().sa.length() - wrelem_.front().pos;
            wrelem_.pop_front();
        }
        wrelem_.front().pos += amt;
        if (wrelem_.front().pos == wrelem_.front().sa.length()) {
            assert(wrelem_.size() == 1);
            wrelem_.front().sa.clear();
            wrelem_.front().pos = 0;
        }
        if (pace_recovered())
            pacer_();
    } else if (amt == 0)
        fd_.close();
    else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
        fd_.close(-errno);
}

tamed void redis_fd::writer_coroutine() {
    tvars {
        tamer::event<> kill;
        tamer::rendezvous<> rendez;
    }

    kill = wrkill_ = tamer::make_event(rendez);

    while (kill && fd_) {
        if (wrelem_.size() == 1 && wrelem_.front().sa.empty())
            twait { wrwake_ = make_event(); }
        else if (wrblocked_) {
            twait { tamer::at_fd_write(fd_.value(), make_event()); }
            wrblocked_ = false;
        } else
            write_once();
    }

    kill();
}

tamed void test_redis_async() {
    tvars {
        pq::RedisfdHashClient* client;
        tamer::fd fd;
        String v;
    }
    twait { tamer::tcp_connect(in_addr{htonl(INADDR_LOOPBACK)}, 6379, make_event(fd)); }
    client = new pq::RedisfdHashClient(fd);
    twait { client->set("k1", "v1", make_event()); }
    twait { client->get("k1", make_event(v)); }
}