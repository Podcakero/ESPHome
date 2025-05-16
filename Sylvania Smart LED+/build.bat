@echo off

setlocal EnableDelayedExpansion

set data=
dir devices /s/b | findstr /V base\.yaml$ | findstr /V secrets\.yaml$ | findstr /V \.esphome | findstr \.yaml > tmp.txt
for /f "delims=" %%i in (tmp.txt) do (
    set data=!data! "%%i"
)

esphome -q clean %data%
esphome -q compile %data%