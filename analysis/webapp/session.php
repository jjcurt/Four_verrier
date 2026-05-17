<?php
require_once __DIR__ . '/config.php';
require_once __DIR__ . '/_layout.php';

$id = (int)($_GET['id'] ?? 0);
if (!$id) { header('Location: /index.php'); exit; }

$meta = db()->query("SELECT * FROM sessions WHERE id=$id")->fetch();
if (!$meta) { header('Location: /index.php'); exit; }

$photos = db()->query("
    SELECT filename, caption FROM photos WHERE session_id=$id ORDER BY id
")->fetchAll();

$duration = db()->query("
    SELECT MAX(elapsed_sec)/60.0 AS d FROM datapoints WHERE session_id=$id
")->fetch()['d'];

$title = ($meta['program_name'] ?? '—') . ' — ' . substr($meta['start_datetime'] ?? '', 0, 10);
layoutHead($title, '<script src="https://cdn.plot.ly/plotly-2.35.2.min.js" defer></script>');
?>

<a href="/index.php" class="btn btn-grey btn-sm" style="margin-bottom:14px;">← Retour</a>
<h1><?= h($meta['program_name'] ?? '—') ?> <?= typeBadge($meta['firing_type'] ?? '') ?></h1>

<!-- Métadonnées -->
<div class="meta-grid">
    <div class="meta-item">
        <div class="label">Date</div>
        <div class="value"><?= h(substr($meta['start_datetime'] ?? '', 0, 16)) ?></div>
    </div>
    <div class="meta-item">
        <div class="label">Durée</div>
        <div class="value"><?= $duration ? round($duration) . ' min' : '—' ?></div>
    </div>
    <div class="meta-item">
        <div class="label">Firmware</div>
        <div class="value"><?= h($meta['firmware_version'] ?? '—') ?></div>
    </div>
    <div class="meta-item">
        <div class="label">PID</div>
        <div class="value" style="font-size:.85rem">
            Kp <?= $meta['kp'] ?? '—' ?> · Ki <?= $meta['ki'] ?? '—' ?> · Kd <?= $meta['kd'] ?? '—' ?>
        </div>
    </div>
    <?php if ($meta['num_steps']): ?>
    <div class="meta-item">
        <div class="label">Étapes</div>
        <div class="value"><?= (int)$meta['num_steps'] ?></div>
    </div>
    <?php endif; ?>
    <div class="meta-item">
        <div class="label">Type log</div>
        <div class="value"><?= h($meta['log_type'] ?? '—') ?></div>
    </div>
</div>

<!-- Graphe de profil -->
<h2>Profil de température</h2>
<div id="chart"></div>

<!-- Photos -->
<?php if (!empty($photos)): ?>
<h2>Photos</h2>
<div class="photo-grid">
    <?php foreach ($photos as $p):
        $url = PHOTOS_URL . '/session_' . $id . '/' . rawurlencode($p['filename']);
    ?>
    <div class="photo-card">
        <a href="<?= h($url) ?>" target="_blank" title="Ouvrir en taille réelle">
            <img src="<?= h($url) ?>" alt="<?= h($p['caption'] ?? $p['filename']) ?>" loading="lazy">
        </a>
        <?php if ($p['caption']): ?>
            <div class="photo-caption"><?= h($p['caption']) ?></div>
        <?php endif; ?>
    </div>
    <?php endforeach; ?>
</div>
<?php endif; ?>

<script>
document.addEventListener('DOMContentLoaded', () => {
    fetch('/api.php?action=chart&id=<?= $id ?>')
        .then(r => r.json())
        .then(buildChart)
        .catch(e => { document.getElementById('chart').textContent = 'Erreur : ' + e; });
});

function buildChart(d) {
    const traces = [];

    // Consigne
    traces.push({ x: d.elapsed, y: d.target, mode: 'lines',
        line: { color: '#FF6F00', dash: 'dash', width: 2 },
        name: 'Consigne', yaxis: 'y' });

    // Température (filtrée si dispo, sinon courante)
    const tempY = d.filtered.some(v => v !== null) ? d.filtered : d.temp;
    traces.push({ x: d.elapsed, y: tempY, mode: 'lines',
        line: { color: '#1B5E20', width: 2.5 },
        name: 'Température', yaxis: 'y' });

    // Brute (masquée par défaut, accessible via légende)
    if (d.raw.some(v => v !== null)) {
        traces.push({ x: d.elapsed, y: d.raw, mode: 'lines',
            line: { color: '#BDBDBD', width: 1 },
            name: 'Brute (capteur)', yaxis: 'y', visible: 'legendonly' });
    }

    // Puissance SSR (axe droit)
    if (d.pwm.some(v => v !== null)) {
        traces.push({ x: d.elapsed, y: d.pwm, mode: 'lines', fill: 'tozeroy',
            line: { color: 'rgba(230,74,25,0)', width: 0 },
            fillcolor: 'rgba(230,74,25,0.25)',
            name: 'Puissance (%)', yaxis: 'y2' });
    }

    // Couleurs de phase (rectangles en arrière-plan)
    const phaseColors = { RAMP:'#E3F2FD', HOLD:'#FFF9C4', STABILIZING:'#FCE4EC',
                          BOOST:'#FBE9E7', IDLE:'#F5F5F5' };
    const shapes = [];
    let prevPhase = null, prevX = d.elapsed[0];
    d.phase.forEach((ph, i) => {
        if (ph !== prevPhase) {
            if (prevPhase && phaseColors[prevPhase]) {
                shapes.push({ type:'rect', xref:'x', yref:'paper',
                    x0: prevX, x1: d.elapsed[i], y0: 0, y1: 1,
                    fillcolor: phaseColors[prevPhase], opacity: 0.35,
                    line: { width: 0 }, layer: 'below' });
            }
            prevPhase = ph; prevX = d.elapsed[i];
        }
    });
    if (prevPhase && phaseColors[prevPhase]) {
        shapes.push({ type:'rect', xref:'x', yref:'paper',
            x0: prevX, x1: d.elapsed[d.elapsed.length - 1], y0: 0, y1: 1,
            fillcolor: phaseColors[prevPhase], opacity: 0.35,
            line: { width: 0 }, layer: 'below' });
    }

    const layout = {
        xaxis:  { title: 'Temps (min)', gridcolor: '#eee' },
        yaxis:  { title: 'Température (°C)', gridcolor: '#eee' },
        yaxis2: { title: 'Puissance (%)', overlaying: 'y', side: 'right',
                  range: [0, 120], showgrid: false },
        legend: { orientation: 'h', y: -0.18 },
        margin: { t: 16, r: 60, b: 60, l: 55 },
        shapes,
        paper_bgcolor: 'white',
        plot_bgcolor:  '#fafafa',
    };

    Plotly.newPlot('chart', traces, layout, { responsive: true, displaylogo: false });
}
</script>

<?php layoutFoot(); ?>
