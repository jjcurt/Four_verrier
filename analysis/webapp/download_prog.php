<?php
// Téléchargement sécurisé d'un fichier programme JSON
require_once __DIR__ . '/config.php';

$file = basename($_GET['file'] ?? '');
if (!preg_match('/^[\w\-]+\.json$/i', $file)) {
    http_response_code(400); exit('Fichier invalide.');
}

$path = PROGRAMS_DIR . '/' . $file;
if (!file_exists($path)) {
    http_response_code(404); exit('Fichier introuvable.');
}

header('Content-Type: application/json; charset=utf-8');
header('Content-Disposition: attachment; filename="' . $file . '"');
readfile($path);
