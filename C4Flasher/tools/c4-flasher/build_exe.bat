@echo off
chcp 65001 >nul
cd /d "%~dp0"

set "OUTPUT_DIR=..\.."
set "OUTPUT_EXE=%OUTPUT_DIR%\C4Flasher.exe"

where python >nul 2>&1
if errorlevel 1 (
    echo 未找到 Python，请先安装 Python 3.8+
    pause
    exit /b 1
)

python -m pip install -r requirements.txt -q
python -m PyInstaller --noconfirm --distpath "%OUTPUT_DIR%" --workpath build C4Flasher.spec

if exist "%OUTPUT_EXE%" (
    copy /Y config.json "%OUTPUT_DIR%\config.json" >nul
    echo.
    echo 已生成: %OUTPUT_EXE%
    echo 已复制: %OUTPUT_DIR%\config.json
    echo.
    echo 内置 pyOCD（已禁用 CMSIS Pack 管理器）
    echo 使用方式: 双击 C4Flasher\C4Flasher.exe
) else (
    echo 打包失败
)

pause
