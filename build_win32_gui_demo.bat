@echo off
setlocal

set BUILD_DIR=build

cmake -S . -B "%BUILD_DIR%" -G "Visual Studio 18 2026" -A x64 ^
  -DFVIZ_BUILD_EXAMPLES=ON ^
  -DFVIZ_BUILD_TESTS=ON
if errorlevel 1 exit /b %errorlevel%

cmake --build "%BUILD_DIR%" --config Release --target FVizExampleWin32GuiDemo
if errorlevel 1 exit /b %errorlevel%

echo.
echo Built: %BUILD_DIR%\bin\FEAVizWin32GuiDemo.exe
endlocal
