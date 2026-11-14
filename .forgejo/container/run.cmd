@echo off
rem What the workflow runs in the container: run.cmd restore, build or test.
call C:\BuildTools\Common7\Tools\VsDevCmd.bat -arch=amd64 -no_logo >nul
set VCPKG_ROOT=C:\BuildTools\VC\vcpkg
set VCPKG_DOWNLOADS=C:\cache\vcpkg-downloads
set VCPKG_BINARY_SOURCES=clear;files,C:\cache\vcpkg-archives,readwrite
set X_VCPKG_REGISTRIES_CACHE=C:\cache\vcpkg-registries
set X_VCPKG_ASSET_SOURCES=clear;x-script,pwsh -NoLogo -NoProfile -File C:\b\.forgejo\container\vcpkg-fetch.ps1 {url} {dst};x-block-origin
set VCPKG_DISABLE_METRICS=1
if not exist %VCPKG_DOWNLOADS% mkdir %VCPKG_DOWNLOADS%
if not exist C:\cache\vcpkg-archives mkdir C:\cache\vcpkg-archives
if not exist %X_VCPKG_REGISTRIES_CACHE% mkdir %X_VCPKG_REGISTRIES_CACHE%
set FEED=https://artifacts.home.romaine.life/nuget/terminal-dependencies/v3/index.json
set MSBUILD_ARGS=/m:4 /p:Configuration=Release /p:Platform=x64 /p:WindowsTerminalBranding=Dev
cd /d C:\b
goto %1

:restore
C:\tools\nuget-6.14.0.exe restore build/packages.config -ConfigFile NuGet.Config -Source %FEED% -PackagesDirectory packages || exit /b 1
msbuild src/cascadia/CascadiaPackage/CascadiaPackage.wapproj /t:Restore %MSBUILD_ARGS% "/p:SolutionDir=C:\b\\" /p:RestoreSources=%FEED% || exit /b 1
C:\tools\nuget-6.14.0.exe restore dep/nuget/packages.config -ConfigFile NuGet.Config -Source %FEED% -PackagesDirectory packages || exit /b 1
exit /b 0

:build
msbuild OpenConsole.slnx %MSBUILD_ARGS% /p:AppxPackageSigningEnabled=false "/t:Terminal\CascadiaPackage" /bl:msbuild.binlog
exit /b %errorlevel%

:test
msbuild OpenConsole.slnx %MSBUILD_ARGS% /p:AppxPackageSigningEnabled=false "/t:Terminal\Tests\SettingsModel_UnitTests" || exit /b 1
bin\x64\Release\UnitTests_SettingsModel\te.exe bin\x64\Release\UnitTests_SettingsModel\SettingsModel.Unit.Tests.dll /select:"@Name='*ApplicationStateTests*' or @Name='*AtLogTests*' or @Name='*RemovedScrollbarSettingsAreDropped*'"
exit /b %errorlevel%
