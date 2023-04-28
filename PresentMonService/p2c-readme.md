# PresentMon 2 Capture Application

 ![Architecture](docs/images/app-cef-overlay-architecture.jpg)

## Overview

The PresentMon 2 Capture Application is both an offline trace capture and realtime performance overlay for games and other graphics-intensive applications. It uses the PresentMon 2 frame data service to source performance data, a custom Direct3D 11 renderer to display a realtime performance overlay, and a CEF-based UI to configure overlay and trace capture functionality. 

## Building

### Prerequisites

Node.js / NPM are required to build the web assets for the control UI. A specific root signing authority certificate is required to be present on the system to build in release configuration.

### Chromium Embedded Framework (CEF)

#### Version

CEF version 111.2.6 is officially supported. Proximal versions will most likely be compatible, but are not officially supported.

#### Download Distribution

Download CEF distribution from spotify and extract to a local folder. The "Minimal Distribution" is sufficient. [Download Link](https://cef-builds.spotifycdn.com/index.html)

#### Build Wrapper

The "Binary Distribution" also contains source code which must be built. Build steps are as follow:

0. In the distribution root, follow the instructions in CMakeLists.txt to generate a .sln file for a Windows 64-bit CEF binary distribution
0. Open the generated .sln file
0. Update the libcef_dll_wrapper properties
    - Project Properties > C++ > Code Generation > Runtime Library: set to Multi-threaded DLL for Release
    - Project Properties > C++ > Code Generation > Runtime Library: set to Multi-threaded Debug DLL for Debug
    - Project Properties > C++ > Preprocessor > Preprocessor Definitions: set the following macros for Debug
       - _ITERATOR_DEBUG_LEVEL=2
       - _HAS_ITERATOR_DEBUGGING=1
0. Build the Release and Debug versions of the wrapper library

#### Pull CEF Distribution into AppCef Project

The files from the CEF distribution need to be rearranged before they can be consumed by this project. A batch file is provided that will copy necessary files from the CEF distribution folder to the required locations in the AppCef project folder.

Relative to the PresentMon solution root, the batch file is located at `AppCef\Batch\pull-cef.bat`. Run this batch command by passing it the full path to the CEF distribution.

### Web Assets

#### Grab Dependencies

Download dependencies via NPM (only needs to be run once on fresh clone, or after new packages are added)

```
npm ci
```

#### Build Vue.js SPA

You need to build the Vue.js web application (compile single file components and bundle into chunks) before it can be loaded by the Chromium engine embedded in the desktop application.

You can either run a development build process, which builds in dev mode and starts a local server with hotloading support:

```
npm run serve
```

Or you can do a full production build, which places all necessary build artifacts in a directory named `dist/`:

```
npm run build
```

These build artifacts are automatically copied to the output directory as a post-build step in the MSVC project. Furthermore, in Release builds the post-build script automatically downloads dependencies and executes a production build.

### C++ Application

#### Debug Configuration

Debug configuration has no special requirements to build. It can be built and run from Visual Studio with debugger attached, because it does not create its overlay window in an elevated Z-band, and thus does not need uiAccess to be set.

#### Release Configuration

Release configuration requires a specific Trusted Root Certificate to be present on the system in order to successfully build. Release configuration creates its overlay window in an elevated Z-band, requiring uiAccess to be set, which in turn requires that the executable be cryptographically signed.

### Trusted Root Certificate

The build automation for Release configuration signs the executable as a post-build step. For this to work, your `PrivateCertStore` must contain a certificate named "Test Certificate - For Internal Use Only".

Such a certificate can be created with the following command:

```
makecert -r -pe -n "CN=Test Certificate - For Internal Use Only" -ss PrivateCertStore testcert.cer
```

## Running

### Debug Configuration

No special instructions. Can be run from IDE with debugger attached. 

(Note: Visual Studio stores the Debugger working directory setting in user-local config. When doing a fresh clone, you will need to change the Working Directory Debugging setting to `$(OutDir)`)

### Release Configuration

Since `uiAccess=true`, the application must be run from a secure location (e.g. "Program Files" or "System32"). It also cannot be started from Visual Studio (either with or without debugger attached, even if VS is running with admin privilege).

### Command Line Options (AppCef)

The app will load web content located at Web/index.html by default. The following can be used to load from an HTTP server (typically a local dev server) instead:
```
--p2c-url=http://localhost:8080/
```
Only severe errors are logged by default in Release configuration. To log all errors in release:
```
--p2c-verbose
```
Logs and cache files are written to %AppData%\PresentMon2Capture by default. You can change this to the working directory of the application (convenient when launching Debug build from IDE):
```
--p2c-files-working
```

## Projects

### AppCef

Main deliverable. Contains all logic for the UI control layer (mainly in the form of Vue.js components). Contains CEF API and binaries.

Depends on: Core, Shaders

Dependencies: None

### AppTest

App for interactive testing of Core (overlay). Suited to debugging overlay issues due to lack of complications related to CEF (such as multi-process architecture).

Depends on: Core, Shaders

Dependencies: None

### Core

Contains code for spawning Z-band overlay window, rendering realtime graphs in Direct 3D, and interfacing with PresentMon 2 service.

Depends on: None

Dependencies: AppCef, AppTest, Unit Tests

### Unit Tests

Automated testing for select Core components.

Depends on: Core

Dependencies: None

### Shaders

Contains shaders needed for rendering the overlay.

Depends on: None

Dependencies: AppCef, AppTest

## Technology

### Z-band

[Z-bands](https://blog.adeltax.com/window-z-order-in-windows-10/)

Windows has a concept of Z-bands. These add a hierarchical layer to the idea of Z-order, such that all windows in a higher Z-band will always appear on top of windows in any lower Z-bands. By default, user application windows are created in the lowest Z-band (ZBID_DESKTOP), and certain OS elements such as the Start Menu or Xbox Game Bar exist on higher Z-bands.

There exists an undocumented WinAPI function called `CreateWindowInBand` that allows an application to create a window in a Z-band above the default one. When this function is called, the OS will perform a check to make sure the application has the required privileges. We give the app these privileges by setting `uiAccess=true` in the app manifest.

#### Motivation

Our motivation to use `CreateWindowInBand` is to ensure that the performance monitoring overlay appears above the target game application, even when running in fullscreen exclusive mode.

### uiAccess

[MSDN:uiAccess](https://docs.microsoft.com/en-us/windows/security/threat-protection/security-policy-settings/user-account-control-only-elevate-uiaccess-applications-that-are-installed-in-secure-locations)

`uiAccess` is an option that is set in an executable's manifest. It enables bypassing UI restrictions and is meant mainly for accessibility applications such as IMEs that need to appear above the active application.

This ability to bypass UI restrictions means that certain precaution are taken with respect to uiAccess applications:

- The application must be cryptographically signed to protect against tampering
- The application must be run from a trusted location (such as "Program Files")

#### Issues

- There seems to be problems with spawning a uiAccess process from another (non-admin) process.
- There might be problems when a normal (non-admin) process tries to Send/PostMessage to a uiAccess process

#### uiAccess Application Special Abilities / Vulnerabilities

- Set the foreground window.
- Drive any application window by using the SendInput function.
- Use read input for all integrity levels by using low-level hooks, raw input, - GetKeyState, GetAsyncKeyState, and GetKeyboardInput.
- Set journal hooks.
- Use AttachThreadInput to attach a thread to a higher integrity input queue.

#### Observations

We have noted that an application can remain on top (even above fullscreen exclusive games) when uiAccess is set to true, even when `CreateWindowInBand` is not used. This also seems to be reported elsewhere (https://www.autohotkey.com/boards/viewtopic.php?t=75695).

#### Related Info

[MSDN:Integrity Levels](https://docs.microsoft.com/en-us/previous-versions/dotnet/articles/bb625963(v=msdn.10)?redirectedfrom=MSDN)

### CEF

(https://bitbucket.org/chromiumembedded/cef/wiki/Home)

PresentMon 2 Capture uses CEF to implement the the control UI. 

The Chromium Embedded Framework (CEF) is a C++ framework that streamlines development of custom applications with Chromium. With some minimal bootstrapping and configuring code, the framework will spin up and connect Chromium components, binding them to windows, inputs, sockets, etc. on the platform of choice.

Behavior of the framework can be customized by inheriting from base class interfaces and injecting them into the framework, thus hooking various callback functions to implement your desired behavior. In particular, custom objects can be implemented in C++ and then injected into the global (window) namespace in V8 to create an interop between JS and C++ code.

A major challenge when dealing with CEF is the multi-process nature of Chromium. One must be aware at all time on which process and which thread each piece of code is running on. Thread task queues and IPC message queues are used to make sure that operations are executed on the appropriate thread and process. V8 contexts must also be captured and managed when interacting with V8 state.