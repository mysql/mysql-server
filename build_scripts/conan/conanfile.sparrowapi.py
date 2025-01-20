from conans import ConanFile, tools


class SparrowapiConan(ConanFile):
    name = "sparrowapi"
    version = "5.6.46-42010-SPW-000"
    settings = "os", "compiler", "build_type", "arch"
    description = "Sparrow client API"
    url = "None"
    license = "None"
    author = "None"
    topics = None

    def package(self):
        self.copy("*")

    def package_info(self):
        self.cpp_info.libs = tools.collect_libs(self)
