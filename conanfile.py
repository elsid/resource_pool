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

    generators = 'cmake_paths'
    requires = 'boost/1.71.0@conan/stable'

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
        self.cpp_info.libs = ['resource_pool']
