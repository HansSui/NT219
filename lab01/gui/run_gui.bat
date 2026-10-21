@echo off
setlocal
cd /d "%~dp0.."
python gui\aestool_gui.py %*
