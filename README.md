resource_pool
==========

# Описание

Библиотека для создания синхронных и асинхронных пулов ресурсов.

## Синхронный пул

## Асинхронный пул

Работает через boost::asio::io_service, содержит очередь запросов с таймаутами.

### Создание пула

Тип [async::pool](include/yamail/resource_pool/async/pool.hpp#L18-L102) параметризуется типом ресурса:
```c++
template <class T>
class pool;
```

В пуле ресурс хранится под ```boost::shared_ptr```, поэтому нет смысла оборачивать в него тип.

Пример:
```c++
typedef pool<std::fstream> fstream_pool;
```

Объект класса полностью определяет пул, для его создания нужен готовый объект
```boost::asio::io_service```, определение ёмкости пула и очереди запросов.
```c++
pool<T>::pool(
    boost::asio::io_service& io_service,
    std::size_t capacity,
    std::size_t queue_capacity
);
```

Пример:
```c++
boost::asio::io_service ios;
fstream_pool pool(ios, 13, 42);
```

### Запрос ресурса

Ресурс запрашивается через методы:
```c++
void get_auto_waste(
    boost::function<void (
        const boost::system::error_code&,
        boost::shared_ptr<handle>
    )> call,
    const boost::chrono::steady_clock& wait_duration = seconds(0)
);
```
вызовет ```call```, когда ресурс станет доступен, или наступит таймаут, с
```handle``` со стратегией возврата ресурса непригодным для дальнейшего использования.

```c++
void get_auto_recycle(
    boost::function<void (
        const boost::system::error_code&,
        boost::shared_ptr<handle>
    )> call,
    const boost::chrono::steady_clock& wait_duration = seconds(0)
);
```
вызовет ```call```, когда ресурс станет доступен, или наступит таймаут, с
```handle``` со стратегией возврата ресурса пригодным для дальнейшего использования.

Рекомендуется использовать ```get_auto_waste``` и явно вызывать ```recycle```.

### handle

Пул хранит слоты для ресурсов.
Интерфейс слота реализован с помощью класса
[handle](include/yamail/resource_pool/handle.hpp#L22-L65).
Из только что созданного пула вернется пустой слот.
Это можно проверить методом:
```c++
bool empty() const;
```

Чтобы заполнить слот, нужно вызвать метод:
```c++
void reset(boost::shared_ptr<value_type> res);
```

```value_type``` - тип ресурса, заданный в параметре типа pool.

Получить доступ к ресурсу можно через методы:
```с++
value_type& get();
const value_type& get() const;
value_type *operator ->();
const value_type *operator ->() const;
value_type &operator *();
const value_type &operator *() const;
```

Объект класса создается при получении запрошенного ресурса со
стратегией возврата в зависимости от вызванного метода.
Вернуть ресурс можно принудительно методами:

```c++
void recycle();
```
вернет ресурс в пригодном для дальнейшего использования состоянии;

```c++
void waste();
```
вернет ресурс в состоянии не пригодном для дальнейшего использования.

После возвращения ресурса в пул, ```handle``` станет не пригодным для использования:
```c++
bool unusable() const;
```
вернет ```false```.

Если ресурс не был возвращен, то в деструкторе ```handle``` он будет возвращен в
соответствии со стратегией.

Пример:
```c++
std::shared_ptr<std::string> data = get_data();
pool.get_auto_waste([=] (const boost::system::error_code& err,
                         boost::shared_ptr<fstream_pool::handle> h) {
    if (err) {
        std::cerr << "Error get fstream: " << err.message() << std::endl;
        return;
    }
    if (h->empty()) {
        try {
            h->reset(open_file());
        } catch (const std::exception& e) {
            std::cerr << "Can't fill handle: " << e.what() << std::endl;
            return;
        }
    }
    try {
        h->get() << data << std::endl;
        h->recycle();
    } catch (const std::exception& e) {
        std::cerr << "Can't write to file: " << e.what() << std::endl;
    }
}, boost::chrono::seconds(1));
```
