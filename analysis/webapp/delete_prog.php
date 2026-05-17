<?php
// Suppression sécurisée d'un fichier programme JSON
require_once __DIR__ . '/config.php';

header('Content-Type: application/json; charset=utf-8');

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    http_response_code(405); echo '{"error":"method"}'; exit;
}

$file = basename($_POST['file'] ?? '');
if (!preg_match('/^[\w\-]+\.json$/i', $file)) {
    http_response_code(400); echo '{"error":"nom de fichier invalide"}'; exit;
}

$path = PROGRAMS_DIR . '/' . $file;
if (!file_exists($path)) {
    http_response_code(404); echo '{"error":"fichier introuvable"}'; exit;
}

if (unlink($path)) {
    echo json_encode(['ok' => true, 'deleted' => $file]);
} else {
    http_response_code(500);
    echo json_encode(['error' => 'Suppression impossible (permissions ?)']);
}
