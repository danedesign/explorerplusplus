param(
	[ValidateSet("Debug", "Debug-Asan", "Debug-Clang", "Release", "Release-Clang")]
	[string] $Configuration = "Release",

	[ValidateSet("Win32", "x64", "ARM64")]
	[string] $Platform = "x64",

	[switch] $DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = $PSScriptRoot

$knownMsBuildPaths = @(
	"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe",
	"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe",
	"C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe"
)

$msbuild = $knownMsBuildPaths | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1

if (-not $msbuild) {
	$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

	if (Test-Path -LiteralPath $vswhere) {
		$installPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath

		if ($installPath) {
			$candidate = Join-Path $installPath "MSBuild\Current\Bin\amd64\MSBuild.exe"

			if (Test-Path -LiteralPath $candidate) {
				$msbuild = $candidate
			}
		}
	}
}

if (-not $msbuild) {
	throw "MSBuild was not found. Install Visual Studio with Desktop development with C++, or update this script with the MSBuild path."
}

$project = Join-Path $repoRoot "Explorer++\Explorer++\Explorer++.vcxproj"
$solutionDir = (Join-Path $repoRoot "Explorer++") + "\"
$arguments = @(
	$project,
	"/p:Configuration=$Configuration",
	"/p:Platform=$Platform",
	"/p:SolutionDir=$solutionDir"
)

Write-Host "MSBuild: $msbuild"
Write-Host "Project: $project"
Write-Host "Configuration: $Configuration"
Write-Host "Platform: $Platform"

if ($DryRun) {
	Write-Host "Dry run only. Command:"
	Write-Host "& `"$msbuild`" $($arguments -join ' ')"
	exit 0
}

& $msbuild @arguments
exit $LASTEXITCODE
