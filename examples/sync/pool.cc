#include <yamail/resource_pool/sync/pool.hpp>

#include <fstream>
#include <iostream>
#include <memory>

typedef yamail::resource_pool::sync::pool<std::unique_ptr<std::ofstream>> ofstream_pool;
typedef yamail::resource_pool::time_traits time_traits;

int main() {
    ofstream_pool pool(1);
    auto res = pool.get_auto_recycle(time_traits::duration::max());
    auto& ec = res.first;
    if (ec) {
        std::cerr << ec.message() << std::endl;
        return -1;
    }
    auto& handle = res.second;
    if (handle.empty()) {
        std::unique_ptr<std::ofstream> file;
        try {
            file.reset(new std::ofstream("pool.log", std::ios::app));
        } catch (const std::exception& exception) {
            std::cerr << "Open file pool.log error: " << exception.what() << std::endl;
            return -1;
        }
        handle.reset(std::move(file));
    }
    *(handle.get()) << (time_traits::time_point::min() - time_traits::now()).count() << std::endl;
    return 0;
}
