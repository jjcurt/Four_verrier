<?php
// ---------------------------------------------------------------------------
// config.php — Constantes et fonctions partagées
// ---------------------------------------------------------------------------

define('DB_PATH',           '/home/jcurt/Développements/Four/Four_verrier/analysis/db/kiln.db');
define('PHOTOS_DIR',        '/home/jcurt/Développements/Four/Four_verrier/analysis/photos');
define('PHOTOS_URL',        '/photos');   // location NGINX → alias vers PHOTOS_DIR
define('PROGRAMS_DIR',      '/home/jcurt/Développements/Four/Four_verrier/data/programs');
define('FIRING_TYPES_JSON', '/home/jcurt/Développements/Four/Four_verrier/data/config/firing_types.json');
define('INGEST_PY',         '/home/jcurt/Développements/Four/Four_verrier/analysis/ingest.py');
define('LOGS_DIR',          '/home/jcurt/Développements/Four/Four_verrier/analysis/logs');
define('APP_TITLE',         'Four Verrier');

// Connexion PDO SQLite (singleton)
function db(): PDO {
    static $pdo = null;
    if ($pdo === null) {
        $pdo = new PDO('sqlite:' . DB_PATH, null, null, [
            PDO::ATTR_ERRMODE            => PDO::ERRMODE_EXCEPTION,
            PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
        ]);
        $pdo->exec('PRAGMA query_only = ON');   // lecture seule — ingest.py gère les écritures
        $pdo->exec('PRAGMA foreign_keys = ON');
    }
    return $pdo;
}

// Taxonomie types de cuisson (lecture firing_types.json)
function firingTypes(): array {
    static $types = null;
    if ($types === null) {
        $json  = @file_get_contents(FIRING_TYPES_JSON);
        $data  = $json ? json_decode($json, true) : [];
        $types = array_column($data['types'] ?? [], 'label', 'id');
    }
    return $types;
}

function firingLabel(string $id): string {
    if ($id === '') return '';
    $map = firingTypes();
    return $map[$id] ?? ucfirst(str_replace('_', ' ', $id));
}

// Couleur de badge par type
function firingColor(string $id): string {
    $palette = [
        'bouteilles'          => '#1565C0',
        'fusing_simple'       => '#2E7D32',
        'fusing_multicouche'  => '#6A1B9A',
        'fusing_incrustation' => '#AD1457',
        'postformage'         => '#E65100',
        'cuisson_moule'       => '#00695C',
    ];
    return $palette[$id] ?? '#555';
}

function h(string $s): string { return htmlspecialchars($s, ENT_QUOTES); }
