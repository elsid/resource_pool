# Resource pool

[![Build Status](https://travis-ci.org/elsid/resource_pool.svg?branch=master)](https://travis-ci.org/elsid/resource_pool)
[![codecov](https://codecov.io/gh/elsid/resource_pool/branch/master/graph/badge.svg)](https://codecov.io/gh/elsid/resource_pool)

Header only library purposed to create pool of some resources like keep-alive connections.
Supports sync and async interfaces. Based on boost.

## Dependencies

* **CMake** >= 3.12
* **GCC** or **Clang** C++ compilers with C++17 support.
* **Boost** >= 1.66

## Build

Project uses CMake. Build need only to run tests, examples and benchmarks:
```bash
mkdir build
cd build
cmake ..
make -j $(nproc)
ctest -V
examples/sync_pool
examples/async_pool
benchmarks/resource_pool_benchmark_async
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

Object construction requires pool capacity:
```c++
pool(
    std::size_t capacity,
    time_traits::duration idle_timeout = time_traits::duration::max(),
    time_traits::duration lifespan = time_traits::duration::max()
);
```

* `idle_timeout` defines maximum time interval to keep unused resource in the pool.
  Check for elapsed time happens on resource allocation.
* `lifespan` defines maximum time interval to keep resource.
  Check for elapsed time happens on resource allocation and recycle.

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

#### Refresh pool

Following method allows to force all available and used handles to be wasted:
```c++
void refresh();
```

All currently available but not used handles will be wasted. All currently used handles will be wasted on return to the pool.

### Asynchronous pool

Based on ```boost::asio::io_context```. Uses async queue with deadline timer to store waiting resources requests.

#### Create pool

Use type [async::pool](include/yamail/resource_pool/async/pool.hpp#L34-L105). Parametrize resource type:
```c++
template <class Value,
          class IoContext = boost::asio::io_context,
          class Impl = default_pool_impl<Value, IoContext>::type>
class pool;
```

Pool holds ```boost::optional``` of resource type.

Example:
```c++
using fstream_pool = pool<std::fstream>;
```

Object construction requires reference to io service, capacity of pool, queue capacity:
```c++
pool(
    io_context_t& io_context,
    std::size_t capacity,
    std::size_t queue_capacity,
    time_traits::duration idle_timeout = time_traits::duration::max(),
    time_traits::duration lifespan = time_traits::duration::max()
);
```

* `idle_timeout` defines maximum time interval to keep unused resource in the pool.
  Check for elapsed time happens on resource allocation.
* `lifespan` defines maximum time interval to keep resource.
  Check for elapsed time happens on resource allocation and recycle.

Example:
```c++
boost::asio::io_context io;
fstream_pool pool(io, 13, 42);
```

#### Get handle

Use one of these methods:
```c++
template <class CompletionToken>
void get_auto_waste(
    boost::asio::io_context& io,
    CompletionToken&& token,
    const time_traits::duration& wait_duration = time_traits::duration(0)
);
```
returns resource handle when it will be available with auto waste strategy.

```c++
template <class CompletionToken>
void get_auto_recycle(
    boost::asio::io_context& io,
    CompletionToken&& token,
    const time_traits::duration& wait_duration = time_traits::duration(0)
);
```
returns resource handle when it will be available with auto recycle strategy.

Type ```CompetionToken``` must provide interface:
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

#### Refresh pool

Following method allows to force all available and used handles to be wasted:
```c++
void refresh();
```

All currently available but not used handles will be wasted. All currently used handles will be wasted on return to the pool.

## Examples

Source code can be found in [examples](examples) directory.
