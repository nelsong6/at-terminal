@echo off
rem What the workflow runs in the container: run.cmd restore, build or test.
call C:\BuildTools\Common7\Tools\VsDevCmd.bat -arch=amd64 -no_logo >nul
set VCPKG_ROOT=C:\BuildTools\VC\vcpkg
set VCPKG_DOWNLOADS=C:\cache\vcpkg-downloads
if not exist %VCPKG_DOWNLOADS% mkdir %VCPKG_DOWNLOADS%
set MSBUILD_ARGS=/m:4 /p:Configuration=Release /p:Platform=x64 /p:WindowsTerminalBranding=Dev
cd /d C:\b
goto %1

:restore
C:\tools\nuget-6.14.0.exe restore build/packages.config -ConfigFile NuGet.Config -PackagesDirectory packages || exit /b 1
msbuild src/cascadia/CascadiaPackage/CascadiaPackage.wapproj /t:Restore %MSBUILD_ARGS% "/p:SolutionDir=C:\b\\" || exit /b 1
C:\tools\nuget-6.14.0.exe restore dep/nuget/packages.config -ConfigFile NuGet.Config -PackagesDirectory packages || exit /b 1
exit /b 0

:build
msbuild OpenConsole.slnx %MSBUILD_ARGS% /p:AppxPackageSigningEnabled=false "/t:Terminal\CascadiaPackage" /bl:msbuild.binlog
exit /b %errorlevel%

:test
msbuild OpenConsole.slnx %MSBUILD_ARGS% /p:AppxPackageSigningEnabled=false "/t:Terminal\Tests\SettingsModel_UnitTests" || exit /b 1
bin\x64\Release\UnitTests_SettingsModel\te.exe bin\x64\Release\UnitTests_SettingsModel\SettingsModel.Unit.Tests.dll /name:*ApplicationStateTests*
exit /b %errorlevel%
