@echo off
:: Activates the Pigweed environment of this project in the current console
:: (see bootstrap.bat).
for %%i in ("%~dp0.") do set "PW_PROJECT_ROOT=%%~fi"
for %%i in ("%~dp0..\third_party\pigweed\environment") do set "PW_ENVIRONMENT_ROOT=%%~fi"
:: Don't run `pw doctor` on activation, see bootstrap.sh.
set "PW_ACTIVATE_SKIP_CHECKS=1"
call "%~dp0..\third_party\pigweed\activate.bat"
