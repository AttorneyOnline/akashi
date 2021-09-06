<#PSScriptInfo

.VERSION 1.0

.GUID 5a2a17a2-99ba-4fce-b736-02762c6cb081

.AUTHOR https://github.com/Salanto

.LICENSEURI https://www.gnu.org/licenses/agpl-3.0.de.html

.RELEASENOTES Initial Release
#>

<# 
.DESCRIPTION 
 Creates an unsorted music list for the Akashi AO2 Server.
 Uses Folders as replacements for category.
#> 
$Path = (Get-Location)

$jsonFile = [System.Collections.ArrayList]::new();

function Get-CategorySongs { #Convenience function to add songs inside a folder into an array.
    param ([String]$folderPath)
    $list = New-Object System.Collections.ArrayList
    ForEach ($song in (Get-ChildItem -File -Path $folderPath\* -Include *.mp3, *.opus, *.ogg) | Split-Path -Leaf) {
        $list.Add( @{ 
            name = $song
            length = -1
        }) | Out-Null
    }
    return $list
}

$categories = Get-ChildItem -Directory -Path $path | Split-Path -Leaf
if ($categories.Length -eq 0) {
    $CategoryFormat = @{
        category = "==Music=="
        songs = (Get-CategorySongs -folderPath $Path)
    }
    $jsonFile = $CategoryFormat
}
else {
    ForEach ($category in $categories) {
        $CategoryFormat = [PSCustomObject]@{
            category = $category
            songs = (Get-CategorySongs -folderPath $Path\$category)
        }
        $jsonFile.add($CategoryFormat) | Out-Null
    }
}

if (Test-Path ".\music.json") {
    Set-Content -Path ".\music.json" -Value (ConvertTo-Json -InputObject $jsonFile -Depth 4 | ForEach-Object { [System.Text.RegularExpressions.Regex]::Unescape($_) })
}
else {
    New-Item -ItemType "File" -Path $path -Name "music.json" -Value (ConvertTo-Json -InputObject $jsonFile -Depth 4 | ForEach-Object { [System.Text.RegularExpressions.Regex]::Unescape($_) })
}

#Stupid hack moment because Powershell 5 is stupid :)
if (!(Get-Content -Raw -Path "$path\music.json").StartsWith("[")) {
    Set-Content -Path "$path\music.json" -Value ("[" + (Get-Content -Path "$path\music.json" -Raw) + "]")
}