<?php
// ---------------------------------------------------------------------------
// api.php — Endpoints JSON (utilisés par les pages JS/Fetch)
// ---------------------------------------------------------------------------
// GET /api.php?action=sessions[&type=...]   → liste sessions
// GET /api.php?action=chart&id=N            → données graphe (columnar)
// GET /api.php?action=photos&id=N           → photos d'une session
// ---------------------------------------------------------------------------

require_once __DIR__ . '/config.php';

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');

$action = $_GET['action'] ?? '';

try {
    switch ($action) {

        // ── Liste des sessions ──────────────────────────────────────────────
        case 'sessions':
            $type  = $_GET['type'] ?? '';
            $where = $type !== '' ? 'WHERE s.firing_type = ' . db()->quote($type) : '';
            $rows  = db()->query("
                SELECT s.id, s.program_name, s.firing_type, s.log_type,
                       s.start_datetime, s.firmware_version, s.kp, s.ki, s.kd,
                       COUNT(DISTINCT d.id)          AS nb_points,
                       MAX(d.elapsed_sec) / 60.0     AS duration_min,
                       COUNT(DISTINCT p.id)          AS nb_photos,
                       MIN(p.filename)               AS first_photo
                FROM sessions s
                LEFT JOIN datapoints d ON d.session_id = s.id
                LEFT JOIN photos     p ON p.session_id = s.id
                $where
                GROUP BY s.id
                ORDER BY s.start_datetime DESC
            ")->fetchAll();
            echo json_encode($rows);
            break;

        // ── Données graphe d'une session ────────────────────────────────────
        case 'chart':
            $id   = (int)($_GET['id'] ?? 0);
            $meta = db()->query("SELECT * FROM sessions WHERE id = $id")->fetch();
            if (!$meta) { http_response_code(404); echo '{"error":"not found"}'; exit; }

            $stmt = db()->query("
                SELECT elapsed_sec, current_temp, target_temp, raw_temp,
                       filtered_temp, ssr2_pwm, phase
                FROM datapoints
                WHERE session_id = $id
                ORDER BY elapsed_sec
            ");

            $elapsed = $temp = $target = $raw = $filtered = $pwm = $phase = [];
            while ($row = $stmt->fetch()) {
                $elapsed[]  = round($row['elapsed_sec'] / 60, 3);
                $temp[]     = $row['current_temp'];
                $target[]   = $row['target_temp'];
                $raw[]      = $row['raw_temp'];
                $filtered[] = $row['filtered_temp'];
                $pwm[]      = $row['ssr2_pwm'] !== null
                                ? round($row['ssr2_pwm'] / 1023 * 100, 1)
                                : null;
                $phase[]    = $row['phase'];
            }

            echo json_encode([
                'meta'     => $meta,
                'elapsed'  => $elapsed,
                'temp'     => $temp,
                'target'   => $target,
                'raw'      => $raw,
                'filtered' => $filtered,
                'pwm'      => $pwm,
                'phase'    => $phase,
            ]);
            break;

        // ── Données graphe comparaison multi-sessions ───────────────────────
        case 'chart_multi':
            $ids = array_map('intval', explode(',', $_GET['ids'] ?? ''));
            $ids = array_filter($ids);
            if (empty($ids)) { echo '[]'; exit; }

            $series = [];
            foreach ($ids as $id) {
                $meta = db()->query("SELECT * FROM sessions WHERE id=$id")->fetch();
                if (!$meta) continue;

                $stmt = db()->query("
                    SELECT elapsed_sec, current_temp, target_temp, filtered_temp
                    FROM datapoints WHERE session_id=$id ORDER BY elapsed_sec
                ");
                $elapsed = $temp = $target = $filtered = [];
                while ($row = $stmt->fetch()) {
                    $elapsed[]  = round($row['elapsed_sec'] / 60, 3);
                    $temp[]     = $row['current_temp'];
                    $target[]   = $row['target_temp'];
                    $filtered[] = $row['filtered_temp'];
                }
                $series[] = [
                    'id'      => $id,
                    'label'   => ($meta['program_name'] ?? '?') . ' — ' . substr($meta['start_datetime'] ?? '', 0, 10),
                    'elapsed' => $elapsed,
                    'temp'    => $filtered ?: $temp,
                    'target'  => $target,
                ];
            }
            echo json_encode($series);
            break;

        // ── Photos d'une session ────────────────────────────────────────────
        case 'photos':
            $id   = (int)($_GET['id'] ?? 0);
            $rows = db()->query("
                SELECT filename, caption FROM photos WHERE session_id=$id ORDER BY id
            ")->fetchAll();
            foreach ($rows as &$r) {
                $r['url'] = PHOTOS_URL . '/session_' . $id . '/' . rawurlencode($r['filename']);
            }
            echo json_encode($rows);
            break;

        default:
            http_response_code(400);
            echo '{"error":"unknown action"}';
    }
} catch (Exception $e) {
    http_response_code(500);
    echo json_encode(['error' => $e->getMessage()]);
}
