# Shared host C compiler discovery for TobyTank host tests and tools.
# Prefers gcc or clang on PATH, otherwise locates a Visual Studio MSVC
# toolchain with vswhere so nothing has to run from a developer shell.

function Find-VcVars {
    $installer = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer"
    $vswhere = Join-Path $installer "vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        return $null
    }
    $install = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ([string]::IsNullOrWhiteSpace($install)) {
        return $null
    }
    $vcvars = Join-Path $install "VC\Auxiliary\Build\vcvars64.bat"
    if (Test-Path $vcvars) {
        return $vcvars
    }
    return $null
}

function Get-HostOutputDir {
    $outDir = Join-Path ([System.IO.Path]::GetTempPath()) "tobytank_host_tests"
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
    return $outDir
}

# Compiles the given sources into $ExeName inside the shared temp directory and
# returns the executable path. Throws on failure.
function Invoke-HostBuild {
    param(
        [Parameter(Mandatory = $true)][string[]] $Sources,
        [Parameter(Mandatory = $true)][string[]] $IncludeDirs,
        [Parameter(Mandatory = $true)][string] $ExeName
    )

    $outDir = Join-Path (Get-HostOutputDir) ([System.IO.Path]::GetFileNameWithoutExtension($ExeName))
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
    $exe = Join-Path $outDir $ExeName

    $gcc = Get-Command gcc -ErrorAction SilentlyContinue
    $clang = Get-Command clang -ErrorAction SilentlyContinue
    $cl = Get-Command cl -ErrorAction SilentlyContinue

    if ($gcc -or $clang) {
        $compiler = if ($gcc) { $gcc.Source } else { $clang.Source }
        $arguments = @('-std=c11', '-Wall', '-Wextra', '-Werror')
        foreach ($dir in $IncludeDirs) { $arguments += "-I$dir" }
        $arguments += $Sources
        $arguments += @('-lm', '-o', $exe)
        # Keep compiler chatter out of the return value; show it only on failure.
        $output = & $compiler @arguments 2>&1
        if ($LASTEXITCODE -ne 0) {
            $output | ForEach-Object { Write-Host $_ }
            throw "$ExeName failed to compile (exit $LASTEXITCODE)"
        }
        return $exe
    }

    # MSVC drops .obj files in the working directory, so build inside $outDir.
    $quotedIncludes = ($IncludeDirs | ForEach-Object { "`"/I$_`"" }) -join ' '
    $quotedSources = ($Sources | ForEach-Object { "`"$_`"" }) -join ' '
    $clCommand = "cl /nologo /std:c11 /W4 /WX /D_CRT_SECURE_NO_WARNINGS $quotedIncludes $quotedSources /Fe$ExeName"

    if ($cl) {
        $command = "cd /d `"$outDir`" && $clCommand"
    } else {
        $vcvars = Find-VcVars
        if ($null -eq $vcvars) {
            throw "No host C compiler found. Install gcc, clang, or the Visual Studio C++ build tools."
        }
        $command = "call `"$vcvars`" >nul 2>nul && cd /d `"$outDir`" && $clCommand"
    }

    $output = & cmd.exe /c $command 2>&1
    if ($LASTEXITCODE -ne 0) {
        $output | ForEach-Object { Write-Host $_ }
        throw "$ExeName failed to compile (exit $LASTEXITCODE)"
    }
    return $exe
}
