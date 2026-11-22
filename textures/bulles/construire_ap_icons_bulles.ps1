# construire_ap_icons_bulles.ps1
# ---------------------------------------------------------------------------
# Produit mods\ap_icons_bulles.gz : substitution de l'entree 2350 de
# textures-s3 (ui/tiles/icons.texture = UI\Tiles\Icons.dds, atlas des bulles
# de dilemme, DXT5 2048 x 2048, grille 16 x 16 de cases de 128 px).
#
# Methode de l'auteur (Frost\Nouveau dossier\RemplacerBlocAtlas1179.txt) :
# le blob d'origine est decompresse, et SEULS les blocs DXT des cases
# modifiees sont remplaces, dans chaque niveau de mip, par ceux que texconv
# produit depuis l'image retouchee. Le reste reste identique a l'original.
#
# Format d'un blob : gzip de { en-tete du jeu de 144 octets ; mips DXT5 du
# plus petit (1x1) au plus grand }.
#
# Usage :
#   .\construire_ap_icons_bulles.ps1                    (02350_modifie.png)
#   .\construire_ap_icons_bulles.ps1 -Source autre.png
#   .\construire_ap_icons_bulles.ps1 -Deployer          (copie dans mods\ si le jeu est ferme)
# ---------------------------------------------------------------------------
param(
    [string]$Source = (Join-Path $PSScriptRoot "02350_modifie.png"),   # memes dimensions que l'atlas
    [switch]$Deployer
)

# --- reglages ---
$cible    = 2350
$nCols    = 16
$nRows    = 16
$cases    = @(            # icon_vanilla, filler, important, progressive, trap, done (TechIcons.bulleCol/Row)
    @(7, 1),
    @(8, 1),
    @(9, 1),
    @(10, 1),
    @(11, 1),
    @(12, 1),
    @(8, 4)               # icon_vanilla_groupe (vanilla reduite de 15 %)
)
$jeu      = "C:\Program Files\Epic Games\Frostpunk"
$sortie   = Join-Path $PSScriptRoot "ap_icons_bulles.gz"
$dat      = "$jeu\textures-s3.dat"
$idx      = "$jeu\textures-s3.idx"
# texconv (https://github.com/microsoft/DirectXTex/releases) : Texconv\ a la racine du projet
$texconv  = Join-Path $PSScriptRoot "..\..\Texconv\texconv.exe"
$sevenZip = "C:\Program Files\7-Zip\7z.exe"

if (-not (Test-Path $Source)) { "Source introuvable : $Source"; return }

# --- index : decalage, place jusqu'au blob suivant, taille compressee ---
$tailleDat = (Get-Item $dat).Length
$b = [IO.File]::ReadAllBytes($idx)
$nEntrees = ($b.Length - 11) / 29
$e = for ($i = 0; $i -lt $nEntrees; $i++) {
    $p = 11 + $i * 29
    [PSCustomObject]@{ Index = $i; Off = [BitConverter]::ToUInt64($b, $p + 20); Comp = [BitConverter]::ToUInt64($b, $p + 4) }
}
$tri = $e | Sort-Object Off
$len = @{}
for ($i = 0; $i -lt $tri.Count - 1; $i++) { $len[$tri[$i].Index] = [int64]($tri[$i + 1].Off - $tri[$i].Off) }
$len[$tri[-1].Index] = [int64]($tailleDat - $tri[-1].Off)

$c  = $e[$cible]
$lc = [int]$len[$cible]
# Limite retenue : la plus petite de la place et de la taille compressee de
# l'index (longueur que le jeu lit sans doute).
$limite = [Math]::Min($lc, [int]$c.Comp)
"blob #$cible  offset=0x{0:X}  place={1}  taille index={2}  limite={3}" -f $c.Off, $lc, $c.Comp, $limite

# --- decompression de l'original ---
# On lit EXACTEMENT la taille compressee de l'index : avec toute la place
# (jusqu'au blob suivant), le GZipStream de .NET bute sur les octets qui
# suivent le gzip, leve une exception (avalee) et perd la fin du blob
# (5 570 560 octets au lieu de 5 592 576 : d'ou l'ancien 02350_2048x2048.dds
# tronque).
$aLire = [int]$c.Comp
$fs = [IO.File]::OpenRead($dat); $fs.Position = [int64]$c.Off
$orig = [byte[]]::new($aLire); $lu = 0
while ($lu -lt $aLire) { $r = $fs.Read($orig, $lu, $aLire - $lu); if ($r -le 0) { break }; $lu += $r }
$fs.Close()
$out = [IO.MemoryStream]::new()
try {
    $in = [IO.MemoryStream]::new($orig, 0, $lu)
    $gz = [IO.Compression.GZipStream]::new($in, [IO.Compression.CompressionMode]::Decompress)
    $tmp = [byte[]]::new(65536)
    while ($true) { $k = $gz.Read($tmp, 0, $tmp.Length); if ($k -le 0) { break }; $out.Write($tmp, 0, $k) }
    $gz.Dispose()
} catch { }
$brut = $out.ToArray()

$w = [int][BitConverter]::ToUInt32($brut, 0)
$h = [int][BitConverter]::ToUInt32($brut, 4)
"original : $w x $h,  $($brut.Length) octets  (case {0}x{1} px)" -f ($w / $nCols), ($h / $nRows)

