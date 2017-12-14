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
cmake ..
make -j $(nproc)
ctest -V
examples/sync_pool
examples/async_pool
```

## Install

Include as subdirectory into your CMake project or copy folder include.

## Usage

### Handle

The handle contains iterator to ```boost::optional``` of resource value in pool.
Declared as type [handle](include/yamail/resource_pool/handle.hpp#L11-L56).
Constructs with one of strategies that uses in destructor:
* waste -- resets iterator if handle is usable.
* recycle -- returns iterator to pool if handle is usable.

Pool contains slots for resources that means handle iterator may refers to empty ```boost::optional```.
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
void reset(value_type&& res);
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

Based on ```std::condition_variable```.

#### Create pool

Use type [sync::pool](include/yamail/resource_pool/sync/pool.hpp#L14-L65). Parametrize resource type:
```c++
template <class Value
          class Impl = detail::pool_impl<Value, std::condition_variable> >
class pool;
```

Pool holds ```boost::optional``` of resource type.

Example:
```c++
using fstream_pool = pool<std::fstream>;
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

#### Get handle

Use one of these methods:
```c++
std::pair<boost::system::error_code, handle> get_auto_waste(
    time_traits::duration wait_duration = time_traits::duration(0)
);
```
returns resource handle when it will be available with auto waste strategy.

```c++
std::pair<boost::system::error_code, handle> get_auto_recycle(
    time_traits::duration wait_duration = time_traits::duration(0)
);
```
returns resource handle when it will be available with auto recycle strategy.

Recommends to use ```get_auto_waste``` and explicit call ```recycle```.

Example:
```c++
auto r = pool.get(time_traits::duration(1));
const auto& ec = r.first;
if (ec) {
    std::cerr << "Can't get resource: " << ec.message() << std::endl;
    return;
}
auto& h = r.second;
if (h.empty()) {
    h.reset(create_resource());
}
use_resource(h.get());
```

### Asynchronous pool

Based on ```boost::asio::io_service```. Uses async queue with deadline timer to store waiting resources requests.

#### Create pool

Use type [async::pool](include/yamail/resource_pool/async/pool.hpp#L34-L105). Parametrize resource type:
```c++
template <class Value,
          class IoService = boost::asio::io_service,
          class Impl = default_pool_impl<Value, IoService>::type>
class pool;
```

Pool holds ```boost::optional``` of resource type.

Example:
```c++
using fstream_pool = pool<std::fstream>;
```

Object constructing requires reference to io service, capacity of pool, queue capacity:
```c++
pool(
    io_service_t& io_service,
    std::size_t capacity,
    std::size_t queue_capacity,
    time_traits::duration idle_timeout = time_traits::duration::max()
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
    Callback call,
    const time_traits::duration& wait_duration = time_traits::duration(0)
);
```
returns resource handle when it will be available with auto waste strategy.

```c++
template <class Callback>
void get_auto_recycle(
    Callback call,
    const time_traits::duration& wait_duration = time_traits::duration(0)
);
```
returns resource handle when it will be available with auto recycle strategy.

Type ```Callback``` must provide interface:
```c++
void operator ()(
    const boost::system::error_code&,
    handle
);
```
or it can be any valid completion token from **Boost.Asio** like ```boost::asio::yield_context```, ```boost::asio::use_future``` and so on.

Recommends to use ```get_auto_waste``` and explicit call ```recycle```.

If error occurs ```ec``` will be not ok and ```handle``` will be unusable.

Example with classic callbacks:
```c++
struct on_create_resource {
    handle h;

    on_create_resource(handle h) : h(std::move(h)) {}

    void operator ()(const boost::system::error_code& ec, handle::value_type&& r) {
        if (ec) {
            std::cerr << "Can't create resource: " << ec.message() << std::endl;
            return;
        }
        h.reset(std::move(r));
        use_resource(h.get());
    }
};

struct handle_get {
    void operator ()(const boost::system::error_code& ec, handle h) {
        if (ec) {
            std::cerr << "Can't get resource: " << ec.message() << std::endl;
            return;
        }
        if (h.empty()) {
            async_create_resource(on_create_resource(std::move(h)));
        } else {
            use_resource(h.get());
        }
    }
};

pool.get(handle_get(), time_traits::duration(1));
```

Example with Boost.Coroutines:
```c++
boost::asio::spawn(io, [&](boost::asio::yield_context yield) {
    auto h = pool.get(yield, time_traits::duration(1));
    if (h.empty()) {
        h.reset(create_resource(yield));
    }
    use_resource(h.get());
}
```

## Examples

Source code can be found in [examples](examples) directory.
