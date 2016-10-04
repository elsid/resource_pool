#include <yamail/resource_pool/sync/pool.hpp>

#include <fstream>
#include <iostream>

typedef yamail::resource_pool::sync::pool<std::ofstream> ofstream_pool;
typedef yamail::resource_pool::time_traits time_traits;

int main() {
    ofstream_pool pool(1);
    ofstream_pool::handle_ptr handle = pool.get_auto_recycle(time_traits::duration::max());
    if (handle->error()) {
        std::cerr << handle->error().message() << std::endl;
        return -1;
    }
    if (handle->empty()) {
        std::shared_ptr<std::ofstream> file;
        try {
            file = std::make_shared<std::ofstream>("pool.log", std::ios::app);
        } catch (const std::exception& exception) {
            std::cerr << "Open file pool.log error: " << exception.what() << std::endl;
            return -1;
        }
        handle->reset(file);
    }
    handle->get() << (time_traits::time_point::min() - time_traits::now()).count() << std::endl;
    return 0;
}
