$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
$taskPython = Join-Path $PSScriptRoot '.venv/Scripts/python.exe'
if (-not (Test-Path -LiteralPath $taskPython)) {
    py -3 -m venv .venv
    if ($LASTEXITCODE -ne 0) { throw 'Python 3.10+ with Tk is required.' }
}
& $taskPython -c 'import bleak, cbor2, tkinter'
if ($LASTEXITCODE -ne 0) {
    & $taskPython -m pip install -r requirements.txt
    if ($LASTEXITCODE -ne 0) { throw 'Dependency installation failed.' }
}
& $taskPython app.py
if ($LASTEXITCODE -ne 0) { throw 'OTA UI failed. See errors above.' }
