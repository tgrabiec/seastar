#pragma once

#include "core/shared_ptr.hh"

namespace seastar {

class my_thing;

template<>
struct lw_shared_ptr_deleter<my_thing> {
    static void dispose(my_thing*);
};

using my_ptr = seastar::lw_shared_ptr<my_thing>;

my_ptr make_my_thing();

}
