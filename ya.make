LIBRARY()

OWNER(
    dskut
)

NO_UTIL()

PEERDIR(
    contrib/libs/boost
)

ADDINCL(GLOBAL mail/github/resource_pool/include)

CFLAGS(
    GLOBAL -DBOOST_COROUTINES_NO_DEPRECATION_WARNING
)

END()

RECURSE(
    tests
)
