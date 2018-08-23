#include <yamail/resource_pool/async/pool.hpp>

#include <fstream>
#include <iostream>
#include <thread>
#include <memory>

using ofstream_pool = yamail::resource_pool::async::pool<std::unique_ptr<std::ofstream>>;
using time_traits = yamail::resource_pool::time_traits;

struct on_get {
    using executor_type = boost::asio::io_context::strand;

    boost::asio::io_context::strand& strand;

    void operator ()(const boost::system::error_code& ec, ofstream_pool::handle handle) {
        try {
            if (ec) {
                std::cerr << ec.message() << std::endl;
                return;
            }
            std::cout << "got resource handle" << std::endl;
            if (handle.empty()) {
                auto file = std::make_unique<std::ofstream>("pool.log", std::ios::app);
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

    executor_type get_executor() const noexcept {
        return strand;
    }
};

int main() {
    boost::asio::io_context service;
    boost::asio::io_context::strand strand(service);
    ofstream_pool pool(2, 10);
    pool.get_auto_waste(service, on_get {strand}, time_traits::duration::max());
    pool.get_auto_waste(service, on_get {strand}, time_traits::duration::max());
    pool.get_auto_waste(service, on_get {strand}, time_traits::duration::max());
    std::thread worker1([&] { return service.run(); });
    std::thread worker2([&] { return service.run(); });
    worker1.join();
    worker2.join();
    return 0;
}
