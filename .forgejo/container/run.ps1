# Runs one of run.cmd's steps in the build container: the kept folder at C:\b, so
# paths don't change between runs, and downloads kept in a cache beside it.
param([Parameter(Mandatory)][ValidateSet('restore', 'build', 'test')][string]$Step)

$cache = Join-Path $env:USERPROFILE 'at-terminal-ci\cache'
New-Item -ItemType Directory -Force $cache | Out-Null
$tools = Split-Path $env:NUGET_EXE
& $env:DOCKER run --rm --isolation=process -m 40g `
    -e VCPKG_MAX_CONCURRENCY -e CMAKE_BUILD_PARALLEL_LEVEL -e _CL_ `
    -v "$($env:TREE):C:\b" -v "${cache}:C:\cache" -v "${tools}:C:\tools:ro" `
    $env:IMAGE cmd /c C:\b\.forgejo\container\run.cmd $Step
exit $LASTEXITCODE
