@echo off
setlocal
cd /d "%~dp0.."
if exist "%~dp0..\bin\aescore.dll" set "AESGUI_LIBRARY=%~dp0..\bin\aescore.dll"
python gui\aestool_gui.py %*
