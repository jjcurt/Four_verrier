<?php
// ---------------------------------------------------------------------------
// _layout.php — Fonctions header/footer partagées
// ---------------------------------------------------------------------------

function layoutHead(string $title, string $extraHead = ''): void {
    $pages = [
        'index.php'   => 'Cuissons',
        'compare.php' => 'Comparer',
        'programs.php'=> 'Programmes',
        'import.php'  => 'Importer',
    ];
    $current = basename($_SERVER['SCRIPT_FILENAME'] ?? '');
    ?>
<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
<title><?= h($title) ?> — <?= APP_TITLE ?></title>
<link rel="stylesheet" href="/assets/kiln.css">
<?= $extraHead ?>
</head>
<body>
<nav class="topnav">
    <span class="brand">🔥 <?= APP_TITLE ?></span>
    <div class="nav-links">
    <?php foreach ($pages as $file => $label): ?>
        <a href="/<?= $file ?>" class="<?= $file === $current ? 'active' : '' ?>"><?= $label ?></a>
    <?php endforeach; ?>
    </div>
</nav>
<main>
<?php
}

function layoutFoot(): void { ?>
</main>
<footer class="foot">
    <span>Four Verrier — Analyse</span>
</footer>
</body>
</html>
<?php
}

// Affiche un badge type de cuisson
function typeBadge(string $typeId): string {
    if ($typeId === '') return '';
    $label = firingLabel($typeId);
    $color = firingColor($typeId);
    return sprintf(
        '<span class="badge" style="background:%s">%s</span>',
        h($color), h($label)
    );
}

// Carte de session (utilisée dans index et compare)
function sessionCard(array $s, bool $selectable = false): void {
    $photoThumb = '';
    if ($s['first_photo'] !== null) {
        $url = PHOTOS_URL . '/session_' . $s['id'] . '/' . rawurlencode($s['first_photo']);
        $photoThumb = '<img class="card-thumb" src="' . h($url) . '" alt="" loading="lazy">';
    }
    $date     = substr($s['start_datetime'] ?? '', 0, 16);
    $duration = $s['duration_min'] !== null ? round($s['duration_min']) . ' min' : '—';
    $badge    = typeBadge($s['firing_type'] ?? '');
    $check    = $selectable ? '<input type="checkbox" name="ids[]" value="' . (int)$s['id'] . '" class="sel-check">' : '';
    ?>
    <div class="card" onclick="<?= $selectable ? 'toggleCheck(this)' : "location.href='/session.php?id=" . (int)$s['id'] . "'" ?>">
        <?= $check ?>
        <?= $photoThumb ?>
        <div class="card-body">
            <div class="card-title"><?= h($s['program_name'] ?? '—') ?> <?= $badge ?></div>
            <div class="card-meta">
                <span>📅 <?= h($date) ?></span>
                <span>⏱ <?= $duration ?></span>
                <span>📊 <?= number_format((int)$s['nb_points']) ?> pts</span>
                <?php if ($s['nb_photos'] > 0): ?>
                    <span>📷 <?= (int)$s['nb_photos'] ?></span>
                <?php endif; ?>
            </div>
        </div>
    </div>
    <?php
}
