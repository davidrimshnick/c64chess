@echo off
echo === C64 Chess Engine - Tool Installation ===
echo This script must be run as Administrator.
echo.

choco install cc65-compiler -y
choco install winvice-nightly -y
choco install mingw -y
choco install make -y

echo.
echo === Verifying installations ===
cl65 --version
x64sc --version
gcc --version
make --version

echo.
echo === Done! Close and reopen your terminal for PATH updates. ===
pause
