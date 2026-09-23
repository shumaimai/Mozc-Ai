param(
  [Parameter(Mandatory = $true)]
  [string]$MozcRoot
)

$ErrorActionPreference = "Stop"
$wordsPath = Join-Path $MozcRoot "src\\data\\dictionary_manual\\words.tsv"
$entry = "に`tに`t接続詞"
$utf8NoBom = [System.Text.UTF8Encoding]::new($false)

if (-not (Test-Path $wordsPath)) {
  throw "Mozc words.tsv was not found: $wordsPath"
}

$text = [System.IO.File]::ReadAllText($wordsPath, $utf8NoBom)
$lines = [System.IO.File]::ReadAllLines($wordsPath, $utf8NoBom)

if ($lines -contains $entry) {
  Write-Host "Entry already exists: $entry"
  exit 0
}

$prefix = if ($text.Length -eq 0 -or $text.EndsWith("`n")) { "" } else { "`n" }
[System.IO.File]::AppendAllText($wordsPath, $prefix + $entry + "`n", $utf8NoBom)

$verify = [System.IO.File]::ReadAllLines($wordsPath, $utf8NoBom)
if (-not ($verify -contains $entry)) {
  throw "Failed to add the conjunction entry."
}

Write-Host "Added Mozc dictionary entry: $entry"
