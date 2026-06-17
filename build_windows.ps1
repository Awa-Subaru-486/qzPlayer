taskkill /F /IM clangd.exe
#Get-ChildItem -Recurse -Filter "*.pcm" | Remove-Item -Force
cmake.exe --build --target all --preset llvm-mingw-debug -j 4