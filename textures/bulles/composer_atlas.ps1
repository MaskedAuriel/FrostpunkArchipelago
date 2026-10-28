# composer_atlas.ps1
# ---------------------------------------------------------------------------
# Place les icones de icones\<nom>.png dans les cases de l'atlas des bulles
# (02350_origine.png -> 02350_modifie.png). Chaque icone est recadree sur
# son contenu visible (alpha > 0), redimensionnee sans deformation pour
# tenir dans la case (marge comprise), centree ; la case est videe avant.
# Produit aussi controle.png (atlas sur fond sombre, cases encadrees) et
# planche.png (les icones seules).
# ---------------------------------------------------------------------------
param(
    [int]$Marge = 8     # pixels laisses libres autour de l'icone dans la case de 128
)
Add-Type -AssemblyName System.Drawing
$d = $PSScriptRoot
$case = 128
# Placements : nom (affiche), fichier source dans icones\, case (col, ligne),
# echelle (1 = toute la case moins la marge). Doit correspondre a
# ARCHIPELAGO_ICONS_TECH (bulleCol, bulleRow) et aux $cases du script de build.
$placements = @(
    @{ nom = 'vanilla';        fichier = 'vanilla';     col = 7;  lig = 1; echelle = 1.0 },
    @{ nom = 'filler';         fichier = 'filler';      col = 8;  lig = 1; echelle = 1.0 },
    @{ nom = 'important';      fichier = 'important';   col = 9;  lig = 1; echelle = 1.0 },
    @{ nom = 'progressive';    fichier = 'progressive'; col = 10; lig = 1; echelle = 1.0 },
    @{ nom = 'trap';           fichier = 'trap';        col = 11; lig = 1; echelle = 1.0 },
    @{ nom = 'done';           fichier = 'done';        col = 12; lig = 1; echelle = 1.0 },
    # Icone des groupes (carte, liste a droite) : vanilla reduite de 15 %.
    @{ nom = 'vanilla_groupe'; fichier = 'vanilla';     col = 8;  lig = 4; echelle = 0.85 }
)

# Rectangle du contenu visible d'une image (alpha > 8).
function Contenu([Drawing.Bitmap]$img) {
    $r = New-Object Drawing.Rectangle 0, 0, $img.Width, $img.Height
    $data = $img.LockBits($r, 'ReadOnly', [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $n = $data.Stride * $img.Height
    $px = New-Object byte[] $n
    [Runtime.InteropServices.Marshal]::Copy($data.Scan0, $px, 0, $n)
    $img.UnlockBits($data)
    $minX = $img.Width; $minY = $img.Height; $maxX = -1; $maxY = -1
    for ($y = 0; $y -lt $img.Height; $y++) {
        $ligne = $y * $data.Stride
        for ($x = 0; $x -lt $img.Width; $x++) {
            if ($px[$ligne + $x * 4 + 3] -gt 8) {
                if ($x -lt $minX) { $minX = $x }; if ($x -gt $maxX) { $maxX = $x }
                if ($y -lt $minY) { $minY = $y }; if ($y -gt $maxY) { $maxY = $y }
            }
        }
    }
    if ($maxX -lt 0) { return $r }
    return New-Object Drawing.Rectangle $minX, $minY, ($maxX - $minX + 1), ($maxY - $minY + 1)
}

$atlas = New-Object Drawing.Bitmap (Join-Path $d "02350_origine.png")
$atlas32 = $atlas.Clone((New-Object Drawing.Rectangle 0, 0, $atlas.Width, $atlas.Height), [Drawing.Imaging.PixelFormat]::Format32bppArgb)
$atlas.Dispose()
$g = [Drawing.Graphics]::FromImage($atlas32)
$g.InterpolationMode = 'HighQualityBicubic'; $g.SmoothingMode = 'HighQuality'; $g.PixelOffsetMode = 'HighQuality'
$g.CompositingMode = 'SourceCopy'

$planche = New-Object Drawing.Bitmap ($placements.Count * 160), 190
$gp = [Drawing.Graphics]::FromImage($planche); $gp.Clear([Drawing.Color]::FromArgb(40, 40, 48)); $gp.InterpolationMode = 'HighQualityBicubic'
$k = 0
foreach ($pl in $placements) {
    $nom = $pl.nom
    $f = Join-Path $d "icones\$($pl.fichier).png"
    if (-not (Test-Path $f)) { "absent : $f"; continue }
    $img = New-Object Drawing.Bitmap $f
    $cont = Contenu $img
    $util = ($case - 2 * $Marge) * $pl.echelle
    $echelle = [Math]::Min($util / $cont.Width, $util / $cont.Height)
    $lw = [int][Math]::Round($cont.Width * $echelle); $lh = [int][Math]::Round($cont.Height * $echelle)
    $cx = $pl.col * $case; $cy = $pl.lig * $case
    # Vider la case (transparent), puis dessiner l'icone centree.
    $g.CompositingMode = 'SourceCopy'
    $g.FillRectangle((New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(0, 0, 0, 0))), $cx, $cy, $case, $case)
    $g.CompositingMode = 'SourceOver'
    $dest = New-Object Drawing.Rectangle ($cx + [int](($case - $lw) / 2)), ($cy + [int](($case - $lh) / 2)), $lw, $lh
    $g.DrawImage($img, $dest, $cont, [Drawing.GraphicsUnit]::Pixel)
    "{0,-15} contenu {1}x{2} a ({3},{4}) -> {5}x{6} dans la case ({7},{8})" -f $nom, $cont.Width, $cont.Height, $cont.X, $cont.Y, $lw, $lh, $pl.col, $pl.lig
    $gp.DrawImage($img, (New-Object Drawing.Rectangle ($k * 160 + 10), 10, 140, 140), $cont, [Drawing.GraphicsUnit]::Pixel)
    $gp.DrawString($nom, (New-Object Drawing.Font "Arial", 10), [Drawing.Brushes]::White, ($k * 160 + 10), 160)
    $img.Dispose(); $k++
}
$g.Dispose(); $gp.Dispose()
$atlas32.Save((Join-Path $d "02350_modifie.png"), [Drawing.Imaging.ImageFormat]::Png)
$planche.Save((Join-Path $d "planche.png")); $planche.Dispose()

# Controle : la zone des cases, agrandie, sur fond sombre.
$zoom = 2; $x0 = 6 * $case; $y0 = 0; $lz = 8 * $case; $hz = 6 * $case
$ctrl = New-Object Drawing.Bitmap ($lz * $zoom / 2), ($hz * $zoom / 2)
$gc = [Drawing.Graphics]::FromImage($ctrl); $gc.Clear([Drawing.Color]::FromArgb(40, 40, 48)); $gc.InterpolationMode = 'HighQualityBicubic'
$gc.DrawImage($atlas32, (New-Object Drawing.Rectangle 0, 0, $ctrl.Width, $ctrl.Height), (New-Object Drawing.Rectangle $x0, $y0, $lz, $hz), [Drawing.GraphicsUnit]::Pixel)
$pen = New-Object Drawing.Pen ([Drawing.Color]::FromArgb(160, 60, 255, 120)), 2
$s = $zoom / 2
foreach ($pl in $placements) { $gc.DrawRectangle($pen, ($pl.col * $case - $x0) * $s, ($pl.lig * $case - $y0) * $s, $case * $s, $case * $s) }
$gc.Dispose(); $ctrl.Save((Join-Path $d "controle.png")); $ctrl.Dispose()
$atlas32.Dispose()
"ok"
