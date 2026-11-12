@echo off
rem NXNative Windows Builder: pick NSP files and prod.keys, then Build IPA File
cd /d "%~dp0"
start "" pythonw "%~dp0win\gui.pyw"
