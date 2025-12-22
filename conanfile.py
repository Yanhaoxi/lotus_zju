from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.build import check_min_cppstd
from os.path import join
import os

required_conan_version = ">=2.0"

class LotusConan(ConanFile):
    name = "lotus"
    version = "0.1.0"
    license = "MIT"
    author = "Lotus Team"
    url = "https://github.com/ZJU-PL/lotus"
    description = "Lotus is a static analysis tool for C/C++ code"
    topics = ("static-analysis", "security", "llvm")
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "build_tests": [True, False],
        "build_examples": [True, False],
        "enable_clam": [True, False],
        "enable_svf": [True, False],
        "enable_seahorn": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "build_tests": False,
        "build_examples": True,
        "enable_clam": True,
        "enable_svf": False,
        "enable_seahorn": True,
    }
    
    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC
    
    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
    
    def requirements(self):
        # Core dependencies
        # Note: Z3 4.11+ is required, using 4.12.2 which is compatible
        self.requires("z3/4.12.2")
        # Boost 1.65+ is required
        self.requires("boost/1.84.0")
        # GMP is required for some components
        self.requires("gmp/6.3.0")
        
        # Test dependencies (optional)
        if self.options.build_tests:
            self.requires("gtest/1.14.0")
    
    def build_requirements(self):
        self.tool_requires("cmake/[>=3.10]")
    
    def validate(self):
        check_min_cppstd(self, "14")
    
    def layout(self):
        cmake_layout(self)
    
    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        
        # Set CMake options based on Conan options
        tc.variables["BUILD_TESTS"] = self.options.build_tests
        tc.variables["BUILD_EXAMPLES"] = self.options.build_examples
        tc.variables["ENABLE_CLAM"] = self.options.enable_clam
        tc.variables["ENABLE_SVF"] = self.options.enable_svf
        tc.variables["ENABLE_SEAHORN"] = self.options.enable_seahorn
        
        # Disable auto-download of dependencies since Conan manages them
        tc.variables["DOWNLOAD_BOOST"] = False
        tc.variables["DOWNLOAD_CRAB"] = False
        
        # Set C++ standard
        tc.variables["CMAKE_CXX_STANDARD"] = "14"
        tc.variables["CMAKE_CXX_STANDARD_REQUIRED"] = "ON"
        
        # Handle fPIC
        if self.options.get_safe("fPIC"):
            tc.variables["CMAKE_POSITION_INDEPENDENT_CODE"] = True
        
        # LLVM configuration - try to find system LLVM
        # Note: LLVM is typically not available via Conan, so we rely on system installation
        # Users should set LLVM_BUILD_PATH or LLVM_DIR if needed
        if "LLVM_BUILD_PATH" in os.environ:
            tc.variables["LLVM_BUILD_PATH"] = os.environ["LLVM_BUILD_PATH"]
        if "LLVM_DIR" in os.environ:
            tc.variables["LLVM_DIR"] = os.environ["LLVM_DIR"]
        if "LLVM_CONFIG_PATH" in os.environ:
            tc.variables["LLVM_CONFIG_PATH"] = os.environ["LLVM_CONFIG_PATH"]
        
        # Z3 configuration - use Conan's Z3
        tc.variables["Z3_DIR"] = self.dependencies["z3"].package_folder
        
        # Boost configuration - use Conan's Boost
        tc.variables["CUSTOM_BOOST_ROOT"] = self.dependencies["boost"].package_folder
        
        tc.generate()
    
    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
    
    def package(self):
        cmake = CMake(self)
        cmake.install()
    
    def package_info(self):
        self.cpp_info.libs = ["lotus"]
        self.cpp_info.includedirs = ["include"]
        
        # Add runtime path for shared libraries
        if self.options.shared:
            self.runenv_info.define_path("LD_LIBRARY_PATH", 
                                         join(self.package_folder, "lib"))
        
        # Ensure binaries are in PATH
        self.env_info.PATH.append(join(self.package_folder, "bin"))