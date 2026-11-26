from conan import ConanFile


class KickcamDev(ConanFile):
    """Local development conanfile -- installs dependencies based on build options.

    Options mirror those in scripts/configure.sh and CMakeLists.txt.
    setup_build.sh passes them via -o when calling conan install.
    """
    settings = "os", "compiler", "build_type", "arch"

    options = {
        "unit_tests": [True, False],
    }
    default_options = {
        "unit_tests": False,
    }

    generators = "CMakeDeps"

    def requirements(self):
        self.requires("tinyxml2/11.0.0")
        if self.options.unit_tests:
            self.requires("gtest/1.15.0")

    def configure(self):
        if self.options.unit_tests:
            self.options["gtest"].build_gmock = True
