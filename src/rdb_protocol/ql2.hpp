// Copyright 2010-2012 RethinkDB, all rights reserved.
#ifndef RDB_PROTOCOL_QL2_HPP_
#define RDB_PROTOCOL_QL2_HPP_

#include <stack>
#include <string>
#include <vector>
#include <map>

#include "utils.hpp"

#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/ptr_container/ptr_map.hpp>

#include "containers/uuid.hpp"
#include "clustering/administration/namespace_interface_repository.hpp"

#include "rdb_protocol/datum.hpp"
#include "rdb_protocol/env.hpp"
#include "rdb_protocol/protocol.hpp"
#include "rdb_protocol/ql2.pb.h"
#include "rdb_protocol/stream_cache.hpp"
#include "rdb_protocol/val.hpp"

namespace ql {

struct backtrace_t {
    struct frame_t {
    public:
        frame_t(int _pos) : type(POS), pos(_pos) { }
        frame_t(const std::string &_opt) : type(OPT), opt(_opt) { }
        Response2_Frame toproto() const;
    private:
        enum type_t { POS = 0, OPT = 1 };
        type_t type;
        int pos;
        std::string opt;
    };
    std::list<frame_t> frames;
};

class exc_t : public std::exception {
public:
    exc_t(const std::string &_msg) : msg(_msg) { }
    virtual ~exc_t() throw () { }
    backtrace_t backtrace;
    const char *what() const throw () { return msg.c_str(); }
private:
    const std::string msg;
};

void _runtime_check(const char *test, const char *file, int line,
                    bool pred, std::string msg = "");
#define runtime_check(pred, msg) \
    _runtime_check(stringify(pred), __FILE__, __LINE__, pred, msg)
// TODO: do something smarter?
#define runtime_fail(args...) runtime_check(false, strprintf(args))
// TODO: make this crash in debug mode
#define sanity_check(test) runtime_check(test, "SANITY_CHECK")

class term_t;
class func_t {
public:
    func_t(env_t *env, const std::vector<int> &args, const Term2 *body_source);
    val_t *call(const std::vector<datum_t *> &args);
private:
    std::vector<datum_t *> argptrs;
    scoped_ptr_t<term_t> body;
};

term_t *compile_term(env_t *env, const Term2 *t);

class term_t {
public:
    term_t(env_t *_env);
    virtual ~term_t();

    virtual const char *name() const = 0;
    val_t *eval(bool use_cached_val = true);

    val_t *new_val(datum_t *d);
    val_t *new_val() { return new_val(new datum_t()); }
    template<class T>
    val_t *new_val(T t) { return new_val(new datum_t(t)); }
    template<class T>
    void set_bt(T t) { frame.init(new backtrace_t::frame_t(t)); }
    bool has_bt() { return frame.has(); }
    backtrace_t::frame_t get_bt() { return *frame.get(); }
private:
    virtual val_t *eval_impl() = 0;
    val_t *cached_val;
    env_t *env;

    scoped_ptr_t<backtrace_t::frame_t> frame;
};

class datum_term_t : public term_t {
public:
    datum_term_t(env_t *env, const Datum *datum);
    virtual ~datum_term_t();

    virtual val_t *eval_impl();
    virtual const char *name() const;
private:
    scoped_ptr_t<val_t> raw_val;
};

class op_term_t : public term_t {
public:
    op_term_t(env_t *env, const Term2 *term);
    virtual ~op_term_t();
    size_t num_args() const;
    term_t *get_arg(size_t i);
    void check_no_optargs() const;
private:
    boost::ptr_vector<term_t> args;
    boost::ptr_map<const std::string, term_t> optargs;
    //std::vector<term_t *> args;
    //std::map<const std::string, term_t *> optargs;
};

class simple_op_term_t : public op_term_t {
public:
    simple_op_term_t(env_t *env, const Term2 *term);
    virtual ~simple_op_term_t();
    virtual val_t *eval_impl();
private:
    virtual val_t *simple_call_impl(std::vector<val_t *> *args) = 0;
};

class predicate_term_t : public simple_op_term_t {
public:
    predicate_term_t(env_t *env, const Term2 *term);
    virtual ~predicate_term_t();
    virtual val_t *simple_call_impl(std::vector<val_t *> *args);
    virtual const char *name() const;
private:
    const char *namestr;
    bool invert;
    bool (datum_t::*pred)(const datum_t &rhs) const;
};

class arith_term_t : public simple_op_term_t {
public:
    arith_term_t(env_t *env, const Term2 *term);
    virtual ~arith_term_t();
    virtual val_t *simple_call_impl(std::vector<val_t *> *args);
    virtual const char *name() const;
private:
    const char *namestr;
    datum_t (*op)(const datum_t &lhs, const datum_t &rhs);
};

// Fills in [res] with an error of type [type] and message [msg].
void fill_error(Response2 *res, Response2_ResponseType type, std::string msg,
                const backtrace_t &bt=backtrace_t());

void run(Query2 *q, env_t *env, Response2 *res, stream_cache_t *stream_cache);

} // namespace ql

#endif /* RDB_PROTOCOL_QL2_HPP_ */