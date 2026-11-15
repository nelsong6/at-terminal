# Runs one of run.cmd's steps in the build container: the kept folder at C:\b, so
# paths don't change between runs, and downloads kept in a cache beside it.
# vcpkg's registry, github.com/microsoft/vcpkg, comes from Forgejo's copy
# (forgejo k8s/copies.txt), read with PIPELINE_DISPATCH_TOKEN (FORGEJO_TOKEN): a job's own token reads only its repo.
param([Parameter(Mandatory)][ValidateSet('restore', 'build', 'test')][string]$Step)

# Terminal prompts and Git for Windows' credential-helper selector must both be disabled.
$cache = 'D:\ci\at-terminal\cache'
New-Item -ItemType Directory -Force $cache | Out-Null
$tools = Split-Path $env:NUGET_EXE
# at-protocol (dep/vcpkg-overlay-ports/at-protocol) is cloned from Forgejo by its port
# with the same token.
$signIn = 'AUTHORIZATION: basic ' + [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes("x-access-token:$env:FORGEJO_TOKEN"))
$env:GIT_CONFIG_COUNT = '3'
$env:GIT_CONFIG_KEY_0 = 'url.https://forgejo.romaine.life/microsoft/vcpkg.insteadOf'
$env:GIT_CONFIG_VALUE_0 = 'https://github.com/microsoft/vcpkg'
$env:GIT_CONFIG_KEY_1 = 'http.https://forgejo.romaine.life/microsoft/vcpkg.extraHeader'
$env:GIT_CONFIG_VALUE_1 = $signIn
$env:GIT_CONFIG_KEY_2 = 'http.https://forgejo.romaine.life/romaine-life/at-protocol.git.extraHeader'
$env:GIT_CONFIG_VALUE_2 = $signIn
# vcpkg clears its port-build environment unless these names are explicitly retained.
$env:VCPKG_KEEP_ENV_VARS = (@('GIT_CONFIG_COUNT') +
    @(0..([int]$env:GIT_CONFIG_COUNT - 1) | ForEach-Object { "GIT_CONFIG_KEY_$_"; "GIT_CONFIG_VALUE_$_" })) -join ';'
& $env:DOCKER run --rm --isolation=process -m 40g `
    -e GIT_TERMINAL_PROMPT=0 -e "GIT_CONFIG_PARAMETERS='credential.helper='" `
    -e VCPKG_MAX_CONCURRENCY -e CMAKE_BUILD_PARALLEL_LEVEL -e _CL_ `
    -e GIT_CONFIG_COUNT -e GIT_CONFIG_KEY_0 -e GIT_CONFIG_VALUE_0 -e GIT_CONFIG_KEY_1 -e GIT_CONFIG_VALUE_1 -e GIT_CONFIG_KEY_2 -e GIT_CONFIG_VALUE_2 `
    -e VCPKG_KEEP_ENV_VARS `
    -v "$($env:TREE):C:\b" -v "${cache}:C:\cache" -v "${tools}:C:\tools:ro" `
    $env:IMAGE cmd /c C:\b\.forgejo\container\run.cmd $Step
exit $LASTEXITCODE
