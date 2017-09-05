#pragma once

#include "core/shared_ptr.hh"

namespace seastar {
namespace internal {

template<typename T>
T* lw_shared_ptr_deleter_accessors<T>::to_value(seastar::lw_shared_ptr_counter_base* counter) {
    return static_cast<T*>(counter);
}

}
}