# --- chaine de mips (DXT : 1 octet par pixel) ---
$dims = @(); $tailles = @(); $dw = $w; $dh = $h
while ($true) {
    $bw = [int][Math]::Max(1, [Math]::Ceiling($dw / 4.0))
    $bh = [int][Math]::Max(1, [Math]::Ceiling($dh / 4.0))
    $dims += , @($dw, $dh); $tailles += ($bw * $bh * 16)
    if ($dw -le 1 -and $dh -le 1) { break }
    $dw = [Math]::Max(1, [int]($dw / 2)); $dh = [Math]::Max(1, [int]($dh / 2))
}
$total = 0; foreach ($t in $tailles) { $total += $t }
"chaine : $($dims.Count) niveaux, total $total  (donnees du blob : $($brut.Length - 144))"
if ($total -ne $brut.Length - 144) { "ATTENTION : la chaine ne correspond pas, on s'arrete"; return }

# offsets dans le BLOB : du plus petit au plus grand a partir de 144
$posBlob = New-Object 'int[]' $dims.Count
$p = 144
for ($i = $dims.Count - 1; $i -ge 0; $i--) { $posBlob[$i] = $p; $p += $tailles[$i] }
"niveau 0 a l'offset $($posBlob[0])"

# --- mips de reference produits par texconv depuis notre image ---
$tmpDir = Join-Path $env:TEMP "fp_mips"
New-Item -ItemType Directory -Force -Path $tmpDir | Out-Null
Get-ChildItem $tmpDir -Recurse -File -ErrorAction SilentlyContinue | Remove-Item -Force
$dirOut = Join-Path $tmpDir "out"; New-Item -ItemType Directory -Force -Path $dirOut | Out-Null
$log = & $texconv -f BC3_UNORM -m $dims.Count -dx9 -ft dds -o $dirOut -y $Source 2>&1
$gen = Get-ChildItem $dirOut -Filter *.dds | Select-Object -First 1
if (-not $gen) {
    "texconv a echoue :"
    $log | % { "   $_" }
    return
}
$ref = [IO.File]::ReadAllBytes($gen.FullName)
$rw = [BitConverter]::ToUInt32($ref, 16); $rh = [BitConverter]::ToUInt32($ref, 12)
if ($rw -ne $w -or $rh -ne $h) { "La source fait $rw x $rh, l'atlas $w x $h : on s'arrete"; return }

$posTex = New-Object 'int[]' $dims.Count
$p = 128
for ($i = 0; $i -lt $dims.Count; $i++) { $posTex[$i] = $p; $p += $tailles[$i] }

# --- patch des cases dans CHAQUE niveau ---
for ($i = 0; $i -lt $dims.Count; $i++) {
    $lw = $dims[$i][0]; $lh = $dims[$i][1]
    $bpl = [int][Math]::Max(1, [Math]::Ceiling($lw / 4.0))
    $bpc = [int][Math]::Max(1, [Math]::Ceiling($lh / 4.0))
    $cw  = [int]($bpl / $nCols)
    $ch  = [int]($bpc / $nRows)
    if ($cw -lt 1 -or $ch -lt 1) { continue }

    $n = 0
    foreach ($cs in $cases) {
        $col = $cs[0]; $row = $cs[1]
        for ($by = $row * $ch; $by -lt ($row + 1) * $ch; $by++) {
            for ($bx = $col * $cw; $bx -lt ($col + 1) * $cw; $bx++) {
                $o = ($by * $bpl + $bx) * 16
                if ($posTex[$i] + $o + 16 -le $ref.Length -and $posBlob[$i] + $o + 16 -le $brut.Length) {
                    [Array]::Copy($ref, $posTex[$i] + $o, $brut, $posBlob[$i] + $o, 16); $n++
                }
            }
        }
    }
    "  niveau $i ($lw x $lh) : $n bloc(s)"
}

# --- compression ---
$ms = [IO.MemoryStream]::new()
$gz = [IO.Compression.GZipStream]::new($ms, [IO.Compression.CompressionLevel]::Optimal, $true)
$gz.Write($brut, 0, $brut.Length); $gz.Close()
$comp = $ms.ToArray(); $ms.Close()
"  .NET  : $($comp.Length)"
if (Test-Path $sevenZip) {
    $d2 = Join-Path $env:TEMP "fp_gz"; New-Item -ItemType Directory -Force -Path $d2 | Out-Null
    Get-ChildItem $d2 -File -ErrorAction SilentlyContinue | Remove-Item -Force
    $raw = Join-Path $d2 "blob.bin"; $gzf = Join-Path $d2 "blob.bin.gz"
    [IO.File]::WriteAllBytes($raw, $brut)
    & $sevenZip a -tgzip -mx9 -mfb=258 -mpass=15 $gzf $raw | Out-Null
    if (Test-Path $gzf) { $c7 = [IO.File]::ReadAllBytes($gzf); "  7-Zip : $($c7.Length)"; if ($c7.Length -lt $comp.Length) { $comp = $c7 } }
}
"retenu : {0} / limite {1}   (marge {2})" -f $comp.Length, $limite, ($limite - $comp.Length)
if ($comp.Length -gt $limite) { "TROP GROS de {0}" -f ($comp.Length - $limite); return }
[IO.File]::WriteAllBytes($sortie, $comp)
"ecrit : $sortie"

# --- deploiement ---
if ($Deployer) {
    if (Get-Process Frostpunk -ErrorAction SilentlyContinue) { "Jeu en cours : copie non faite (fermer Frostpunk)." }
    else { Copy-Item $sortie "$jeu\mods\ap_icons_bulles.gz" -Force; "copie dans $jeu\mods\ap_icons_bulles.gz" }
}
