@echo off
:: Sets up the Pigweed environment of this project (configured by pigweed.json
:: in this folder) in third_party\pigweed\environment and activates it in the
:: current console. Use activate.bat to only activate an already set up
:: environment.
for %%i in ("%~dp0.") do set "PW_PROJECT_ROOT=%%~fi"
for %%i in ("%~dp0..\third_party\pigweed\environment") do set "PW_ENVIRONMENT_ROOT=%%~fi"
call "%~dp0..\third_party\pigweed\bootstrap.bat"
