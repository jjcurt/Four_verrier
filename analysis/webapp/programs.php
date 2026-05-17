<?php
require_once __DIR__ . '/config.php';
require_once __DIR__ . '/_layout.php';

// Lire tous les programmes depuis data/programs/
$programs = [];
foreach (glob(PROGRAMS_DIR . '/*.json') as $path) {
    $raw = @file_get_contents($path);
    if (!$raw) continue;
    $p = json_decode($raw, true);
    if (!$p) continue;
    $programs[] = [
        'filename' => basename($path),
        'name'     => $p['name'] ?? basename($path, '.json'),
        'type'     => $p['type'] ?? '',
        'steps'    => count($p['steps'] ?? []),
        'log'      => isset($p['enableMaintenanceLog']) ? 'maintenance'
                    : (isset($p['enableTestLog'])       ? 'test'
                    : (isset($p['enableDataLog'])       ? 'user' : '')),
    ];
}

// Tri : par type puis par nom
usort($programs, fn($a, $b) => strcmp($a['type'].$a['name'], $b['type'].$b['name']));

$typeFilter = $_GET['type'] ?? '';
$types      = firingTypes();

layoutHead('Programmes');
?>

<h1>Programmes de cuisson</h1>

<?php if (!empty($types)): ?>
<div class="filter-bar">
    <label>Type :</label>
    <select onchange="location.href='?type='+this.value">
        <option value="">Tous</option>
        <?php foreach ($types as $id => $label): ?>
            <option value="<?= h($id) ?>" <?= $typeFilter===$id?'selected':'' ?>><?= h($label) ?></option>
        <?php endforeach; ?>
    </select>
    <?php if ($typeFilter !== ''): ?>
        <a href="?" class="btn btn-grey btn-sm">✕ Effacer</a>
    <?php endif; ?>
</div>
<?php endif; ?>

<?php
$visible = array_filter($programs, fn($p) => $typeFilter === '' || $p['type'] === $typeFilter);
if (empty($visible)): ?>
    <p class="alert alert-info">Aucun programme trouvé.</p>
<?php else:
    foreach ($visible as $p):
        $logLabel = ['maintenance'=>'Log maintenance','test'=>'Log test','user'=>'Log utilisateur'][$p['log']] ?? '';
        $espUrl   = 'http://192.168.0.100/download?path=/programs/' . rawurlencode($p['filename']);
?>
    <div class="prog-card">
        <div>
            <div class="prog-name"><?= h($p['name']) ?> <?= typeBadge($p['type']) ?></div>
            <div class="prog-meta">
                <?= $p['steps'] ?> étape<?= $p['steps']>1?'s':'' ?>
                <?php if ($logLabel): ?> · <?= h($logLabel) ?><?php endif; ?>
            </div>
        </div>
        <div class="prog-actions">
            <a class="btn btn-grey btn-sm"
               href="/program_view.php?file=<?= urlencode($p['filename']) ?>">👁 Voir</a>
            <a class="btn btn-primary btn-sm"
               href="/download_prog.php?file=<?= urlencode($p['filename']) ?>"
               download="<?= h($p['filename']) ?>">⬇ Télécharger</a>
        </div>
    </div>
<?php endforeach; endif; ?>

<div class="alert alert-info" style="margin-top:24px;">
    💡 Pour créer ou modifier un programme, utilisez l'interface du four :
    <a href="http://192.168.0.100/programs.html" target="_blank">192.168.0.100/programs.html</a>
</div>

<?php layoutFoot(); ?>
