$dllPath = "D:\\Projects\\C++\\UDF\\release\\sosna_udf.dll"
$dependsPath = "D:\\Programs\\MinGW\\depends32\\depends.exe"

if (Test-Path $dependsPath) {
    & $dependsPath $dllPath
} else {
    Write-Host "Dependency Walker не найден по пути $dependsPath"
}