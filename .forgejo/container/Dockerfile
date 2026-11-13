# The toolchain at-terminal builds with. .vsconfig is Visual Studio's export of
# the components the build needs.
FROM artifacts.home.romaine.life/mcr/windows/servercore:ltsc2025@sha256:c622317822fefb54a056fd09ab51f5bb48390feb47abb2b8696b2c526bdce4ef
SHELL ["cmd", "/S", "/C"]
# internet: the Build Tools installer downloads its payload from Microsoft as it installs
ADD https://aka.ms/vs/18/stable/vs_buildtools.exe C:/setup/vs_buildtools.exe
COPY .vsconfig C:/setup/.vsconfig
RUN C:\setup\vs_buildtools.exe --quiet --wait --norestart --nocache --config C:\setup\.vsconfig --installPath "C:\BuildTools" || IF "%ERRORLEVEL%"=="3010" EXIT 0
RUN rmdir /s /q C:\setup
ADD https://artifacts.home.romaine.life/general/github/PowerShell/PowerShell/releases/download/v7.6.9/PowerShell-7.6.9-win-x64.zip C:/ps.zip
RUN mkdir "C:\Program Files\PowerShell\7" && tar -xf C:\ps.zip -C "C:\Program Files\PowerShell\7" && del C:\ps.zip
RUN setx /M PATH "%PATH%;C:\Program Files\PowerShell\7"
