#!/usr/bin/env python3
"""
analyze_log.py — Analyse et visualisation des logs de maintenance du Four Verrier

Usage:
    python scripts/analyze_log.py <fichier.csv>
    python scripts/analyze_log.py <fichier.csv> --save    # sauvegarde le graphique en PNG
    python scripts/analyze_log.py <fichier.csv> --stats   # affiche les statistiques détaillées

Dépendances : pip install pandas matplotlib
"""

import sys
import argparse
from pathlib import Path

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.ticker import FuncFormatter


# ---------------------------------------------------------------------------
# Couleurs par phase
# ---------------------------------------------------------------------------
PHASE_COLORS = {
    "RAMP":       "#FF8C00",   # orange
    "HOLD":       "#2196F3",   # bleu
    "STABILIZING":"#9C27B0",   # violet
    "IDLE":       "#9E9E9E",   # gris
}
PHASE_ALPHA = 0.12


def load_csv(path: Path) -> pd.DataFrame:
    df = pd.read_csv(path)
    df.columns = df.columns.str.strip()
    df["Timestamp"] = pd.to_datetime(df["Timestamp"])
    df["ElapsedMin"] = df["ElapsedSeconds"] / 60.0
    return df


def fmt_elapsed(x, _):
    """Formatteur axe X : MM:SS."""
    m, s = divmod(int(x * 60), 60)
    return f"{m}:{s:02d}"


def add_phase_bands(ax, df):
    """Colorie le fond selon la phase active."""
    phase_changes = df["Phase"].ne(df["Phase"].shift()).cumsum()
    for _, group in df.groupby(phase_changes):
        phase = group["Phase"].iloc[0]
        color = PHASE_COLORS.get(phase, "#CCCCCC")
        ax.axvspan(group["ElapsedMin"].iloc[0], group["ElapsedMin"].iloc[-1],
                   color=color, alpha=PHASE_ALPHA, linewidth=0)


def print_stats(df):
    """Statistiques de la cuisson."""
    print("\n" + "=" * 60)
    print(f"  Fichier  : {df.attrs.get('source', '?')}")
    print(f"  Durée    : {df['ElapsedSeconds'].max():.0f} s  "
          f"({df['ElapsedSeconds'].max()/60:.1f} min)")
    print(f"  Temp max : {df['CurrentTemp'].max():.2f} °C "
          f"(à t={df.loc[df['CurrentTemp'].idxmax(), 'ElapsedSeconds']:.0f} s)")
    print(f"  Temp min : {df['CurrentTemp'].min():.2f} °C")

    # Statistiques par phase
    print()
    for phase, grp in df.groupby("Phase"):
        color = ""
        print(f"  Phase {phase:12s}  "
              f"durée={grp['ElapsedSeconds'].iloc[-1]-grp['ElapsedSeconds'].iloc[0]:.0f} s  "
              f"T=[{grp['CurrentTemp'].min():.1f}..{grp['CurrentTemp'].max():.1f}] °C")

    # Dépassement en HOLD
    hold = df[df["Phase"] == "HOLD"]
    if not hold.empty:
        targets = hold["TargetTemp"].unique()
        for tgt in targets:
            hold_t = hold[hold["TargetTemp"] == tgt]
            overshoot = hold_t["CurrentTemp"].max() - tgt
            print(f"\n  Cible HOLD {tgt:.0f} °C :")
            print(f"    Dépassement max : +{overshoot:.2f} °C")
            # Temps de stabilisation : premier point < 1 °C d'erreur
            stable = hold_t[abs(hold_t["CurrentTemp"] - tgt) < 1.0]
            if not stable.empty:
                t0 = hold_t["ElapsedSeconds"].iloc[0]
                ts = stable["ElapsedSeconds"].iloc[0]
                print(f"    Temps de stabilisation (±1 °C) : {ts - t0:.0f} s")

    # Énergie approximative (SSR intégration)
    dt = df["ElapsedSeconds"].diff().fillna(0)
    energy_pct_s = (df["SSR2_PWM"] / 1023.0 * dt).sum()
    print(f"\n  Énergie relative (∫PWM dt) : {energy_pct_s:.0f} %·s  "
          f"({energy_pct_s/df['ElapsedSeconds'].max()*100:.1f}% moyen)")
    print("=" * 60 + "\n")


