<?php
require_once __DIR__ . '/config.php';
require_once __DIR__ . '/_layout.php';

$file = basename($_GET['file'] ?? '');
if (!preg_match('/^[\w\-]+\.json$/i', $file)) { header('Location: /programs.php'); exit; }

$path = PROGRAMS_DIR . '/' . $file;
if (!file_exists($path)) { header('Location: /programs.php'); exit; }

$raw = file_get_contents($path);
$p   = json_decode($raw, true);
if (!$p) { header('Location: /programs.php'); exit; }

$name  = $p['name']  ?? basename($file, '.json');
$type  = $p['type']  ?? '';
$steps = $p['steps'] ?? [];

layoutHead('Programme : ' . $name);
?>

<a href="/programs.php" class="btn btn-grey btn-sm" style="margin-bottom:16px;">← Programmes</a>
<h1><?= h($name) ?> <?= typeBadge($type) ?></h1>

<!-- Paramètres généraux -->
<h2>Paramètres généraux</h2>
<div class="meta-grid">

    <?php if (!empty($p['manualMode'])): ?>
    <div class="meta-item">
        <div class="label">Mode</div>
        <div class="value">Manuel — PWM <?= (int)($p['manualPWM'] ?? 512) ?>/1023
             (<?= round(($p['manualPWM'] ?? 512) / 1023 * 100) ?> %)</div>
    </div>
    <?php endif; ?>

    <?php
    $logType = isset($p['enableMaintenanceLog']) ? 'Maintenance'
             : (isset($p['enableTestLog'])       ? 'Test PID'
             : (isset($p['enableDataLog'])        ? 'Utilisateur' : '—'));
    ?>
    <div class="meta-item">
        <div class="label">Log</div>
        <div class="value"><?= $logType ?></div>
    </div>

    <?php if (!empty($p['dataLogInterval'])): ?>
    <div class="meta-item">
        <div class="label">Intervalle log</div>
        <div class="value"><?= (int)$p['dataLogInterval'] / 1000 ?> s</div>
    </div>
    <?php endif; ?>

    <div class="meta-item">
        <div class="label">Échant. adaptatif</div>
        <div class="value"><?= !empty($p['adaptiveLogging']) ? 'Oui' : 'Non' ?></div>
    </div>

    <?php if (!empty($p['disablePidReset'])): ?>
    <div class="meta-item">
        <div class="label">Reset PID</div>
        <div class="value">Désactivé entre étapes</div>
    </div>
    <?php endif; ?>

    <?php if (!empty($p['enableBoost'])): ?>
    <div class="meta-item">
        <div class="label">Boost initial</div>
        <div class="value">
            Seuil <?= $p['boostTempRise'] ?? '?' ?> °C,
            max <?= $p['boostMaxSec'] ?? '?' ?> s
        </div>
    </div>
    <?php endif; ?>

    <?php if (!empty($p['pidKp'])): ?>
    <div class="meta-item">
        <div class="label">PID spécifique</div>
        <div class="value" style="font-size:.85rem">
            Kp <?= $p['pidKp'] ?> · Ki <?= $p['pidKi'] ?? '—' ?> · Kd <?= $p['pidKd'] ?? '—' ?>
        </div>
    </div>
    <?php endif; ?>

    <div class="meta-item">
        <div class="label">Étapes</div>
        <div class="value"><?= count($steps) ?></div>
    </div>
</div>

<!-- Tableau des étapes -->
<?php if (!empty($steps)): ?>
<h2>Profil de cuisson</h2>
<div style="overflow-x:auto;">
<table style="width:100%;border-collapse:collapse;background:white;
              border-radius:10px;box-shadow:0 2px 8px rgba(0,0,0,.1);overflow:hidden;">
    <thead>
        <tr style="background:var(--green);color:white;text-align:left;">
            <th style="padding:10px 14px;">#</th>
            <th style="padding:10px 14px;">Cible (°C)</th>
            <th style="padding:10px 14px;">Rampe (°C/min)</th>
            <th style="padding:10px 14px;">Maintien (min)</th>
            <th style="padding:10px 14px;">Mode</th>
        </tr>
    </thead>
    <tbody>
    <?php foreach ($steps as $i => $s):
        $ramp    = $s['withRamp'] ?? false;
        $rate    = $ramp ? (($s['rampRate'] ?? 0) > 0 ? ($s['rampRate'] . ' °C/min') : 'Libre') : '—';
        $hold    = ($s['holdTime'] ?? 0) > 0 ? $s['holdTime'] . ' min' : '—';
        $rowBg   = $i % 2 === 0 ? '#fafafa' : 'white';
    ?>
        <tr style="background:<?= $rowBg ?>;">
            <td style="padding:10px 14px;font-weight:700;color:var(--green);"><?= $i + 1 ?></td>
            <td style="padding:10px 14px;font-weight:700;"><?= h((string)($s['targetTemp'] ?? '—')) ?> °C</td>
            <td style="padding:10px 14px;"><?= h($rate) ?></td>
            <td style="padding:10px 14px;"><?= h($hold) ?></td>
            <td style="padding:10px 14px;font-size:.82rem;color:var(--muted);">
                <?= $ramp ? 'Rampe' : 'Palier direct' ?>
            </td>
        </tr>
    <?php endforeach; ?>
    </tbody>
</table>
</div>
<?php endif; ?>

<!-- JSON brut (repliable) -->
<h2 style="margin-top:28px;">
    <button onclick="toggleJson()" style="background:none;border:none;cursor:pointer;
        font-size:1.15rem;font-weight:700;color:var(--green);padding:0;display:flex;
        align-items:center;gap:8px;">
        <span id="jsonArrow">▶</span> JSON source
    </button>
</h2>
<pre id="jsonBlock" style="display:none;background:#1a1a1a;color:#b0ffb0;
     font-family:monospace;font-size:.82rem;padding:16px;border-radius:8px;
     overflow-x:auto;white-space:pre-wrap;"><?= h(json_encode($p, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE)) ?></pre>

<div style="margin-top:24px;display:flex;gap:10px;">
    <a href="/download_prog.php?file=<?= urlencode($file) ?>"
       download="<?= h($file) ?>" class="btn btn-primary">⬇ Télécharger</a>
    <a href="/programs.php" class="btn btn-grey">← Retour</a>
</div>

<script>
function toggleJson() {
    const b = document.getElementById('jsonBlock');
    const a = document.getElementById('jsonArrow');
    const show = b.style.display === 'none';
    b.style.display = show ? 'block' : 'none';
    a.textContent   = show ? '▼' : '▶';
}
</script>

<?php layoutFoot(); ?>
