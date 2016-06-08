# Resource pool

[![Build Status](https://travis-ci.org/elsid/resource_pool.svg?branch=master)](https://travis-ci.org/elsid/resource_pool)
[![Coverage Status](https://coveralls.io/repos/github/elsid/resource_pool/badge.svg?branch=master)](https://coveralls.io/github/elsid/resource_pool?branch=master)

Header only library purposed to create pool of some resources like keep-alive connections.
Supports sync and async interfaces. Based on boost.

## Build

Project uses CMake. Build need only to run tests or examples:
```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j $(nproc)
ctest -V -j $(nproc)
examples/sync_pool
examples/async_pool
```

## Install

Include as subdirectory into your CMake project or copy folder include.

## Usage

### Handle

The wrapper contains ```boost::shared_ptr``` of resource type.
Declared as type [handle](include/yamail/resource_pool/handle.hpp#L12-L59).
Constructs with one of strategies that uses in destructor:
* waste -- resets ```boost::shared_ptr``` value if handle is usable.
* recycle -- returns resource to pool if handle is usable.

Pool contains slots for resources that means handle may contains pointer to some client object or ```nullptr```.
Client always must check value before using by method:
```c++
bool empty() const;
```

Access to value provides by methods:
```c++
value_type& get();
const value_type& get() const;
value_type *operator ->();
const value_type *operator ->() const;
value_type &operator *();
const value_type &operator *() const;
```

```value_type``` -- resource type.

Value of handle changes by method:
```c++
void reset(const boost::shared_ptr<value_type>& res);
```

To drop resource use method:
```c++
void waste();
```

To return resource into pool use method:
```c++
void recycle();
```

Both methods makes handle unusable that could be checked by method:
```c++
bool unusable() const;
```

Calling one of these methods for unusable handle throws an exception ```error::unusable_handle```.

### Synchronous pool

Based on ```boost::condition_variable```.

#### Create pool

Use type [sync::pool](include/yamail/resource_pool/sync/pool.hpp#L14-L54). Parametrize resource type:
```c++
template <class Value
          class Impl = detail::pool_impl<Value, boost::condition_variable> >
class pool;
```

Pool holds ```boost::shared_ptr``` of resource type. Type wrapping isn't necessary.

Example:
```c++
typedef pool<std::fstream> fstream_pool;
```

Object constructing requires capacity of pool:
```c++
pool(
    std::size_t capacity,
    time_traits::duration idle_timeout = time_traits::duration::max()
);
```

Example:
```c++
fstream_pool pool(42);
```

#### Specific handle

Declared as type [sync::handle](include/yamail/resource_pool/sync/handle.hpp#L14-L33)
based on type [handle](include/yamail/resource_pool/handle.hpp#L12-L59).
Contains extra method to get error code:
```c++
const boost::system::error_code& error() const;
```

If new handle is not usable error value will be not ok.

#### Get handle

Use one of these methods:
```c++
boost::shared_ptr<handle> get_auto_waste(
    time_traits::duration wait_duration = time_traits::duration(0)
);
```
returns resource handle when it will be available with auto waste strategy.

```c++
boost::shared_ptr<handle> get_auto_recycle(
    time_traits::duration wait_duration = time_traits::duration(0)
);
```
returns resource handle when it will be available with auto recycle strategy.

Recommends to use ```get_auto_waste``` and explicit call ```recycle```.

Example:
```c++
boost::shared<handle> h = pool.get(time_traits::duration(1));
if (h->error()) {
    std::cerr << "Cant't get resource: " << h->error().message() << std::endl;
    return;
}
if (h->empty()) {
    h->reset(create_resource());
}
use_resource(h.get());
```

### Asynchronous pool

Based on ```boost::asio::io_service```. Uses async queue with deadline timer to store waiting resources requests.

#### Create pool

Use type [async::pool](include/yamail/resource_pool/async/pool.hpp#L36-L124). Parametrize resource type:
```c++
template <class Value,
          class IoService = boost::asio::io_service,
          class Impl = default_pool_impl<Value, IoService>::type>
class pool;
```

Pool holds ```boost::shared_ptr``` of resource type. Type wrapping isn't necessary.

Example:
```c++
typedef pool<std::fstream> fstream_pool;
```

Object constructing requires reference to io service, capacity of pool, queue capacity:
```c++
pool(
    io_service_t& io_service,
    std::size_t capacity,
    std::size_t queue_capacity,
    time_traits::duration idle_timeout = time_traits::duration::max(),
    const on_catch_handler_exception_type& on_catch_handler_exception = detail::abort()
);
```

Example:
```c++
boost::asio::io_service ios;
fstream_pool pool(ios, 13, 42);
```

#### Get handle

Use one of these methods:
```c++
template <class Callback>
void get_auto_waste(
    const Callback& call,
    const time_traits::duration& wait_duration = time_traits::duration(0)
);
```
returns resource handle when it will be available with auto waste strategy.

```c++
template <class Callback>
void get_auto_recycle(
    const Callback& call,
    const time_traits::duration& wait_duration = time_traits::duration(0)
);
```
returns resource handle when it will be available with auto recycle strategy.

Type ```Callback``` must provide interface:
```c++
void operator ()(
    const boost::system::error_code&,
    const boost::shared_ptr<handle>&
);
```

Recommends to use ```get_auto_waste``` and explicit call ```recycle```.

If error occurs ```ec``` will be not ok and ```handle``` will be nullptr.

Example:
```c++
struct on_create_resource {
    boost::shared_ptr<handle> h;

    on_create_resource(const boost::shared_ptr<handle>& h) : h(h) {}

    void operator ()(const boost::system::error_code& ec, const handle::pointer& r) {
        if (ec) {
            std::cerr << "Cant't create resource: " << ec.message() << std::endl;
            return;
        }
        h->reset(r);
        use_resource(h->get());
    }
};

struct handle_get {
    void operator ()(const boost::system::error_code& ec,
                     const boost::shared_ptr<handle>& h) {
        if (ec) {
            std::cerr << "Cant't get resource: " << ec.message() << std::endl;
            return;
        }
        if (h->empty()) {
            async_create_resource(on_create_resource(h));
        } else {
            use_resource(h->get());
        }
    }
};

pool.get(handle_get(), time_traits::duration(1));
```
