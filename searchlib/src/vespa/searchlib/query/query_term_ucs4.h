// Copyright 2019 Oath Inc. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
#pragma once

#include "query_term_simple.h"
#include <vespa/fastlib/text/unicodeutil.h>
#include <atomic>

namespace search {

/**
 * Query term that can be returned in UCS-4 encoded form.
 */
class QueryTermUCS4 : public QueryTermSimple {
public:
    typedef std::unique_ptr<QueryTermUCS4> UP;
    QueryTermUCS4(const QueryTermUCS4 &) = delete;
    QueryTermUCS4 & operator = (const QueryTermUCS4 &) = delete;
    QueryTermUCS4(QueryTermUCS4 &&) = delete;
    QueryTermUCS4 & operator = (QueryTermUCS4 &&) = delete;
    QueryTermUCS4();
    QueryTermUCS4(const string & term_, Type type);
    ~QueryTermUCS4();
    uint32_t getTermLen() const { return _cachedTermLen; }
    uint32_t term(const char * & t)     const { t = getTerm(); return _cachedTermLen; }
    void visitMembers(vespalib::ObjectVisitor &visitor) const override;
    uint32_t term(const ucs4_t * & t) {
        if (!_filled.load(std::memory_order_relaxed)) {
            fillUCS4();
        }
        t = (_termUCS4) ? _termUCS4.get() : &ZERO_TERM;
        return _cachedTermLen;
    }
private:
    void fillUCS4();
    static ucs4_t ZERO_TERM;
    std::unique_ptr<ucs4_t[]>  _termUCS4;
    uint32_t                   _cachedTermLen;
    std::atomic<bool>          _filled;
};

}

