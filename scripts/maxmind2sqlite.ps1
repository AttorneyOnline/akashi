<#PSScriptInfo
.VERSION 1.0
.GUID aca39872-c8c6-434f-98fe-a6e95be92aa7
#>

<# 
.DESCRIPTION 
 Best-effort MaxMind ASN CSV database to SQLite converter. 
#> 

$sDatabasePath= "$PSScriptRoot\storage\asn.sqlite3"
$license_key = ""
$uri_maxmind = [string]::Format("https://download.maxmind.com/app/geoip_download?edition_id=GeoLite2-ASN-CSV&license_key={0}&suffix=zip",$license_key)

Write-Host("Checking and downloading dependencies.")

$uri_sqlite = "https://system.data.sqlite.org/blobs/1.0.118.0/sqlite-netFx451-binary-bundle-x64-2013-1.0.118.0.zip"
if (!(Test-Path -Path "$PSScriptRoot\bin\System.Data.SQLite.dll")) {
    Write-Host("Downloading SQlite3!")
    Start-BitsTransfer $uri_sqlite -Destination "$PSScriptRoot\sqlite3.zip" -HttpMethod GET
    Expand-Archive -Path "$PSScriptRoot\sqlite3.zip" -DestinationPath "$PSScriptRoot\bin" -Force
    Remove-Item -Path "$PSScriptRoot\sqlite3.zip" -Force
}
else {
    Write-Host("sqlite3 found!") -ForegroundColor Green
}

if (!(Test-Path -Path "$PSScriptRoot\storage")) {
    New-Item -Path "$PSScriptRoot\storage" -ItemType Directory
}

if (Test-Path -Path "$PSScriptRoot\storage\asn.sqlite3") {
    Remove-Item -Path "$PSScriptRoot\storage\asn.sqlite3" -Force
}
New-Item -Path "$PSScriptRoot\storage\asn.sqlite3" -ItemType File -Force

if (![string]::IsNullOrEmpty($license_key)) {

Write-Host($uri_maxmind)
    Write-Host("MaxMind License key available. Trying to download the database.")
    #Maxmiund has issues when I use BITS. So sad.
    try {
        Invoke-WebRequest -Uri $uri_maxmind -OutFile "$PSScriptRoot\maxmind.zip" -Method GET -ErrorAction Stop
    }
    catch {
        Write-Host("Unable to download MaxMind CSV database. Aborting script. Please check your license key and try again later.")
        exit
    }
    Expand-Archive -Path "$PSScriptRoot\maxmind.zip" -DestinationPath "$PSScriptRoot\storage" -Force
    Remove-Item -Path "$PSScriptRoot\maxmind.zip" -Force
}


$ipv4 = Get-ChildItem -Recurse -Path "$PSScriptRoot\storage\*IPv4.csv" | Get-Content | ConvertFrom-Csv -Delimiter ","
$ipv6 = Get-ChildItem -Recurse -Path "$PSScriptRoot\storage\*IPv6.csv" | Get-Content | ConvertFrom-Csv -Delimiter ","

[Reflection.Assembly]::LoadFile("$PSScriptRoot\bin\System.Data.SQLite.dll")
$sDatabaseConnectionString=[string]::Format("data source={0}",$sDatabasePath)
$oSQLiteDBConnection = New-Object System.Data.SQLite.SQLiteConnection
$oSQLiteDBConnection.ConnectionString = $sDatabaseConnectionString
$oSQLiteDBConnection.open()

$oSQLiteDBCommand=$oSQLiteDBConnection.CreateCommand()
$oSQLiteDBCommand.Commandtext='CREATE TABLE "maxmind" (
	"ip"	TEXT,
	"asn"	INTEGER,
	"organization"	TEXT,
	"type"	INTEGER
);'
$oSQLiteDBCommand.CommandType = [System.Data.CommandType]::Text
$oSQLiteDBCommand.ExecuteReader()

Write-Host("Inserting IPv4 entries.")
foreach($entry in $ipv4) {
    $oSQLiteDBInsertCommand = $oSQLiteDBConnection.CreateCommand()
    $oSQLiteDBInsertCommand.Commandtext='INSERT INTO maxmind (ip, asn, organization, type) VALUES (@ip_addr, @asn_id, @org, 4)'
    $oSQLiteDBInsertCommand.Parameters.AddWithValue("ip_addr", $entry.network) | Out-Null
    $oSQLiteDBInsertCommand.Parameters.AddWithValue("asn_id", $entry.autonomous_system_number) | Out-Null
    $oSQLiteDBInsertCommand.Parameters.AddWithValue("org", $entry.autonomous_system_organization) | Out-Null
    $oSQLiteDBInsertCommand.CommandType = [System.Data.CommandType]::Text
    $oSQLiteDBInsertCommand.ExecuteNonQuery() | Out-Null
    $i++
}

Write-Host("Inserting IPv6 entries.")
foreach($entry in $ipv6) {
    $oSQLiteDBInsertCommand = $oSQLiteDBConnection.CreateCommand()
    $oSQLiteDBInsertCommand.Commandtext='INSERT INTO maxmind (ip, asn, organization, type) VALUES (@ip_addr, @asn_id, @org, 6)'
    $oSQLiteDBInsertCommand.Parameters.AddWithValue("ip_addr", $entry.network) | Out-Null
    $oSQLiteDBInsertCommand.Parameters.AddWithValue("asn_id", $entry.autonomous_system_number) | Out-Null
    $oSQLiteDBInsertCommand.Parameters.AddWithValue("org", $entry.autonomous_system_organization) | Out-Null
    $oSQLiteDBInsertCommand.CommandType = [System.Data.CommandType]::Text
    $oSQLiteDBInsertCommand.ExecuteNonQuery() | Out-Null
}
$oSQLiteDBConnection.Close()