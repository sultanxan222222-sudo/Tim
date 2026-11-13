from pathlib import Path
import unittest

PATCH = Path(__file__).resolve().parents[1] / "flycast-mali-gles2-compat.patch"

class PatchContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.patch = PATCH.read_text(encoding="utf-8")

    def test_fragment_precision_is_capability_gated(self):
        self.assertIn("#if TARGET_GL == GLES2 && HIGH_PRECISION_FLOAT == 0", self.patch)
        self.assertIn('#define highp mediump', self.patch)
        self.assertIn('addConstant("HIGH_PRECISION_FLOAT", gl.highp_float_supported ? 1 : 0);', self.patch)

    def test_compile_errors_fail_fast_and_log_source(self):
        self.assertIn("glDeleteShader(rv);", self.patch)
        self.assertIn('INFO_LOG(RENDERER, "// %s SHADER SOURCE', self.patch)

    def test_link_failure_always_cleans_up(self):
        self.assertIn("if (result != GL_TRUE)", self.patch)
        self.assertIn("glDeleteProgram(program);", self.patch)
        self.assertIn('"driver returned no linker log"', self.patch)

    def test_null_driver_strings_are_safe(self):
        for line in (
            'if (extensions == nullptr) extensions = "";',
            'if (vendor == nullptr) vendor = "unknown";',
            'if (renderer == nullptr) renderer = "unknown";',
            'if (version == nullptr) version = "unknown";',
        ):
            self.assertIn(line, self.patch)

if __name__ == "__main__":
    unittest.main(verbosity=2)
