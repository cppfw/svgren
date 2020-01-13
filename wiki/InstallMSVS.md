# Using with Visual Studio 2015 or above

There is a NuGet package on nuget.org: **libsvgren**.

## VS2019+ and v142 tools

When using `v142` tools (`VS2019` or above) one needs to set also the runtime library in project master settings:

`project properties` (right click on project and select `properties`) -> `Project Master Settings` -> `Runtime Library`.

The value can be one of `Multithreaded`, `Multithreaded Debug`, `Multithreaded DLL`, `Multithreaded DLL Debug`. Set the same value is in your `C/C++` -> `Runtime Library`.

If `Project Master Settings` / `Runtime Library` is not set you will get linking errors when building your project.
