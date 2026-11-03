[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string] $InputFile,

    [Parameter(Mandatory = $false, Position = 1)]
    [string] $OutputDirectory = "log-parts",

    [Parameter(Mandatory = $false)]
    [ValidateRange(1, 30)]
    [int] $MaxMiB = 30
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$resolvedInput = [System.IO.Path]::GetFullPath($InputFile)
if (-not [System.IO.File]::Exists($resolvedInput)) {
    throw "Arquivo não encontrado: $resolvedInput"
}

$resolvedOutput = [System.IO.Path]::GetFullPath($OutputDirectory)
[System.IO.Directory]::CreateDirectory($resolvedOutput) | Out-Null
$limitBytes = [int64]$MaxMiB * 1024 * 1024

$utf8 = [System.Text.UTF8Encoding]::new($false)
$reader = [System.IO.StreamReader]::new($resolvedInput, $utf8, $true, 1024 * 1024)
$writer = $null
$partNumber = 0
$partBytes = [int64]0
$lineNumber = [int64]0

try {
    while ($null -ne ($line = $reader.ReadLine())) {
        $lineNumber++
        $lineBytes = $utf8.GetByteCount($line) + 1

        if ($lineBytes -gt $limitBytes) {
            throw "A linha $lineNumber sozinha excede o limite de $MaxMiB MiB; não é seguro dividi-la sem corromper o log."
        }

        if ($null -eq $writer -or ($partBytes -gt 0 -and $partBytes + $lineBytes -gt $limitBytes)) {
            if ($null -ne $writer) {
                $writer.Flush()
                $writer.Dispose()
            }

            $partNumber++
            $partName = "{0}-part-{1:D4}.log" -f [System.IO.Path]::GetFileNameWithoutExtension($resolvedInput), $partNumber
            $partPath = [System.IO.Path]::Combine($resolvedOutput, $partName)
            $writer = [System.IO.StreamWriter]::new($partPath, $false, $utf8, 1024 * 1024)
            $partBytes = 0
        }

        $writer.WriteLine($line)
        $partBytes += $lineBytes
    }
}
finally {
    if ($null -ne $writer) {
        $writer.Flush()
        $writer.Dispose()
    }
    $reader.Dispose()
}

Write-Output ("Criadas {0} partes em {1} (limite de {2} MiB; {3:N0} linhas lidas)." -f $partNumber, $resolvedOutput, $MaxMiB, $lineNumber)
