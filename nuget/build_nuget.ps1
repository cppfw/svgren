Push-Location
$scriptdir = Split-Path $MyInvocation.MyCommand.Path
cd $scriptdir

#extract version from debian/changelog
$ver = (Get-Content ..\debian\changelog -Head 1) -replace ".*\((\d*\.\d*\.\d*)(\-\d+){0,1}\).*",'$1'
#Write-Host $ver

#insert version into all *.in files
Get-ChildItem "." -Filter *.in | Foreach-Object{
    $content = Get-Content $_.FullName

    #filter and save content to the original file
    #$content | Where-Object {$_ -match 'step[49]'} | Set-Content $_.FullName

    #filter and save content to a new file 
    ($content -replace '\$\(version\)',"$ver") | Set-Content ($_.BaseName)
}


"%VS140COMNTOOLS%\VsMSBuildCmd.bat"
msbuild ../msvs_solution/msvs_solution.sln /t:Rebuild /p:Configuration=Debug /p:Platform=x86 /v:minimal /nologo
If(!$?){exit 1}
msbuild ../msvs_solution/msvs_solution.sln /t:Rebuild /p:Configuration=Release /p:Platform=x86 /v:minimal /nologo
If(!$?){exit 1}
msbuild ../msvs_solution/msvs_solution.sln /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /v:minimal /nologo
If(!$?){exit 1}
msbuild ../msvs_solution/msvs_solution.sln /t:Rebuild /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo
If(!$?){exit 1}
Write-NuGetPackage nuget.autopkg
If(!$?){exit 1}
Pop-Location
