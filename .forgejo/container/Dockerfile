# The toolchain at-terminal builds with: Visual Studio Build Tools from the offline layout
# the workflow keeps in Artifact Keeper (LAYOUT, its address), installed with --noWeb and
# deleted in the same step, so no layer holds it.
FROM artifacts.home.romaine.life/mcr/windows/servercore:ltsc2025@sha256:c622317822fefb54a056fd09ab51f5bb48390feb47abb2b8696b2c526bdce4ef
SHELL ["cmd", "/S", "/C"]
ADD https://artifacts.home.romaine.life/general/github/PowerShell/PowerShell/releases/download/v7.6.9/PowerShell-7.6.9-win-x64.zip C:/ps.zip
RUN mkdir "C:\Program Files\PowerShell\7" && tar -xf C:\ps.zip -C "C:\Program Files\PowerShell\7" && del C:\ps.zip
RUN setx /M PATH "%PATH%;C:\Program Files\PowerShell\7"
SHELL ["C:\\Program Files\\PowerShell\\7\\pwsh.exe", "-NoLogo", "-NoProfile", "-Command", "$ErrorActionPreference = 'Stop'; $PSNativeCommandUseErrorActionPreference = $true;"]
ARG LAYOUT
RUN New-Item -ItemType Directory C:\layout | Out-Null; \
    curl.exe -fsSL $env:LAYOUT | tar.exe -xf - -C C:\layout; \
    $setup = Start-Process C:\layout\vs_buildtools.exe -Wait -PassThru -ArgumentList '--noWeb', '--quiet', '--wait', '--norestart', '--nocache', '--config', 'C:\layout\at-terminal.vsconfig', '--installPath', 'C:\BuildTools'; \
    if ($setup.ExitCode -notin 0, 3010) { throw ('Build Tools setup exited with {0}' -f $setup.ExitCode) }; \
    Remove-Item -Recurse -Force C:\layout
