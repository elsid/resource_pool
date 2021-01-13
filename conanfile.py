from conans import ConanFile, CMake
from conans.tools import load
import re


def get_version():
    try:
        content = load("CMakeLists.txt")
        version = re.search(r"^\s*project\(resource_pool\s+VERSION\s+([^\s]+)", content, re.M).group(1)
        return version.strip()
    except Exception:
        return None


class ResourcePool(ConanFile):
    name = 'resource_pool'
    version = get_version()
    license = 'MIT'
    url = 'https://github.com/elsid/resource_pool'
    description = 'Conan package for elsid resource pool'

    exports_sources = 'include/*', 'CMakeLists.txt', 'resource_poolConfig.cmake', 'LICENCE', 'AUTHORS'

    generators = 'cmake_find_package'
    requires = 'boost/1.74.0'

    def _configure_cmake(self):
        cmake = CMake(self)
        cmake.configure()
        return cmake

    def build(self):
        cmake = self._configure_cmake()
        cmake.build()

    def package(self):
        cmake = self._configure_cmake()
        cmake.install()

    def package_id(self):
        self.info.header_only()

    def package_info(self):
        self.cpp_info.components["_resource_pool"].includedirs = ["include"]
        self.cpp_info.components["_resource_pool"].requires = ["boost::boost"]
        self.cpp_info.components["_resource_pool"].defines = ["BOOST_ASIO_USE_TS_EXECUTOR_AS_DEFAULT"]

        self.cpp_info.filenames["cmake_find_package"] = "resource_pool"
        self.cpp_info.filenames["cmake_find_package_multi"] = "resource_pool"
        self.cpp_info.names["cmake_find_package"] = "elsid"
        self.cpp_info.names["cmake_find_package_multi"] = "elsid"
        self.cpp_info.components["_resource_pool"].names["cmake_find_package"] = "resource_pool"
        self.cpp_info.components["_resource_pool"].names["cmake_find_package_multi"] = "resource_pool"
