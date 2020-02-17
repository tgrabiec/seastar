/*
 * This file is open source software, licensed to you under the terms
 * of the Apache License, Version 2.0 (the "License").  See the NOTICE file
 * distributed with this work for additional information regarding copyright
 * ownership.  You may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/*
 * Copyright 2016 ScyllaDB
 */

#pragma once

#include <execinfo.h>
#include <iosfwd>
#include <variant>
#include <boost/container/static_vector.hpp>

#include <seastar/core/sstring.hh>
#include <seastar/core/print.hh>
#include <seastar/core/scheduling.hh>
#include <seastar/core/shared_ptr.hh>

namespace seastar {

struct shared_object {
    sstring name;
    uintptr_t begin;
    uintptr_t end; // C++-style, last addr + 1
};

struct frame {
    const shared_object* so;
    uintptr_t addr;
};

bool operator==(const frame& a, const frame& b);


// If addr doesn't seem to belong to any of the provided shared objects, it
// will be considered as part of the executable.
frame decorate(uintptr_t addr);

// Invokes func for each frame passing it as argument.
template<typename Func>
void backtrace(Func&& func) noexcept(noexcept(func(frame()))) {
    constexpr size_t max_backtrace = 100;
    void* buffer[max_backtrace];
    int n = ::backtrace(buffer, max_backtrace);
    for (int i = 0; i < n; ++i) {
        auto ip = reinterpret_cast<uintptr_t>(buffer[i]);
        func(decorate(ip - 1));
    }
}

class simple_backtrace {
public:
    using vector_type = boost::container::static_vector<frame, 64>;
private:
    vector_type _frames;
    size_t _hash;
private:
    size_t calculate_hash() const;
public:
    simple_backtrace() = default;
    simple_backtrace(vector_type f) : _frames(std::move(f)) {}
    size_t hash() const { return _hash; }

    friend std::ostream& operator<<(std::ostream& out, const simple_backtrace&);

    bool operator==(const simple_backtrace& o) const {
        return _hash == o._hash && _frames == o._frames;
    }

    bool operator!=(const simple_backtrace& o) const {
        return !(*this == o);
    }
};

using shared_backtrace = seastar::lw_shared_ptr<simple_backtrace>;

class saved_backtrace {
public:
    using entry = std::variant<shared_backtrace, frame>;
    using vector_type = boost::container::static_vector<entry, 32>;
private:
    simple_backtrace _main;
    vector_type _prev;
    scheduling_group _sg;
    size_t _hash;
public:
    saved_backtrace() = default;

    saved_backtrace(simple_backtrace main, vector_type prev, size_t prev_hash, scheduling_group sg)
        : _main(std::move(main))
        , _prev(std::move(prev))
        , _sg(sg)
        , _hash(main.hash() * 31 ^ prev_hash)
    { }

    size_t hash() const { return _hash; }

    friend std::ostream& operator<<(std::ostream& out, const saved_backtrace&);

    bool operator==(const saved_backtrace& o) const {
        return _hash == o._hash && _main == o._main && _prev == o._prev;
    }

    bool operator!=(const saved_backtrace& o) const {
        return !(*this == o);
    }
};

}

namespace std {

template<>
struct hash<seastar::simple_backtrace> {
    size_t operator()(const seastar::simple_backtrace& b) const {
        return b.hash();
    }
};

template<>
struct hash<seastar::saved_backtrace> {
    size_t operator()(const seastar::saved_backtrace& b) const {
        return b.hash();
    }
};

}

namespace seastar {

saved_backtrace current_backtrace() noexcept;

// Collects backtrace only within the currently executing task.
simple_backtrace current_backtrace_tasklocal() noexcept;

std::ostream& operator<<(std::ostream& out, const saved_backtrace& b);

namespace internal {

template<class Exc>
class backtraced : public Exc {
    std::shared_ptr<sstring> _backtrace;
public:
    template<typename... Args>
    backtraced(Args&&... args)
            : Exc(std::forward<Args>(args)...)
            , _backtrace(std::make_shared<sstring>(format("{} Backtrace: {}", Exc::what(), current_backtrace()))) {}

    /**
     * Returns the original exception message with a backtrace appended to it
     *
     * @return original exception message followed by a backtrace
     */
    virtual const char* what() const noexcept override {
        assert(_backtrace);
        return _backtrace->c_str();
    }
};

}

    /**
     * Throws an exception of unspecified type that is derived from the Exc type
     * with a backtrace attached to its message
     *
     * @tparam Exc exception type to be caught at the receiving side
     * @tparam Args types of arguments forwarded to the constructor of Exc
     * @param args arguments forwarded to the constructor of Exc
     * @return never returns (throws an exception)
     */
template <class Exc, typename... Args>
[[noreturn]]
void
throw_with_backtrace(Args&&... args) {
    using exc_type = std::decay_t<Exc>;
    static_assert(std::is_base_of<std::exception, exc_type>::value,
            "throw_with_backtrace only works with exception types");
    throw internal::backtraced<exc_type>(std::forward<Args>(args)...);
};

}
