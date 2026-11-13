from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "apply_patch.py"


class PatchContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.script = SCRIPT.read_text(encoding="utf-8")

    def test_fragment_precision_is_capability_gated(self):
        self.assertIn("#if TARGET_GL == GLES2 && HIGH_PRECISION_FLOAT == 0", self.script)
        self.assertIn("#define highp mediump", self.script)
        self.assertIn('addConstant("HIGH_PRECISION_FLOAT", gl.highp_float_supported ? 1 : 0);', self.script)

    def test_compiler_fails_fast_and_cleans_up(self):
        self.assertIn("glCreateShader failed", self.script)
        self.assertIn("glDeleteShader(rv);", self.script)
        self.assertIn("glDeleteProgram(program);", self.script)
        self.assertIn("driver returned no linker log", self.script)

    def test_missing_gl_strings_are_safe(self):
        for value in ('vendor = "unknown"', 'renderer = "unknown"', 'version = "unknown"'):
            self.assertIn(value, self.script)

    def test_pinned_commit_is_enforced(self):
        self.assertIn('if head != PINNED_COMMIT:', self.script)
        self.assertIn("72b8e67141d85ca5c9259f04563b49f7f92f928b", self.script)


if __name__ == "__main__":
    unittest.main(verbosity=2)
