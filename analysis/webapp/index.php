<?php
require_once __DIR__ . '/config.php';
require_once __DIR__ . '/_layout.php';

$typeFilter = $_GET['type'] ?? '';
$types      = firingTypes();

layoutHead('Mes cuissons');
?>

<h1>Mes cuissons</h1>

<!-- Filtre par type -->
<div class="filter-bar">
    <label>Type :</label>
    <select onchange="location.href='?type='+this.value">
        <option value="" <?= $typeFilter==='' ? 'selected' : '' ?>>Tous</option>
        <?php foreach ($types as $id => $label): ?>
            <option value="<?= h($id) ?>" <?= $typeFilter===$id ? 'selected' : '' ?>>
                <?= h($label) ?>
            </option>
        <?php endforeach; ?>
    </select>
    <?php if ($typeFilter !== ''): ?>
        <a href="?" class="btn btn-grey btn-sm">✕ Effacer</a>
    <?php endif; ?>
</div>

<!-- Liste sessions -->
<?php
$where = '';
if ($typeFilter !== '') {
    $where = 'WHERE s.firing_type = ' . db()->quote($typeFilter);
}
$sessions = db()->query("
    SELECT s.id, s.program_name, s.firing_type, s.log_type,
           s.start_datetime,
           COUNT(DISTINCT d.id)      AS nb_points,
           MAX(d.elapsed_sec)/60.0   AS duration_min,
           COUNT(DISTINCT p.id)      AS nb_photos,
           MIN(p.filename)           AS first_photo
    FROM sessions s
    LEFT JOIN datapoints d ON d.session_id = s.id
    LEFT JOIN photos     p ON p.session_id = s.id
    $where
    GROUP BY s.id
    ORDER BY s.start_datetime DESC
")->fetchAll();
?>

<?php if (empty($sessions)): ?>
    <p class="alert alert-info">Aucune session enregistrée — utilisez <a href="/import.php">Importer</a> pour charger vos fichiers CSV.</p>
<?php else: ?>
    <div class="cards">
        <?php foreach ($sessions as $s): sessionCard($s); endforeach; ?>
    </div>
<?php endif; ?>

<?php layoutFoot(); ?>
