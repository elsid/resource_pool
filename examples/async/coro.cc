#include <yamail/resource_pool/async/pool.hpp>

#include <fstream>
#include <iostream>
#include <thread>
#include <memory>

typedef yamail::resource_pool::async::pool<std::unique_ptr<std::ofstream>> ofstream_pool;
typedef yamail::resource_pool::time_traits time_traits;

int main() {
    boost::asio::io_service service;
    ofstream_pool pool(1, 10);
    boost::asio::spawn(service, [&](boost::asio::yield_context yield){
        boost::system::error_code ec;
        auto handle = pool.get_auto_waste(service, yield[ec], time_traits::duration::max());
        std::cerr << ec.message() << std::endl;
        return;
        if (handle.empty()) {
            std::unique_ptr<std::ofstream> file;
            try {
                file.reset(new std::ofstream("pool.log", std::ios::app));
            } catch (const std::exception& exception) {
                std::cerr << "Open file pool.log error: " << exception.what() << std::endl;
                return;
            }
            handle.reset(std::move(file));
        }
        *(handle.get()) << (time_traits::time_point::min() - time_traits::now()).count() << std::endl;
        handle.recycle();
    });
    service.run();
    return 0;
}
