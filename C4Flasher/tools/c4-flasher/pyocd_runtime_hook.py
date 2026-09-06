# PyInstaller runtime hook: disable CMSIS pack manager in frozen exe.
import io
import os
import sys

os.environ["PYOCD_DISABLE_CMSIS_PACK"] = "1"

# windowed exe 没有控制台，sys.stdout 可能为 None，会导致 pyOCD 进度条崩溃
if sys.stdout is None:
    sys.stdout = io.StringIO()
if sys.stderr is None:
    sys.stderr = io.StringIO()
