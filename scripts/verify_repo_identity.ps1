param(
    [switch]$SourceOnly,
    [switch]$InstallHook,
    [string]$PushUrl
)

$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..')).TrimEnd('\')
$identity = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'repository_identity.json') -Raw | ConvertFrom-Json
if (-not (Test-Path -LiteralPath (Join-Path $projectRoot $identity.solution) -PathType Leaf) -or
    (Test-Path -LiteralPath (Join-Path $projectRoot $identity.forbidden_solution))) {
    throw 'Project solution does not match the repository identity.'
}

# Scan source/automation/docs only; never inspect runtime INI, logs or user data.
$files = @(Get-ChildItem -LiteralPath $projectRoot -File)
foreach ($directory in @('src', 'scripts', 'docs', 'tests')) {
    $path = Join-Path $projectRoot $directory
    if (Test-Path -LiteralPath $path) {
        $files += @(Get-ChildItem -LiteralPath $path -Recurse -File)
    }
}
$extensions = @('.md', '.cpp', '.h', '.ps1', '.bat', '.sln', '.vcxproj', '.filters', '.rc')
foreach ($file in $files) {
    if ($extensions -contains $file.Extension.ToLowerInvariant()) {
        if (Select-String -LiteralPath $file.FullName -Pattern '^(<{7} |>{7} )' -Quiet) {
            throw "Unresolved merge marker: $($file.FullName)"
        }
    }
}

if ($InstallHook) {
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
        Write-Warning 'Git is unavailable; source validation passed but hook installation was skipped.'
    } elseif (-not (Test-Path -LiteralPath (Join-Path $projectRoot '.git'))) {
        Write-Host 'Source archive: no Git hook to install.'
    } else {
        $top = & git -C $projectRoot rev-parse --show-toplevel
        if ($LASTEXITCODE -ne 0 -or [IO.Path]::GetFullPath($top).TrimEnd('\') -ne $projectRoot) {
            throw 'Refusing to install a hook outside this repository.'
        }
        $customHooks = & git -C $projectRoot config --get core.hooksPath
        if ($customHooks) { throw 'Custom core.hooksPath exists; preserve it and integrate the guard explicitly.' }
        $gitDirectory = & git -C $projectRoot rev-parse --absolute-git-dir
        if ($LASTEXITCODE -ne 0) { throw 'Cannot resolve the Git directory.' }
        $hookPath = Join-Path $gitDirectory 'hooks\pre-push'
        $hookSource = Join-Path $PSScriptRoot 'hooks\pre-push'
        if (Test-Path -LiteralPath $hookPath) {
            if ((Get-FileHash -LiteralPath $hookPath).Hash -ne (Get-FileHash -LiteralPath $hookSource).Hash) {
                throw 'Existing pre-push hook differs; it was not overwritten.'
            }
        } else {
            if (-not (Test-Path -LiteralPath (Split-Path $hookPath))) {
                New-Item -ItemType Directory -Path (Split-Path $hookPath) | Out-Null
            }
            Copy-Item -LiteralPath $hookSource -Destination $hookPath
        }
        Write-Host 'Repository pre-push guard installed; other Git hooks were preserved.'
    }
}

if (-not $SourceOnly) {
    if (-not $identity.publication_enabled) {
        throw 'Publication is disabled for this local-only project until the user confirms a destination.'
    }
    if (-not $PushUrl) {
        $PushUrl = & git -C $projectRoot remote get-url --push origin 2>$null
        if ($LASTEXITCODE -ne 0) { throw 'No verified origin; publication is disabled.' }
    }
    $slug = $null
    if ($PushUrl -match '^https://github\.com/([^/\s:@]+)/([^/\s?#]+?)(?:\.git)?/?$' -or
        $PushUrl -match '^git@github\.com:([^/\s:]+)/([^/\s?#]+?)(?:\.git)?/?$' -or
        $PushUrl -match '^ssh://git@github\.com/([^/\s:]+)/([^/\s?#]+?)(?:\.git)?/?$') {
        $slug = "$($Matches[1])/$($Matches[2])"
    }
    if ($slug -ne $identity.github_repository) {
        throw "Push blocked: destination must be $($identity.github_repository); aliases and other URLs are not accepted."
    }
    Write-Host "Publication identity PASS: $slug"
}
Write-Host "Source identity PASS: $($identity.solution)"
