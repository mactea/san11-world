param([Parameter(Mandatory=$true)][string]$Folder, [Parameter(Mandatory=$true)][string]$MasterImage)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$portraitDir=[System.IO.Path]::GetFullPath($Folder)
New-Item -ItemType Directory -Force -Path $portraitDir | Out-Null
$portraitMaster=Join-Path $portraitDir 'portrait_master.png'
Copy-Item -LiteralPath $MasterImage -Destination $portraitMaster
$portraitSource=[System.Drawing.Image]::FromFile($portraitMaster)
$portraitSide=[Math]::Min($portraitSource.Width,$portraitSource.Height)
$portraitLeft=[int](($portraitSource.Width-$portraitSide)/2)
foreach($portraitSize in @(@(240,240),@(64,80),@(32,40))) {
    $portraitW=$portraitSize[0]; $portraitH=$portraitSize[1]
    $portraitBitmap=[System.Drawing.Bitmap]::new($portraitW,$portraitH,[System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $portraitGraphics=[System.Drawing.Graphics]::FromImage($portraitBitmap)
    $portraitGraphics.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $portraitGraphics.PixelOffsetMode=[System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    if($portraitW -eq 240) {
        $portraitRect=[System.Drawing.Rectangle]::new($portraitLeft,0,$portraitSide,$portraitSide)
    } else {
        $portraitRect=[System.Drawing.Rectangle]::new($portraitLeft+[int]($portraitSide*.2),0,[int]($portraitSide*.64),[int]($portraitSide*.8))
    }
    $portraitDest=[System.Drawing.Rectangle]::new(0,0,$portraitW,$portraitH)
    $portraitGraphics.DrawImage($portraitSource,$portraitDest,$portraitRect,[System.Drawing.GraphicsUnit]::Pixel)
    $portraitBase=Join-Path $portraitDir "portrait_$($portraitW)x$($portraitH)"
    $portraitBitmap.Save("$portraitBase.png",[System.Drawing.Imaging.ImageFormat]::Png)
    $portraitBitmap.Save("$portraitBase.bmp",[System.Drawing.Imaging.ImageFormat]::Bmp)
    $portraitGraphics.Dispose(); $portraitBitmap.Dispose()
}
$portraitSource.Dispose()