def plot(df, save_path=None):
    source = df.attrs.get("source", "log")
    fig, axes = plt.subplots(3, 1, figsize=(14, 10), sharex=True)
    fig.suptitle(f"Analyse de cuisson — {source}", fontsize=13, fontweight="bold")

    t = df["ElapsedMin"]

    # ------------------------------------------------------------------
    # 1. Températures
    # ------------------------------------------------------------------
    ax1 = axes[0]
    add_phase_bands(ax1, df)
    ax1.plot(t, df["TargetTemp"],  color="#E53935", linestyle="--", linewidth=1.2,
             label="Cible", alpha=0.8)
    ax1.plot(t, df["FilteredTemp"], color="#43A047", linewidth=1.5,
             label="Filtrée (EMA)")
    ax1.plot(t, df["CurrentTemp"],  color="#1E88E5", linewidth=1.0, alpha=0.6,
             label="Brute")
    ax1.set_ylabel("Température (°C)")
    ax1.legend(loc="upper left", fontsize=9)
    ax1.grid(True, alpha=0.3)

    # Annotations des changements de phase
    phase_changes = df["Phase"].ne(df["Phase"].shift())
    for idx in df[phase_changes].index:
        row = df.loc[idx]
        ax1.axvline(x=row["ElapsedMin"], color="gray", linewidth=0.8, alpha=0.5)
        ax1.annotate(row["Phase"],
                     xy=(row["ElapsedMin"], ax1.get_ylim()[0] if idx > 0 else row["CurrentTemp"]),
                     xytext=(row["ElapsedMin"] + 0.05, ax1.get_ylim()[0]),
                     fontsize=7, color=PHASE_COLORS.get(row["Phase"], "gray"),
                     rotation=90, va="bottom")

    # ------------------------------------------------------------------
    # 2. Puissance SSR
    # ------------------------------------------------------------------
    ax2 = axes[1]
    add_phase_bands(ax2, df)
    ax2.fill_between(t, df["SSR2_PWM"] / 1023.0 * 100,
                     color="#FF8F00", alpha=0.6, label="SSR2 PWM")
    ax2.plot(t, df["SSR2_PWM"] / 1023.0 * 100,
             color="#E65100", linewidth=1.0)
    ax2.set_ylabel("Puissance SSR (%)")
    ax2.set_ylim(0, 105)
    ax2.legend(loc="upper right", fontsize=9)
    ax2.grid(True, alpha=0.3)

    # ------------------------------------------------------------------
    # 3. Termes PID
    # ------------------------------------------------------------------
    ax3 = axes[2]
    add_phase_bands(ax3, df)
    ax3.plot(t, df["Pterm"],     color="#1565C0", linewidth=1.2, label="P")
    ax3.plot(t, df["Iterm"],     color="#2E7D32", linewidth=1.2, label="I")
    ax3.plot(t, df["Dterm"],     color="#C62828", linewidth=1.2, label="D")
    ax3.plot(t, df["PidOutput"], color="#6A1B9A", linewidth=1.5, linestyle="--",
             label="Sortie PID", alpha=0.8)
    ax3.axhline(0, color="black", linewidth=0.5, alpha=0.4)
    ax3.set_ylabel("Termes PID")
    ax3.set_xlabel("Temps (MM:SS)")
    ax3.legend(loc="upper right", fontsize=9)
    ax3.grid(True, alpha=0.3)

    # Formatteur axe X
    for ax in axes:
        ax.xaxis.set_major_formatter(FuncFormatter(fmt_elapsed))

    # Légende des phases (en bas)
    phase_patches = [mpatches.Patch(color=c, alpha=0.5, label=p)
                     for p, c in PHASE_COLORS.items() if p in df["Phase"].values]
    fig.legend(handles=phase_patches, loc="lower center", ncol=len(phase_patches),
               fontsize=9, title="Phases", framealpha=0.8,
               bbox_to_anchor=(0.5, 0.0))

    plt.tight_layout(rect=[0, 0.04, 1, 1])

    if save_path:
        fig.savefig(save_path, dpi=150, bbox_inches="tight")
        print(f"Graphique sauvegardé : {save_path}")
    else:
        plt.show()


def main():
    parser = argparse.ArgumentParser(description="Analyse de logs Four Verrier")
    parser.add_argument("csv", help="Fichier CSV de log (maint_*.csv ou test_*.csv)")
    parser.add_argument("--save", action="store_true",
                        help="Sauvegarder en PNG à côté du CSV")
    parser.add_argument("--stats", action="store_true",
                        help="Afficher les statistiques détaillées")
    args = parser.parse_args()

    path = Path(args.csv)
    if not path.exists():
        print(f"Erreur : fichier introuvable : {path}", file=sys.stderr)
        sys.exit(1)

    df = load_csv(path)
    df.attrs["source"] = path.stem

    if args.stats or True:   # toujours afficher un résumé court
        print_stats(df)

    save_path = path.with_suffix(".png") if args.save else None
    plot(df, save_path)


if __name__ == "__main__":
    main()
