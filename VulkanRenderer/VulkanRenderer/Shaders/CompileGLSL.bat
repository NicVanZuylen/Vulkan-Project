echo off
for /r %%f in (*.vert *.tesc *.tese *.geom *.frag *.comp) do glslangValidator -V -o "%~dp0\SPIR-V\%%~nf.spv" "%%f"
pause