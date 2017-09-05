#include "myptr.hh"
#include "core/shared_ptr_impl.hh"

namespace seastar {

struct dummy {
    int x = 6;
};

class my_thing : public dummy, public seastar::enable_lw_shared_from_this<my_thing> {
public:
    int y = 3;
};

my_ptr make_my_thing() {
    return seastar::make_lw_shared<my_thing>();
}

void lw_shared_ptr_deleter<my_thing>::dispose(my_thing* x) {
    delete x;
}

}
