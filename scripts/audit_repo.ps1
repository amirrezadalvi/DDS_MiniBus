Param(
  [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..') )
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-Rel([string]$RelPath) {
  if ([System.IO.Path]::IsPathRooted($RelPath)) { return $RelPath }
  return (Join-Path $Root $RelPath)
}

function HasFile([string]$RelPath) {
  return Test-Path (Resolve-Rel $RelPath)
}

function AnyFileContains {
  param(
    [Parameter(Mandatory=$true)][string]$Filter,       # e.g. '*.cpp' or 'serializer.cpp'
    [Parameter(Mandatory=$true)][string]$Pattern,      # plain text (SimpleMatch)
    [string]$RelativeDir = '.'
  )
  $dir = Resolve-Rel $RelativeDir
  $files = Get-ChildItem -Path $dir -Recurse -File -Filter $Filter -ErrorAction SilentlyContinue
  foreach ($f in $files) {
    if (Select-String -Path $f.FullName -Pattern $Pattern -SimpleMatch -Quiet) { return $true }
  }
  return $false
}

function FirstHit {
  param(
    [Parameter(Mandatory=$true)][string]$Filter,
    [Parameter(Mandatory=$true)][string]$Pattern,
    [string]$RelativeDir='.'
  )
  $dir = Resolve-Rel $RelativeDir
  $files = Get-ChildItem -Path $dir -Recurse -File -Filter $Filter -ErrorAction SilentlyContinue
  foreach ($f in $files) {
    $m = Select-String -Path $f.FullName -Pattern $Pattern -SimpleMatch -List -ErrorAction SilentlyContinue
    if ($m) { return "$($f.FullName):$($m.LineNumber) `"$($m.Line.Trim())`"" }
  }
  return $null
}

$tests = @(
  [pscustomobject]@{
    Item='CBOR encode/decode present'
    Pass = (AnyFileContains -Filter 'serializer.cpp' -RelativeDir 'serialization' -Pattern 'encodeDataCBOR') -and
           (AnyFileContains -Filter 'serializer.cpp' -RelativeDir 'serialization' -Pattern 'decodeCBOR')
    Evidence = (FirstHit -Filter 'serializer.cpp' -RelativeDir 'serialization' -Pattern 'encodeDataCBOR')
  },
  [pscustomobject]@{
    Item='Negotiation helper exists'
    Pass = (AnyFileContains -Filter '*.c*' -RelativeDir 'serialization' -Pattern 'negotiateFormat')
    Evidence = (FirstHit -Filter '*.c*' -RelativeDir 'serialization' -Pattern 'negotiateFormat')
  },
  [pscustomobject]@{
    Item='DDSCore uses per-peer negotiation'
    Pass = (AnyFileContains -Filter 'dds_core.cpp' -RelativeDir 'core' -Pattern 'negotiateFormat') -and
           (AnyFileContains -Filter 'dds_core.cpp' -RelativeDir 'core' -Pattern 'fmt=')
    Evidence = (FirstHit -Filter 'dds_core.cpp' -RelativeDir 'core' -Pattern 'negotiateFormat')
  },
  [pscustomobject]@{
    Item='Discovery advertises serialization'
    Pass = (AnyFileContains -Filter 'serializer.cpp' -RelativeDir 'serialization' -Pattern '"serialization"') -and
           (AnyFileContains -Filter 'discovery_manager.cpp' -RelativeDir 'discovery' -Pattern 'getSupportedSerialization')
    Evidence = (FirstHit -Filter 'discovery_manager.cpp' -RelativeDir 'discovery' -Pattern 'getSupportedSerialization')
  },
  [pscustomobject]@{
    Item='Tests: negotiation present'
    Pass = HasFile 'tests\unit\test_negotiation.cpp'
    Evidence = (Resolve-Rel 'tests\unit\test_negotiation.cpp')
  },
  [pscustomobject]@{
    Item='Tests: serializer has CBOR paths'
    Pass = (HasFile 'tests\unit\test_serializer.cpp') -and
           (AnyFileContains -Filter 'test_serializer.cpp' -RelativeDir 'tests\unit' -Pattern 'encodeDataCBOR')
    Evidence = (FirstHit -Filter 'test_serializer.cpp' -RelativeDir 'tests\unit' -Pattern 'encodeDataCBOR')
  },
  [pscustomobject]@{
    Item='Docs: diagrams.puml exists'
    Pass = HasFile 'docs\diagrams.puml'
    Evidence = (Resolve-Rel 'docs\diagrams.puml')
  },
  [pscustomobject]@{
    Item='Docs: Architecture.md exists'
    Pass = HasFile 'docs\Architecture.md'
    Evidence = (Resolve-Rel 'docs\Architecture.md')
  },
  [pscustomobject]@{
    Item='README mentions CBOR'
    Pass = AnyFileContains -Filter 'README.md' -RelativeDir '.' -Pattern 'CBOR'
    Evidence = (FirstHit -Filter 'README.md' -RelativeDir '.' -Pattern 'CBOR')
  },
  [pscustomobject]@{
    Item='Config contains serialization section'
    Pass = AnyFileContains -Filter 'config*.json' -RelativeDir '.' -Pattern '"serialization"'
    Evidence = (FirstHit -Filter 'config*.json' -RelativeDir '.' -Pattern '"serialization"')
  }
)

$results = foreach ($t in $tests) {
  [pscustomobject]@{
    Item = $t.Item
    Status = if ($t.Pass) { 'PASS' } else { 'MISSING' }
    Evidence = if ($t.Evidence) { $t.Evidence } else { '' }
  }
}

$results | Format-Table -AutoSize
$pass = ($results | Where-Object {$_.Status -eq 'PASS'}).Count
$total = $results.Count
Write-Host "`nSummary: $pass / $total checks passed."
