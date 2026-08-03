"use client";

import type { PriceResponse } from "@/lib/api";

function fmt(n: number, digits = 6): string {
  if (!Number.isFinite(n)) return "—";
  return n.toLocaleString(undefined, {
    maximumFractionDigits: digits,
    minimumFractionDigits: 2,
  });
}

type GreeksPanelProps = {
  spot: PriceResponse | null;
  loading?: boolean;
};

export default function GreeksPanel({ spot, loading }: GreeksPanelProps) {
  const rows = [
    ["Price", spot?.price],
    ["Delta", spot?.delta],
    ["Gamma", spot?.gamma],
    ["Vega", spot?.vega],
    ["Theta", spot?.theta],
  ] as const;

  return (
    <section aria-live="polite">
      <div className="flex items-baseline justify-between gap-3">
        <h2 className="font-mono text-xs tracking-[0.18em] text-muted uppercase">
          Live Greeks
        </h2>
        {loading && (
          <span className="font-mono text-[11px] tracking-wide text-muted">
            Updating…
          </span>
        )}
      </div>

      <dl className="mt-4 grid grid-cols-2 gap-x-6 gap-y-4 sm:grid-cols-3 lg:grid-cols-5">
        {rows.map(([label, value]) => (
          <div key={label}>
            <dt className="font-mono text-[11px] tracking-wide text-muted uppercase">
              {label}
            </dt>
            <dd
              className={`mt-1 font-mono text-lg tabular-nums ${
                label === "Price" ? "text-accent" : "text-ink"
              }`}
            >
              {value == null ? "—" : fmt(value)}
            </dd>
          </div>
        ))}
      </dl>
    </section>
  );
}
