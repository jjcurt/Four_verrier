<?php
require_once __DIR__ . '/config.php';
require_once __DIR__ . '/_layout.php';

$messages = [];
$output   = '';

// ── Traitement POST : upload et/ou lancement ingest.py ──────────────────────
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $action = $_POST['action'] ?? '';

    // -- Upload de fichiers CSV vers logs/ -----------------------------------
    if (!empty($_FILES['csvfiles']['tmp_name'][0])) {
        $logsDir = LOGS_DIR;
        if (!is_dir($logsDir)) mkdir($logsDir, 0755, true);

        $uploaded = 0;
        foreach ($_FILES['csvfiles']['tmp_name'] as $i => $tmp) {
            if (!is_uploaded_file($tmp)) continue;
            $name = basename($_FILES['csvfiles']['name'][$i]);
            if (!preg_match('/\.csv$/i', $name)) continue;
            if (move_uploaded_file($tmp, $logsDir . '/' . $name)) {
                $messages[] = ['ok', "Fichier reçu : $name"];
                $uploaded++;
            } else {
                $messages[] = ['err', "Échec copie : $name"];
            }
        }
    }

    // -- Lancement ingest.py -------------------------------------------------
    if ($action === 'scan' || !empty($_FILES['csvfiles']['tmp_name'][0])) {
        $cmd    = 'python3 ' . escapeshellarg(INGEST_PY) . ' 2>&1';
        $output = shell_exec($cmd) ?? 'Aucune sortie.';
        $messages[] = ['ok', 'Ingestion terminée.'];
    }
}

layoutHead('Importer des cuissons');
?>

<h1>Importer des cuissons</h1>

<?php foreach ($messages as [$type, $msg]): ?>
    <div class="alert alert-<?= $type==='ok'?'ok':'err' ?>"><?= h($msg) ?></div>
<?php endforeach; ?>

<form method="post" enctype="multipart/form-data" id="importForm">

    <!-- Zone de dépôt de fichiers CSV -->
    <h2>1. Déposer des fichiers CSV</h2>
    <p style="color:var(--muted);font-size:.9rem;margin-bottom:10px;">
        Fichiers générés par le four (maint_*.csv, test_*.csv, user_*.csv).
        Ils seront copiés dans le dossier <code>analysis/logs/</code>.
    </p>

    <div class="drop-zone" id="dropZone" onclick="document.getElementById('csvInput').click()">
        <input type="file" id="csvInput" name="csvfiles[]" multiple accept=".csv"
               onchange="updateDropLabel(this)">
        <div id="dropLabel">
            📂 Cliquer ou glisser-déposer vos fichiers CSV ici
        </div>
    </div>

    <!-- Bouton Importer (upload + scan) -->
    <div style="display:flex;gap:12px;flex-wrap:wrap;">
        <button type="submit" name="action" value="upload" class="btn btn-primary">
            ⬆ Envoyer et importer
        </button>

        <!-- Bouton scan seul (sans upload) -->
        <button type="submit" name="action" value="scan" class="btn btn-orange">
            🔄 Scanner logs/ existants
        </button>
    </div>

    <p style="font-size:.82rem;color:var(--muted);margin-top:8px;">
        <em>Scanner logs/ existants</em> : relance ingest.py sur les fichiers déjà présents
        dans analysis/logs/ (utile si vous les avez copiés manuellement).
    </p>
</form>

<?php if ($output !== ''): ?>
<h2>Résultat</h2>
<div class="output-box"><?= h($output) ?></div>
<?php endif; ?>

<div style="margin-top:28px;">
    <a href="/index.php" class="btn btn-grey">← Voir les cuissons</a>
</div>

<script>
// Drag & drop sur la drop zone
const zone = document.getElementById('dropZone');
const inp  = document.getElementById('csvInput');

zone.addEventListener('dragover', e => { e.preventDefault(); zone.classList.add('drag-over'); });
zone.addEventListener('dragleave', () => zone.classList.remove('drag-over'));
zone.addEventListener('drop', e => {
    e.preventDefault();
    zone.classList.remove('drag-over');
    const dt = new DataTransfer();
    [...e.dataTransfer.files].filter(f => f.name.endsWith('.csv')).forEach(f => dt.items.add(f));
    inp.files = dt.files;
    updateDropLabel(inp);
});

function updateDropLabel(input) {
    const n = input.files.length;
    document.getElementById('dropLabel').textContent =
        n === 0 ? '📂 Cliquer ou glisser-déposer vos fichiers CSV ici'
                : `✅ ${n} fichier${n>1?'s':''} sélectionné${n>1?'s':''}`;
}
</script>

<?php layoutFoot(); ?>
