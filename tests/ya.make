GTEST()

OWNER(
    dskut
)

NO_UTIL()

SRCS(
    main.cc
    error.cc
    handle.cc
    time_traits.cc
    sync/pool.cc
    sync/pool_impl.cc
    async/pool.cc
    async/pool_impl.cc
    async/queue.cc
)

PEERDIR(
    mail/github/resource_pool
    contrib/libs/gtest
    contrib/libs/gmock
)

END()
