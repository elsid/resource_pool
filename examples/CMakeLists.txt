set(EXAMPLE_FLAGS -Wall -Wextra -pedantic -Werror)

find_package(Boost REQUIRED COMPONENTS context system coroutine date_time atomic)

add_executable(async_pool "async/pool.cc")
target_link_libraries(async_pool yandex::resource_pool)
target_compile_options(async_pool PRIVATE ${EXAMPLE_FLAGS})

add_executable(coro_pool "async/coro.cc")
target_link_libraries(coro_pool yandex::resource_pool Boost::coroutine Boost::context)
target_compile_options(coro_pool PRIVATE ${EXAMPLE_FLAGS})

add_executable(async_strand "async/strand.cc")
target_link_libraries(async_strand yandex::resource_pool)
target_compile_options(async_strand PRIVATE ${EXAMPLE_FLAGS})

add_executable(sync_pool "sync/pool.cc")
target_link_libraries(sync_pool yandex::resource_pool)
target_compile_options(sync_pool PRIVATE ${EXAMPLE_FLAGS})
