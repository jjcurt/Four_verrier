<?php
require_once __DIR__ . '/config.php';
require_once __DIR__ . '/_layout.php';

$sessions = db()->query("
    SELECT s.id, s.program_name, s.firing_type, s.log_type, s.start_datetime,
           COUNT(DISTINCT d.id)    AS nb_points,
           MAX(d.elapsed_sec)/60  AS duration_min,
           COUNT(DISTINCT p.id)   AS nb_photos,
           MIN(p.filename)        AS first_photo
    FROM sessions s
    LEFT JOIN datapoints d ON d.session_id = s.id
    LEFT JOIN photos     p ON p.session_id = s.id
    GROUP BY s.id
    ORDER BY s.start_datetime DESC
")->fetchAll();

layoutHead('Comparer', '<script src="https://cdn.plot.ly/plotly-2.35.2.min.js" defer></script>');
?>

<h1>Comparer des sessions</h1>
<p style="color:var(--muted);margin-bottom:16px;font-size:.9rem;">
    Sélectionnez jusqu'à 4 sessions puis cliquez sur <strong>Afficher</strong>.
</p>

<form id="cmpForm">
    <div class="cards" style="margin-bottom:16px;" id="selCards">
        <?php foreach ($sessions as $s): sessionCard($s, true); endforeach; ?>
    </div>

    <div style="display:flex;gap:10px;flex-wrap:wrap;margin-bottom:20px;">
        <button type="button" class="btn btn-primary" onclick="loadComparison()">📊 Afficher</button>
        <button type="button" class="btn btn-grey"    onclick="clearSelection()">✕ Tout désélectionner</button>
    </div>
</form>

<div id="chart-compare" style="display:none;"></div>

<script>
function toggleCheck(card) {
    const cb = card.querySelector('.sel-check');
    if (!cb) return;
    const checked = document.querySelectorAll('.sel-check:checked');
    if (!cb.checked && checked.length >= 4) {
        alert('Maximum 4 sessions à comparer.');
        return;
    }
    cb.checked = !cb.checked;
    card.classList.toggle('selected', cb.checked);
}

function clearSelection() {
    document.querySelectorAll('.sel-check').forEach(cb => {
        cb.checked = false;
        cb.closest('.card')?.classList.remove('selected');
    });
}

function loadComparison() {
    const ids = [...document.querySelectorAll('.sel-check:checked')].map(c => c.value);
    if (ids.length < 2) { alert('Sélectionnez au moins 2 sessions.'); return; }

    fetch('/api.php?action=chart_multi&ids=' + ids.join(','))
        .then(r => r.json())
        .then(series => {
            const colors = ['#1B5E20','#1565C0','#E65100','#6A1B9A'];
            const traces = [];
            series.forEach((s, i) => {
                const c = colors[i % colors.length];
                traces.push({ x: s.elapsed, y: s.temp, mode: 'lines',
                    line: { color: c, width: 2 }, name: s.label });
                traces.push({ x: s.elapsed, y: s.target, mode: 'lines',
                    line: { color: c, dash: 'dot', width: 1 },
                    name: s.label + ' (consigne)', showlegend: false });
            });

            const layout = {
                xaxis:  { title: 'Temps (min)', gridcolor: '#eee' },
                yaxis:  { title: 'Température (°C)', gridcolor: '#eee' },
                legend: { orientation: 'h', y: -0.2 },
                margin: { t: 16, r: 30, b: 60, l: 55 },
                paper_bgcolor: 'white', plot_bgcolor: '#fafafa',
            };

            const el = document.getElementById('chart-compare');
            el.style.display = 'block';
            el.scrollIntoView({ behavior: 'smooth', block: 'start' });
            Plotly.newPlot('chart-compare', traces, layout, { responsive: true, displaylogo: false });
        })
        .catch(e => alert('Erreur : ' + e));
}
</script>

<?php layoutFoot(); ?>
