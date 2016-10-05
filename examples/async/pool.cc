#include <yamail/resource_pool/async/pool.hpp>

#include <fstream>
#include <thread>

typedef yamail::resource_pool::async::pool<std::ofstream> ofstream_pool;
typedef yamail::resource_pool::time_traits time_traits;

struct on_get {
    void operator ()(const boost::system::error_code& ec, ofstream_pool::handle handle) {
        if (ec) {
            std::cerr << ec.message() << std::endl;
            return;
        }
        if (handle.empty()) {
            std::shared_ptr<std::ofstream> file;
            try {
                file = std::make_shared<std::ofstream>("pool.log", std::ios::app);
            } catch (const std::exception& exception) {
                std::cerr << "Open file pool.log error: " << exception.what() << std::endl;
                return;
            }
            handle.reset(file);
        }
        handle.get() << (time_traits::time_point::min() - time_traits::now()).count() << std::endl;
        handle.recycle();
    }
};

int main() {
    boost::asio::io_service service;
    boost::asio::io_service::work work(service);
    std::thread worker([&] { return service.run(); });
    ofstream_pool pool(service, 1, 10);
    pool.get_auto_waste(on_get(), time_traits::duration::max());
    service.stop();
    worker.join();
    return 0;
}
