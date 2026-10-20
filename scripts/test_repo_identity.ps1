# Safe, offline identity/merge-marker/hook tests. Fixtures remain as evidence.
$ErrorActionPreference = 'Stop'
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('repo_identity_' + [Guid]::NewGuid().ToString('N'))
$scripts = Join-Path $fixture 'scripts'
New-Item -ItemType Directory -Path (Join-Path $scripts 'hooks') | Out-Null
foreach ($name in @('verify_repo_identity.ps1', 'repository_identity.json')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $scripts $name)
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'hooks\pre-push') -Destination (Join-Path $scripts 'hooks\pre-push')
$identity = Get-Content -LiteralPath (Join-Path $scripts 'repository_identity.json') -Raw | ConvertFrom-Json
$solution = Join-Path $fixture $identity.solution
[IO.File]::WriteAllText($solution, 'Fixture solution; never compiled.')
$guard = Join-Path $scripts 'verify_repo_identity.ps1'
$script:count = 0

function Check-Guard {
    param([string]$Name, [bool]$ExpectedPass, [string[]]$GuardArguments)
    $savedPreference = $ErrorActionPreference
    try {
        # Windows PowerShell wraps native stderr as errors even for expected failures.
        $ErrorActionPreference = 'Continue'
        $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $guard @GuardArguments 2>&1
        $actualPass = $LASTEXITCODE -eq 0
    } finally {
        $ErrorActionPreference = $savedPreference
    }
    if ($actualPass -ne $ExpectedPass) {
        throw "FAIL $Name (expected pass=$ExpectedPass): $output"
    }
    $script:count++
    Write-Host "PASS $Name"
}

Check-Guard 'source-only archive' $true @('-SourceOnly')
$missing = Join-Path $fixture 'missing.sln'
Move-Item -LiteralPath $solution -Destination $missing
Check-Guard 'missing expected solution rejected' $false @('-SourceOnly')
Move-Item -LiteralPath $missing -Destination $solution
$forbidden = Join-Path $fixture $identity.forbidden_solution
[IO.File]::WriteAllText($forbidden, 'Wrong tool fixture.')
Check-Guard 'opposite tool rejected' $false @('-SourceOnly')
Remove-Item -LiteralPath $forbidden
$markerFile = Join-Path $fixture 'README.md'
[IO.File]::WriteAllText($markerFile, ("<" * 7) + " HEAD")
Check-Guard 'unresolved merge marker rejected' $false @('-SourceOnly')
Remove-Item -LiteralPath $markerFile

$slug = $identity.github_repository
foreach ($url in @("https://github.com/$slug.git", "git@github.com:$slug.git", "ssh://git@github.com/$slug.git")) {
    Check-Guard 'canonical URL or local-only policy' ([bool]$identity.publication_enabled) @('-PushUrl', $url)
}
foreach ($url in @('https://github.com/released/HidTestTool.git',
                   'https://github.com/released/unrelated.git',
                   'https://example.com/released/simple_hid_test_tool.git',
                   'https://user:secret@github.com/released/simple_hid_test_tool.git')) {
    Check-Guard 'wrong/alias/credential URL rejected' $false @('-PushUrl', $url)
}
Check-Guard 'missing origin rejected' $false @()

& git -C $fixture init --quiet
if ($LASTEXITCODE -ne 0) { throw 'Fixture Git init failed.' }
Check-Guard 'automatic hook installation' $true @('-SourceOnly', '-InstallHook')
Check-Guard 'idempotent hook installation' $true @('-SourceOnly', '-InstallHook')
$hook = Join-Path $fixture '.git\hooks\pre-push'
[IO.File]::WriteAllText($hook, '# Fixture custom hook; must not be overwritten.')
$originalHash = (Get-FileHash -LiteralPath $hook).Hash
Check-Guard 'custom hook preserved and build fails closed' $false @('-SourceOnly', '-InstallHook')
if ((Get-FileHash -LiteralPath $hook).Hash -ne $originalHash) {
    throw 'Custom hook was overwritten.'
}
Write-Host "PASS $script:count checks; fixtures retained at $fixture"
