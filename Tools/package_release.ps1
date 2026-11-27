param(
    [string]$Configuration = 'Release',
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$cmakeLists = Join-Path $root 'CMakeLists.txt'
$text = Get-Content -Raw -LiteralPath $cmakeLists

if ($text -notmatch 'project\s*\(\s*FormulaCompressor\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
    throw 'Could not read the project version from CMakeLists.txt'
}

$version = $Matches[1]
$pluginName = 'Formula Compressor'
Write-Host "Packaging $pluginName $version ($Configuration)"

if (-not $SkipBuild) {
    & cmake --build (Join-Path $root 'Build') --config $Configuration `
        --target FormulaCompressor_VST3 FormulaCompressor_Standalone --parallel

    if ($LASTEXITCODE -ne 0) {
        throw 'Build failed'
    }
}

$artefacts = Join-Path $root "Build\FormulaCompressor_artefacts\$Configuration"
$vst3 = Join-Path $artefacts "VST3\$pluginName.vst3"
$standalone = Join-Path $artefacts "Standalone\$pluginName.exe"
$readme = Join-Path $root 'README.md'
$readmeZh = Join-Path $root 'README.zh-CN.md'
$changelog = Join-Path $root 'CHANGELOG.md'
$license = Join-Path $root 'LICENSE'
$notices = Join-Path $root 'THIRD-PARTY-NOTICES.md'
$installer = Join-Path $PSScriptRoot 'install_vst3.ps1'

foreach ($required in @($vst3, $standalone, $readme, $readmeZh, $changelog,
                       $license, $notices, $installer)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Missing required file: $required"
    }
}

$moduleInfo = Join-Path $vst3 'Contents\Resources\moduleinfo.json'

if (Test-Path -LiteralPath $moduleInfo) {
    $json = Get-Content -Raw -LiteralPath $moduleInfo

    if ($json -notmatch [regex]::Escape("`"Version`": `"$version`"")) {
        throw "moduleinfo.json does not report version $version. Rebuild FormulaCompressor_VST3."
    }
}

$distRoot = Join-Path $root 'dist'
$stage = Join-Path $distRoot "FormulaCompressor-$version-win64"
$zip = Join-Path $distRoot "FormulaCompressor-$version-win64.zip"

if (Test-Path -LiteralPath $stage) {
    Remove-Item -LiteralPath $stage -Recurse -Force
}

New-Item -ItemType Directory -Force -Path (Join-Path $stage 'VST3') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $stage 'Standalone') | Out-Null

Copy-Item -LiteralPath $vst3 -Destination (Join-Path $stage "VST3\$pluginName.vst3") -Recurse -Force
Copy-Item -LiteralPath $standalone -Destination (Join-Path $stage "Standalone\$pluginName.exe") -Force
Copy-Item -LiteralPath $readme -Destination $stage -Force
Copy-Item -LiteralPath $readmeZh -Destination $stage -Force
Copy-Item -LiteralPath $changelog -Destination $stage -Force
Copy-Item -LiteralPath $license -Destination $stage -Force
Copy-Item -LiteralPath $notices -Destination $stage -Force
Copy-Item -LiteralPath $installer -Destination $stage -Force

Write-Host "Staged: $stage"

if (Test-Path -LiteralPath $zip) {
    Remove-Item -LiteralPath $zip -Force
}

Compress-Archive -Path $stage -DestinationPath $zip -Force
$hash = Get-FileHash -LiteralPath $zip -Algorithm SHA256
$checksums = Join-Path $distRoot 'SHA256SUMS.txt'
$hashLine = "$($hash.Hash.ToLowerInvariant())  $([System.IO.Path]::GetFileName($zip))"
[System.IO.File]::WriteAllText($checksums, $hashLine + "`n")

Write-Host "Zip:       $zip"
Write-Host "Size:      $([math]::Round((Get-Item -LiteralPath $zip).Length / 1MB, 2)) MB"
Write-Host "SHA256:    $($hash.Hash)"
Write-Host "Checksums: $checksums"
