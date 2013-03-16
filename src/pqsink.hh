#ifndef PEQUOD_SINK_HH
#define PEQUOD_SINK_HH
#include "str.hh"
#include "string.hh"
#include "interval.hh"
#include "local_vector.hh"
#include "local_str.hh"
#include "interval_tree.hh"
#include "pqdatum.hh"

namespace pq {
class Join;
class Server;
class Match;
class ValidJoinRange;

class ServerRangeBase {
  public:
    inline ServerRangeBase(Str first, Str last);

    typedef Str endpoint_type;
    inline Str ibegin() const;
    inline Str iend() const;
    inline ::interval<Str> interval() const;
    inline Str subtree_iend() const;
    inline void set_subtree_iend(Str subtree_iend);
  protected:
    LocalStr<24> ibegin_;
    LocalStr<24> iend_;
    Str subtree_iend_;
};

class ServerRange : public ServerRangeBase {
  public:
    enum range_type {
        joinsink = 1, validjoin = 2
    };
    ServerRange(Str first, Str last, range_type type, Join *join = 0);
    virtual ~ServerRange();

    inline range_type type() const;
    inline Join* join() const;
    inline bool expired_at(uint64_t t) const;

    void validate(Str first, Str last, Server& server);

    friend std::ostream& operator<<(std::ostream&, const ServerRange&);

    static uint64_t allocated_key_bytes;
  public:
    rblinks<ServerRange> rblinks_;
  private:
    range_type type_;
    Join* join_;
    uint64_t expires_at_;

    struct validate_args;
    void validate(validate_args& va, int joinpos);

    friend class ValidJoinRange;
};

class IntermediateUpdate : public ServerRangeBase {
  public:
    IntermediateUpdate(Str first, Str last, Str context, Str key, int joinpos, int notifier);

    typedef Str endpoint_type;
    inline Str key() const;
    inline int notifier() const;

    friend std::ostream& operator<<(std::ostream&, const IntermediateUpdate&);

  public:
    rblinks<IntermediateUpdate> rblinks_;
  private:
    LocalStr<12> context_;
    LocalStr<24> key_;
    int joinpos_;
    int notifier_;

    friend class ValidJoinRange;
};

class ValidJoinRange : public ServerRange {
  public:
    inline ValidJoinRange(Str first, Str last, Join *join);
    ~ValidJoinRange();

    inline void ref();
    inline void deref();

    inline bool valid() const;
    inline void invalidate();
    void add_update(int joinpos, const String& context, Str key, int notifier);
    inline bool need_update() const;
    void update(Str first, Str last, Server& server);

    inline void update_hint(const ServerStore& store, ServerStore::iterator hint) const;
    inline Datum* hint() const;

  private:
    int refcount_;
    bool valid_;
    mutable Datum* hint_;
    interval_tree<IntermediateUpdate> updates_;

    bool update_iu(Str first, Str last, IntermediateUpdate* iu, Server& server);
};

class ServerRangeSet {
  public:
    inline ServerRangeSet(Str first, Str last, int types);

    inline void push_back(ServerRange* r);

    void validate(Server& server);

    friend std::ostream& operator<<(std::ostream&, const ServerRangeSet&);
    inline int total_size() const;

  private:
    local_vector<ServerRange*, 5> r_;
    Str first_;
    Str last_;
    int types_;

    void validate_join(ServerRange* jr, Server& server);
};

inline ServerRangeBase::ServerRangeBase(Str first, Str last) : ibegin_(first), iend_(last) {
}

inline Str ServerRangeBase::ibegin() const {
    return ibegin_;
}

inline Str ServerRangeBase::iend() const {
    return iend_;
}

inline interval<Str> ServerRangeBase::interval() const {
    return make_interval(ibegin(), iend());
}

inline Str ServerRangeBase::subtree_iend() const {
    return subtree_iend_;
}

inline void ServerRangeBase::set_subtree_iend(Str subtree_iend) {
    subtree_iend_ = subtree_iend;
}

inline ServerRange::range_type ServerRange::type() const {
    return type_;
}

inline Join* ServerRange::join() const {
    return join_;
}

inline bool ServerRange::expired_at(uint64_t t) const {
    return expires_at_ && (expires_at_ < t);
}

inline ValidJoinRange::ValidJoinRange(Str first, Str last, Join* join)
    : ServerRange(first, last, validjoin, join), refcount_(1), valid_(true), hint_(0) {
}

inline void ValidJoinRange::ref() {
    ++refcount_;
}

inline void ValidJoinRange::deref() {
    if (--refcount_ == 0)
        delete this;            // XXX
}

inline bool ValidJoinRange::valid() const {
    return valid_;
}

inline void ValidJoinRange::invalidate() {
    //valid_ = false;
}

inline bool ValidJoinRange::need_update() const {
    return !updates_.empty();
}

inline void ValidJoinRange::update_hint(const ServerStore& store, ServerStore::iterator hint) const {
    Datum* hd = hint == store.end() ? 0 : hint.operator->();
    if (hd)
        hd->ref();
    if (hint_)
        hint_->deref();
    hint_ = hd;
}

inline Datum* ValidJoinRange::hint() const {
    return hint_ && hint_->valid() ? hint_ : 0;
}

inline ServerRangeSet::ServerRangeSet(Str first, Str last, int types)
    : first_(first), last_(last), types_(types) {
}

inline void ServerRangeSet::push_back(ServerRange* r) {
    if (r->type() & types_)
        r_.push_back(r);
}

inline int ServerRangeSet::total_size() const {
    return r_.size();
}

inline Str IntermediateUpdate::key() const {
    return key_;
}

inline int IntermediateUpdate::notifier() const {
    return notifier_;
}

} // namespace pq
#endif
