# The toolchain at-terminal builds with. .vsconfig is Visual Studio's export of
# the components the build needs.
FROM mcr.microsoft.com/windows/servercore:ltsc2025
SHELL ["cmd", "/S", "/C"]
ADD https://aka.ms/vs/18/stable/vs_buildtools.exe C:/setup/vs_buildtools.exe
COPY .vsconfig C:/setup/.vsconfig
RUN C:\setup\vs_buildtools.exe --quiet --wait --norestart --nocache --config C:\setup\.vsconfig --installPath "C:\BuildTools" || IF "%ERRORLEVEL%"=="3010" EXIT 0
RUN rmdir /s /q C:\setup
ADD https://github.com/PowerShell/PowerShell/releases/download/v7.6.9/PowerShell-7.6.9-win-x64.zip C:/ps.zip
RUN mkdir "C:\Program Files\PowerShell\7" && tar -xf C:\ps.zip -C "C:\Program Files\PowerShell\7" && del C:\ps.zip
RUN setx /M PATH "%PATH%;C:\Program Files\PowerShell\7"
