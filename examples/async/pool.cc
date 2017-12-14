#include <yamail/resource_pool/async/pool.hpp>

#include <fstream>
#include <iostream>
#include <thread>
#include <memory>

using ofstream_pool = yamail::resource_pool::async::pool<std::unique_ptr<std::ofstream>>;
using time_traits = yamail::resource_pool::time_traits;

struct on_get {
    void operator ()(const boost::system::error_code& ec, ofstream_pool::handle handle) {
        try {
            if (ec) {
                std::cerr << ec.message() << std::endl;
                return;
            }
            std::cout << "got resource handle" << std::endl;
            if (handle.empty()) {
                std::unique_ptr<std::ofstream> file(new std::ofstream("pool.log", std::ios::app));
                if (!file->good()) {
                    std::cout << "open file pool.log error: " << file->rdstate() << std::endl;
                    return;
                }
                handle.reset(std::move(file));
            }
            *(handle.get()) << (time_traits::time_point::min() - time_traits::now()).count() << std::endl;
            if (handle.get()->good()) {
                handle.recycle();
            }
        } catch (const std::exception& exception) {
            std::cerr << exception.what() << std::endl;
            return;
        }
    }
};

int main() {
    boost::asio::io_service service;
    ofstream_pool pool(1, 10);
    pool.get_auto_waste(service, on_get(), time_traits::duration::max());
    std::thread worker([&] { return service.run(); });
    worker.join();
    return 0;
}
