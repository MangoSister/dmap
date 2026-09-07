param(
    [string]$vcpkg_dir,
    [bool]$clean = $true,
    [bool]$build_binary = $true
)
Write-Host "vcpkg directory: $vcpkg_dir" -ForegroundColor Cyan
Write-Host "Clean build: $clean" -ForegroundColor Cyan
Write-Host "Build binaries: $build_binary" -ForegroundColor Cyan

$vcpkg_toolchain_file = Join-Path -Path $vcpkg_dir -ChildPath "scripts/buildsystems/vcpkg.cmake"

git submodule update --init src/ks
Set-Location .\src\ks
git submodule update --init deps/cxxopts deps/imgui deps/spdlog deps/stb deps/tinyexr deps/tinygltf deps/tinyobjloader deps/tomlplusplus

$slang_zip_path = "deps/slang-2026.16-windows-x86_64.zip"
$slang_extract_folder = "deps/slang"
if (Test-Path -Path $slang_extract_folder -PathType Container) {
    Remove-Item -Path $slang_extract_folder -Recurse -Force
}
Expand-Archive -Path $slang_zip_path -DestinationPath $slang_extract_folder

Set-Location ../../

mkdir .\build -ErrorAction SilentlyContinue
Set-Location .\build
if ($clean) {
    Write-Host "Cleaning build folder..." -ForegroundColor Green
    Get-ChildItem -Path . | Remove-Item -Recurse -Force
}

# Beware of empty spaces in the paths
cmake .. -G"Visual Studio 17 2022" -A x64 "-DCMAKE_TOOLCHAIN_FILE=$vcpkg_toolchain_file"
Write-Host "Done generating VS solution!"
if ($build_binary) {
    cmake --build . --config Release --parallel
    Write-Host "Done building binaries! Exiting..." -ForegroundColor Green
}
else {
    Write-Host "Open VS solution to build binaries yourself! Exiting..." -ForegroundColor Green
}
Set-Location ..