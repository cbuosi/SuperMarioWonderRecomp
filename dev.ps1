# Development environment for SwitchGamesRecomp (Windows).
# Usage, in PowerShell:  . .\dev.ps1
# The VS install is found with vswhere, so Build Tools 2022, 18 or a full Visual Studio all work;
# $env:NX_VS_PATH overrides it.
$installer = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer"
$env:PATH = "$installer;$env:PATH"  # vswhere, which VsDevCmd calls
if ($env:NX_VS_PATH) {
    $vs = $env:NX_VS_PATH
} else {
    # The generated code needs clang, so an install that has it wins over one that only has MSVC.
    $vs = & "$installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Llvm.Clang -property installationPath
    if (-not $vs) {
        $vs = & "$installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    }
    if (-not $vs) { throw "no Visual Studio with the C++ tools found; set NX_VS_PATH" }
}
Import-Module "$vs\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64" | Out-Null
# clang, cmake and ninja from Visual Studio ahead of MinGW
$cm = "$vs\Common7\IDE\CommonExtensions\Microsoft\CMake"
$env:PATH = "$vs\VC\Tools\Llvm\x64\bin;$cm\CMake\bin;$cm\Ninja;$env:PATH"
$env:PATH = "$PSScriptRoot\private\tools\nstool;$env:PATH"
# Vulkan SDK: headers, vk.xml (tools/gen_vk.py), validation layer (NX_VK_VALIDATION=1)
if (-not $env:VULKAN_SDK) { $env:VULKAN_SDK = (Get-ChildItem C:\VulkanSDK -Directory | Sort-Object Name | Select-Object -Last 1).FullName }
$env:PATH = "$env:VULKAN_SDK\Bin;$env:PATH"
Set-Location $PSScriptRoot
if (Get-Command clang-cl -ErrorAction SilentlyContinue) {
    Write-Host "SwitchGamesRecomp dev env: clang $((clang-cl --version)[0] -replace '.*version ','') | $(Get-Location)"
} else {
    Write-Warning "clang-cl not on the PATH: add 'C++ Clang tools for Windows' to this Visual Studio install (the generated code needs clang)"
    Write-Host "SwitchGamesRecomp dev env: $vs | $(Get-Location)"
}
