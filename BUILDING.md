# Building PresentMon

Requirements:

- Visual Studio
- [Windows 10 SDK 10.0.10240.0](https://developer.microsoft.com/en-us/windows/downloads/sdk-archive/)
- [Windows 8.1 SDK](https://developer.microsoft.com/en-us/windows/downloads/sdk-archive/)
- awk and sed in your path (e.g., `%ProgramFiles%\Git\usr\bin`)

Quick-start:

```batch
msbuild /p:Platform=x64,Configuration=Release PresentMon.sln
build\Release\PresentMon-dev-x64.exe
```
