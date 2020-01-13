# Using with Visual Studio 2015 or above

There is a NuGet package on nuget.org: **libsvgren**.

## VS2019+ and v142 tools

When using `v142` tools (`VS2019` or above) one needs to set also the runtime library in project master settings:

`project properties` (right click on project and select `properties`) -> `Project Master Settings` -> `C/C++ Settings` -> `Runtime Library`.

The value can be one of `Multi-threaded`, `Multi-threaded Debug`, `Multi-threaded DLL`, `Multi-threaded Debug DLL`. Set the same value is in your `C/C++` -> `Code Generation` -> `Runtime Library`.

If `Project Master Settings` / `C/C++ Settings` / `Runtime Library` is not set you will get linking errors when building your project.
