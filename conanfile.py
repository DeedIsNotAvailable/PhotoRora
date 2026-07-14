from conan import ConanFile


class Application(ConanFile):
    settings = "os", "compiler", "arch", "build_type"
    generators = "PkgConfigDeps"
    default_options = {
        "onnxruntime/*:shared": True,
        "onnxruntime/*:with_cuda": False,
        "onnxruntime/*:with_xnnpack": False,
        "flatbuffers/*:shared": False,
        "flatbuffers/*:header_only": True,
    }

    requires = (
        "onnxruntime/1.17.3@aurora",
    )

    def configure(self):
        self.options["onnxruntime"].shared = True
        self.options["onnxruntime"].with_cuda = False
        self.options["onnxruntime"].with_xnnpack = False
        self.options["flatbuffers"].shared = False
        self.options["flatbuffers"].header_only = True
