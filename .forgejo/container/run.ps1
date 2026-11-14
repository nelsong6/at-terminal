# Runs one of run.cmd's steps in the build container: the kept folder at C:\b, so
# paths don't change between runs, and downloads kept in a cache beside it.
# vcpkg's registry, github.com/microsoft/vcpkg, comes from Forgejo's copy
# (forgejo k8s/copies.txt), read with PIPELINE_DISPATCH_TOKEN (FORGEJO_TOKEN): a job's own token reads only its repo.
param([Parameter(Mandatory)][ValidateSet('restore', 'build', 'test')][string]$Step)

$cache = 'D:\ci\at-terminal\cache'
New-Item -ItemType Directory -Force $cache | Out-Null
$tools = Split-Path $env:NUGET_EXE
$env:GIT_CONFIG_COUNT = '2'
$env:GIT_CONFIG_KEY_0 = 'url.https://forgejo.romaine.life/microsoft/vcpkg.insteadOf'
$env:GIT_CONFIG_VALUE_0 = 'https://github.com/microsoft/vcpkg'
$env:GIT_CONFIG_KEY_1 = 'http.https://forgejo.romaine.life/microsoft/vcpkg.extraHeader'
$env:GIT_CONFIG_VALUE_1 = 'AUTHORIZATION: basic ' + [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes("x-access-token:$env:FORGEJO_TOKEN"))
& $env:DOCKER run --rm --isolation=process -m 40g `
    -e VCPKG_MAX_CONCURRENCY -e CMAKE_BUILD_PARALLEL_LEVEL -e _CL_ `
    -e GIT_CONFIG_COUNT -e GIT_CONFIG_KEY_0 -e GIT_CONFIG_VALUE_0 -e GIT_CONFIG_KEY_1 -e GIT_CONFIG_VALUE_1 `
    -v "$($env:TREE):C:\b" -v "${cache}:C:\cache" -v "${tools}:C:\tools:ro" `
    $env:IMAGE cmd /c C:\b\.forgejo\container\run.cmd $Step
exit $LASTEXITCODE
